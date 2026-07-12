#include "GAS/ArenaGameplayAbility_LightningStorm.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "Engine/World.h"
#include "GAS/ArenaAbilityNetworkDebug.h"
#include "GAS/ArenaGameplayTags.h"
#include "GAS/ArenaLightningStormArea.h"
#include "GAS/Targeting/ArenaTargetActor_MouseGround.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"

UArenaGameplayAbility_LightningStorm::UArenaGameplayAbility_LightningStorm()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	NetworkAbilityId = EArenaNetworkAbilityId::LightningStorm;
	InputTag = ArenaGameplayTags::Ability_LightningStorm;
	DamageTypeTag = ArenaGameplayTags::Damage_Lightning;
	StormAreaClass = AArenaLightningStormArea::StaticClass();
	TargetActorClass = AArenaTargetActor_MouseGround::StaticClass();

	SetAssetTags(FGameplayTagContainer(ArenaGameplayTags::Ability_LightningStorm));
	ActivationBlockedTags.AddTag(ArenaGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(ArenaGameplayTags::State_Stunned);
	ActivationBlockedTags.AddTag(ArenaGameplayTags::Cooldown_LightningStorm);
}

void UArenaGameplayAbility_LightningStorm::ActivateAbility(
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

	if (!StormAreaClass || !TargetActorClass || !DamageEffectClass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilityTask_WaitTargetData* TargetDataTask = UAbilityTask_WaitTargetData::WaitTargetData(
		this,
		FName(TEXT("LightningStormTargetData")),
		EGameplayTargetingConfirmation::Instant,
		TargetActorClass);

	if (!TargetDataTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveTargetDataTask = TargetDataTask;
	TargetDataTask->ValidData.AddDynamic(this, &UArenaGameplayAbility_LightningStorm::OnTargetDataReady);
	TargetDataTask->Cancelled.AddDynamic(this, &UArenaGameplayAbility_LightningStorm::OnTargetDataCancelled);
	// 先激活 TargetData Task，让本地鼠标点通过 GAS 标准预测/RPC 路径传到服务端。
	TargetDataTask->ReadyForActivation();

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

void UArenaGameplayAbility_LightningStorm::OnTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData)
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

	// 两端使用相同目标点做预测 Commit；服务端仍独占 Area Actor 和周期伤害。
	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	UAbilitySystemComponent* SourceASC = ActorInfo->AbilitySystemComponent.Get();

	FVector TargetLocation = FVector::ZeroVector;
	if (!ExtractTargetLocation(TargetData, TargetLocation))
	{
		TargetLocation = AvatarActor->GetActorLocation() + AvatarActor->GetActorForwardVector() * MaxTargetRange;
	}

	FTransform SpawnTransform;
	if (!BuildStormSpawnTransform(AvatarActor, TargetLocation, SpawnTransform))
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
		SpawnLightningStormArea(AvatarActor, SourceASC, SpawnTransform);
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UArenaGameplayAbility_LightningStorm::OnTargetDataCancelled(const FGameplayAbilityTargetDataHandle& TargetData)
{
	ActiveTargetDataTask = nullptr;

	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
}

bool UArenaGameplayAbility_LightningStorm::ExtractTargetLocation(
	const FGameplayAbilityTargetDataHandle& TargetData,
	FVector& OutTargetLocation) const
{
	const FGameplayAbilityTargetData* FirstTargetData = TargetData.Get(0);
	if (!FirstTargetData || !FirstTargetData->HasEndPoint())
	{
		return false;
	}

	OutTargetLocation = FirstTargetData->GetEndPoint();
	return !OutTargetLocation.ContainsNaN();
}

bool UArenaGameplayAbility_LightningStorm::BuildStormSpawnTransform(
	AActor* AvatarActor,
	const FVector& TargetLocation,
	FTransform& OutSpawnTransform) const
{
	if (!AvatarActor)
	{
		return false;
	}

	const FVector AvatarLocation = AvatarActor->GetActorLocation();
	FVector HorizontalDelta = TargetLocation - AvatarLocation;
	HorizontalDelta.Z = 0.0f;

	FVector ClampedLocation = TargetLocation;
	const float HorizontalDistance = HorizontalDelta.Size();
	if (HorizontalDistance > MaxTargetRange && HorizontalDistance > KINDA_SMALL_NUMBER)
	{
		// 服务端限制客户端提交的鼠标点，避免超出技能最大施法距离。
		const FVector ClampedHorizontal = HorizontalDelta / HorizontalDistance * MaxTargetRange;
		ClampedLocation = AvatarLocation + ClampedHorizontal;
		ClampedLocation.Z = TargetLocation.Z;
	}
	else if (HorizontalDistance <= KINDA_SMALL_NUMBER)
	{
		const FVector ForwardDirection = AvatarActor->GetActorForwardVector().GetSafeNormal2D();
		if (ForwardDirection.IsNearlyZero())
		{
			return false;
		}

		ClampedLocation = AvatarLocation + ForwardDirection * FMath::Min(MaxTargetRange, StormRadius);
		ClampedLocation.Z = TargetLocation.Z;
	}

	OutSpawnTransform = FTransform(FRotator::ZeroRotator, ClampedLocation);
	return true;
}

void UArenaGameplayAbility_LightningStorm::SpawnLightningStormArea(
	AActor* AvatarActor,
	UAbilitySystemComponent* SourceASC,
	const FTransform& SpawnTransform) const
{
	if (!AvatarActor || !SourceASC || !StormAreaClass || !DamageEffectClass)
	{
		return;
	}

	UWorld* World = AvatarActor->GetWorld();
	if (!World)
	{
		return;
	}

	APawn* InstigatorPawn = Cast<APawn>(AvatarActor);
	AArenaLightningStormArea* LightningStormArea = World->SpawnActorDeferred<AArenaLightningStormArea>(
		StormAreaClass,
		SpawnTransform,
		AvatarActor,
		InstigatorPawn,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (!LightningStormArea)
	{
		return;
	}

	LightningStormArea->InitializeStorm(
		SourceASC,
		AvatarActor,
		DamageEffectClass,
		DamageTypeTag,
		BaseDamage,
		SkillMultiplier,
		StormRadius,
		StormDuration,
		DamageTickInterval);
	LightningStormArea->FinishSpawning(SpawnTransform);

	FGameplayCueParameters CueParameters;
	CueParameters.Instigator = AvatarActor;
	CueParameters.EffectCauser = LightningStormArea;
	CueParameters.Location = SpawnTransform.GetLocation();
	SourceASC->ExecuteGameplayCue(ArenaGameplayTags::GameplayCue_Ability_LightningStorm_Cast, CueParameters);

	if (ArenaAbilityNetworkDebug::IsAuditEnabled())
	{
		UE_LOG(LogArenaAbilityNet, Log, TEXT("[%llu] LightningStorm Key=%d Handle=%s Area=%s"),
			ArenaAbilityNetworkDebug::NextServerExecutionSequence(),
			GetCurrentActivationInfo().GetActivationPredictionKey().Current,
			*GetCurrentAbilitySpecHandle().ToString(),
			*GetNameSafe(LightningStormArea));
	}
}
