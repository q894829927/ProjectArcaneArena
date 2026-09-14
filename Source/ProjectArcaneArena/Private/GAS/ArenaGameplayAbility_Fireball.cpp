#include "GAS/ArenaGameplayAbility_Fireball.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "Core/ArenaPlayerState.h"
#include "Engine/World.h"
#include "GAS/ArenaAbilityNetworkDebug.h"
#include "GAS/ArenaGameplayEffect_Burning.h"
#include "GAS/ArenaGameplayTags.h"
#include "GAS/Targeting/ArenaTargetActor_MouseGround.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "Projectile/ArenaFireballProjectile.h"

DEFINE_LOG_CATEGORY_STATIC(LogArenaFireBuild, Log, All);

// 构造火球技能，配置预测输入、火焰伤害类型和目标选择/投射物类型。
UArenaGameplayAbility_Fireball::UArenaGameplayAbility_Fireball()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	NetworkAbilityId = EArenaNetworkAbilityId::Fireball;
	InputTag = ArenaGameplayTags::Ability_Fireball;
	DamageTypeTag = ArenaGameplayTags::Damage_Fire;
	BurningEffectClass = UArenaGameplayEffect_Burning::StaticClass();
	ProjectileClass = AArenaFireballProjectile::StaticClass();
	TargetActorClass = AArenaTargetActor_MouseGround::StaticClass();

	// 火球属于消耗 Energy 的玩家主动技能，供统一施放事件和触发升级筛选。
	FGameplayTagContainer AbilityAssetTags(ArenaGameplayTags::Ability_Fireball);
	AbilityAssetTags.AddTag(ArenaGameplayTags::Ability_Type_PlayerActive);
	AbilityAssetTags.AddTag(ArenaGameplayTags::Ability_Type_EnergySkill);
	SetAssetTags(AbilityAssetTags);
	ActivationBlockedTags.AddTag(ArenaGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(ArenaGameplayTags::State_Stunned);
	ActivationBlockedTags.AddTag(ArenaGameplayTags::Cooldown_Fireball);
}

// 激活火球技能，启动鼠标目标数据采集并等待客户端/服务端 TargetData 流程。
void UArenaGameplayAbility_Fireball::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bConsumedTargetData = false;
	bServerSpawnConsumed = false;

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

// 收到目标数据后两端提交预测消耗/冷却，只有服务端消费一次生成权威火球投射物。
void UArenaGameplayAbility_Fireball::OnTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData)
{
	ActiveTargetDataTask = nullptr;
	if (bConsumedTargetData)
	{
		return;
	}
	bConsumedTargetData = true;

	const FGameplayAbilitySpecHandle Handle = GetCurrentAbilitySpecHandle();
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const FGameplayAbilityActivationInfo ActivationInfo = GetCurrentActivationInfo();

	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid() || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 两端使用相同目标点做预测 Commit；服务端仍独占最终生成和伤害。
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

	if (ActorInfo->IsNetAuthority() && !bServerSpawnConsumed)
	{
		bServerSpawnConsumed = true;
		SpawnFireballProjectile(AvatarActor, SourceASC, SpawnTransform);
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

// 目标选择取消时清理 TargetData task，并取消当前技能。
void UArenaGameplayAbility_Fireball::OnTargetDataCancelled(const FGameplayAbilityTargetDataHandle& TargetData)
{
	ActiveTargetDataTask = nullptr;

	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
}

// 从 TargetData 中提取鼠标命中的世界位置。
bool UArenaGameplayAbility_Fireball::ExtractTargetLocation(const FGameplayAbilityTargetDataHandle& TargetData, FVector& OutTargetLocation) const
{
	const FGameplayAbilityTargetData* FirstTargetData = TargetData.Get(0);
	if (!FirstTargetData || !FirstTargetData->HasEndPoint())
	{
		return false;
	}

	OutTargetLocation = FirstTargetData->GetEndPoint();
	return !OutTargetLocation.ContainsNaN();
}

// 根据角色位置和目标点计算水平发射方向与生成变换。
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

// 服务端汇总 Fire 构筑升级后生成火球，并把快照化的直接伤害和 Burning 参数交给 projectile。
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

	const AArenaPlayerState* ArenaPlayerState = Cast<AArenaPlayerState>(SourceASC->GetOwnerActor());
	const float FireballDamageBonus = ArenaPlayerState
		? ArenaPlayerState->GetOwnedUpgradeNumericTotal(
			ArenaGameplayTags::Ability_Fireball,
			ArenaGameplayTags::Damage_Fire,
			ArenaGameplayTags::Upgrade_Fireball_Damage)
		: 0.0f;
	const float BurningDamagePerStack = ArenaPlayerState
		? ArenaPlayerState->GetOwnedUpgradeNumericTotal(
			ArenaGameplayTags::Ability_Fireball,
			ArenaGameplayTags::Damage_Fire,
			ArenaGameplayTags::Upgrade_Fireball_Burning)
		: 0.0f;
	const bool bBurningUnlocked = SourceASC->HasMatchingGameplayTag(ArenaGameplayTags::Upgrade_Fireball_Burning);
	const float UpgradedSkillMultiplier = SkillMultiplier * (1.0f + FMath::Max(FireballDamageBonus, 0.0f));

	if (bBurningUnlocked && (!BurningEffectClass || BurningDamagePerStack <= 0.0f))
	{
		UE_LOG(LogArenaFireBuild, Warning,
			TEXT("Fireball Burning is unlocked for %s but its effect or numeric upgrade value is invalid."),
			*GetNameSafe(SourceASC->GetOwnerActor()));
	}

	FireballProjectile->InitializeProjectile(
		SourceASC,
		AvatarActor,
		DamageEffectClass,
		DamageTypeTag,
		BaseDamage,
		UpgradedSkillMultiplier,
		BurningEffectClass,
		bBurningUnlocked,
		BurningDamagePerStack);
	FireballProjectile->FinishSpawning(SpawnTransform);

	FGameplayCueParameters CueParameters;
	CueParameters.Instigator = AvatarActor;
	CueParameters.EffectCauser = FireballProjectile;
	CueParameters.Location = SpawnTransform.GetLocation();
	SourceASC->ExecuteGameplayCue(ArenaGameplayTags::GameplayCue_Ability_Fireball_Cast, CueParameters);

	if (ArenaAbilityNetworkDebug::IsAuditEnabled())
	{
		UE_LOG(LogArenaAbilityNet, Log, TEXT("[%llu] Fireball Key=%d Handle=%s Projectile=%s"),
			ArenaAbilityNetworkDebug::NextServerExecutionSequence(),
			GetCurrentActivationInfo().GetActivationPredictionKey().Current,
			*GetCurrentAbilitySpecHandle().ToString(),
			*GetNameSafe(FireballProjectile));
	}
}
