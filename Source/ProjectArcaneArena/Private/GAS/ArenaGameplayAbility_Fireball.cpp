#include "GAS/ArenaGameplayAbility_Fireball.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "Engine/World.h"
#include "GAS/ArenaGameplayTags.h"
#include "GAS/Targeting/ArenaTargetActor_MouseGround.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "Projectile/ArenaFireballProjectile.h"

UArenaGameplayAbility_Fireball::UArenaGameplayAbility_Fireball()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InputTag = ArenaGameplayTags::Ability_Fireball;
	DamageTypeTag = ArenaGameplayTags::Damage_Fire;
	ProjectileClass = AArenaFireballProjectile::StaticClass();
	TargetActorClass = AArenaTargetActor_MouseGround::StaticClass();

	SetAssetTags(FGameplayTagContainer(ArenaGameplayTags::Ability_Fireball));
	ActivationBlockedTags.AddTag(ArenaGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(ArenaGameplayTags::State_Stunned);
	ActivationBlockedTags.AddTag(ArenaGameplayTags::Cooldown_Fireball);
}

void UArenaGameplayAbility_Fireball::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid() || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!ProjectileClass || !TargetActorClass || !DamageEffectClass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilityTask_WaitTargetData* TargetDataTask = UAbilityTask_WaitTargetData::WaitTargetData(
		this,
		FName(TEXT("FireballTargetData")),
		EGameplayTargetingConfirmation::Instant,
		TargetActorClass);

	if (!TargetDataTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveTargetDataTask = TargetDataTask;
	TargetDataTask->ValidData.AddDynamic(this, &UArenaGameplayAbility_Fireball::OnTargetDataReady);
	TargetDataTask->Cancelled.AddDynamic(this, &UArenaGameplayAbility_Fireball::OnTargetDataCancelled);
	// 先让 Task 进入 active 状态，Instant 确认时回调可以安全广播到 Ability。
	TargetDataTask->ReadyForActivation();

	// C++ 使用 WaitTargetData 时需要手动走 Begin/Finish，服务端远端实例会只注册等待客户端 TargetData。
	AGameplayAbilityTargetActor* SpawnedTargetActor = nullptr;
	const bool bSpawnedTargetActor = TargetDataTask->BeginSpawningActor(this, TargetActorClass, SpawnedTargetActor);
	if (bSpawnedTargetActor)
	{
		TargetDataTask->FinishSpawningActor(this, SpawnedTargetActor);
	}
	else if (ActorInfo->IsLocallyControlled())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
}

void UArenaGameplayAbility_Fireball::OnTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData)
{
	ActiveTargetDataTask = nullptr;

	const FGameplayAbilitySpecHandle Handle = GetCurrentAbilitySpecHandle();
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const FGameplayAbilityActivationInfo ActivationInfo = GetCurrentActivationInfo();

	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid() || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 客户端只负责发送鼠标目标点；服务端实例收到 TargetData 后才提交消耗/冷却和生成投射物。
	if (!ActorInfo->IsNetAuthority())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	UAbilitySystemComponent* SourceASC = ActorInfo->AbilitySystemComponent.Get();

	FVector TargetLocation = FVector::ZeroVector;
	if (!ExtractTargetLocation(TargetData, TargetLocation))
	{
		TargetLocation = AvatarActor->GetActorLocation() + AvatarActor->GetActorForwardVector() * MaxTargetRange;
	}

	FTransform SpawnTransform;
	if (!BuildProjectileSpawnTransform(AvatarActor, TargetLocation, SpawnTransform))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	SpawnFireballProjectile(AvatarActor, SourceASC, SpawnTransform);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UArenaGameplayAbility_Fireball::OnTargetDataCancelled(const FGameplayAbilityTargetDataHandle& TargetData)
{
	ActiveTargetDataTask = nullptr;

	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
}

bool UArenaGameplayAbility_Fireball::ExtractTargetLocation(const FGameplayAbilityTargetDataHandle& TargetData, FVector& OutTargetLocation) const
{
	const FGameplayAbilityTargetData* FirstTargetData = TargetData.Get(0);
	if (!FirstTargetData || !FirstTargetData->HasEndPoint())
	{
		return false;
	}

	OutTargetLocation = FirstTargetData->GetEndPoint();
	return true;
}

bool UArenaGameplayAbility_Fireball::BuildProjectileSpawnTransform(AActor* AvatarActor, const FVector& TargetLocation, FTransform& OutSpawnTransform) const
{
	if (!AvatarActor)
	{
		return false;
	}

	const FVector AvatarLocation = AvatarActor->GetActorLocation();
	FVector AimDelta = TargetLocation - AvatarLocation;
	AimDelta.Z = 0.0f;

	FVector FireDirection = AimDelta.GetSafeNormal();
	if (FireDirection.IsNearlyZero())
	{
		FireDirection = AvatarActor->GetActorForwardVector().GetSafeNormal();
	}

	if (FireDirection.IsNearlyZero())
	{
		return false;
	}

	// 只用水平朝向发射，避免鼠标点高低差让顶视角 projectile 往地面或天空钻。
	const FVector SpawnLocation = AvatarLocation
		+ FireDirection * SpawnForwardOffset
		+ FVector(0.0f, 0.0f, SpawnHeightOffset);
	OutSpawnTransform = FTransform(FireDirection.Rotation(), SpawnLocation);
	return true;
}

void UArenaGameplayAbility_Fireball::SpawnFireballProjectile(AActor* AvatarActor, UAbilitySystemComponent* SourceASC, const FTransform& SpawnTransform) const
{
	if (!AvatarActor || !SourceASC || !ProjectileClass || !DamageEffectClass)
	{
		return;
	}

	UWorld* World = AvatarActor->GetWorld();
	if (!World)
	{
		return;
	}

	APawn* InstigatorPawn = Cast<APawn>(AvatarActor);
	AArenaFireballProjectile* FireballProjectile = World->SpawnActorDeferred<AArenaFireballProjectile>(
		ProjectileClass,
		SpawnTransform,
		AvatarActor,
		InstigatorPawn,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (!FireballProjectile)
	{
		return;
	}

	FireballProjectile->InitializeProjectile(
		SourceASC,
		AvatarActor,
		DamageEffectClass,
		DamageTypeTag,
		BaseDamage,
		SkillMultiplier);
	FireballProjectile->FinishSpawning(SpawnTransform);
}
