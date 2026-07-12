#include "GAS/ArenaGameplayAbility_Dash.h"

#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "AbilitySystemComponent.h"
#include "Components/SceneComponent.h"
#include "GAS/ArenaAbilityNetworkDebug.h"
#include "GAS/ArenaGameplayTags.h"
#include "GAS/Targeting/ArenaTargetActor_DashDirection.h"
#include "GAS/Targeting/ArenaTargetData_DashDirection.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "TimerManager.h"

UArenaGameplayAbility_Dash::UArenaGameplayAbility_Dash()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	NetworkAbilityId = EArenaNetworkAbilityId::Dash;
	InputTag = ArenaGameplayTags::Ability_Dash;
	DashDirectionTargetActorClass = AArenaTargetActor_DashDirection::StaticClass();

	SetAssetTags(FGameplayTagContainer(ArenaGameplayTags::Ability_Dash));
	ActivationBlockedTags.AddTag(ArenaGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(ArenaGameplayTags::State_Stunned);
	ActivationBlockedTags.AddTag(ArenaGameplayTags::State_Dashing);
	ActivationBlockedTags.AddTag(ArenaGameplayTags::Cooldown_Dash);
}

void UArenaGameplayAbility_Dash::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid() || !ActorInfo->AbilitySystemComponent.IsValid()
		|| !Cast<ACharacter>(ActorInfo->AvatarActor.Get()) || !DashDirectionTargetActorClass
		|| DashDistance <= 0.0f || DashDuration <= KINDA_SMALL_NUMBER)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bConsumedTargetData = false;
	UAbilityTask_WaitTargetData* TargetDataTask = UAbilityTask_WaitTargetData::WaitTargetData(
		this,
		FName(TEXT("DashDirectionTargetData")),
		EGameplayTargetingConfirmation::Instant,
		DashDirectionTargetActorClass);
	if (!TargetDataTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveTargetDataTask = TargetDataTask;
	TargetDataTask->ValidData.AddDynamic(this, &UArenaGameplayAbility_Dash::OnDashTargetDataReady);
	TargetDataTask->Cancelled.AddDynamic(this, &UArenaGameplayAbility_Dash::OnDashTargetDataCancelled);
	TargetDataTask->ReadyForActivation();

	AGameplayAbilityTargetActor* SpawnedTargetActor = nullptr;
	if (TargetDataTask->BeginSpawningActor(this, DashDirectionTargetActorClass, SpawnedTargetActor))
	{
		TargetDataTask->FinishSpawningActor(this, SpawnedTargetActor);
	}
	else if (ActorInfo->IsLocallyControlled())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

void UArenaGameplayAbility_Dash::OnDashTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData)
{
	ActiveTargetDataTask = nullptr;
	if (bConsumedTargetData)
	{
		return;
	}
	bConsumedTargetData = true;

	FVector DashDirection = FVector::ZeroVector;
	if (!ExtractAndValidateDashDirection(TargetData, DashDirection))
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!CommitAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo()))
	{
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
		return;
	}

	StartDashWithDirection(DashDirection);
}

void UArenaGameplayAbility_Dash::OnDashTargetDataCancelled(const FGameplayAbilityTargetDataHandle& TargetData)
{
	ActiveTargetDataTask = nullptr;
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
}

void UArenaGameplayAbility_Dash::StartDashWithDirection(const FVector& DashDirection)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!Character || !World || DashDirection.IsNearlyZero())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
		return;
	}

	ActiveDashCharacter = Character;
	ActiveDashASC = ActorInfo->AbilitySystemComponent.Get();
	ApplyDashStateTags(ActiveDashASC.Get());
	PlayDashMontage();

	if (ActorInfo->IsNetAuthority() && ArenaAbilityNetworkDebug::IsAuditEnabled())
	{
		UE_LOG(LogArenaAbilityNet, Log, TEXT("[%llu] Dash Key=%d Handle=%s Direction=%s"),
			ArenaAbilityNetworkDebug::NextServerExecutionSequence(),
			GetCurrentActivationInfo().GetActivationPredictionKey().Current,
			*GetCurrentAbilitySpecHandle().ToString(),
			*DashDirection.ToCompactString());
	}

	const float DashSpeed = DashDistance / DashDuration;
	UAbilityTask_ApplyRootMotionConstantForce* DashMovementTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this,
		FName(TEXT("DashMovement")),
		DashDirection,
		DashSpeed,
		DashDuration,
		false,
		nullptr,
		ERootMotionFinishVelocityMode::SetVelocity,
		FVector::ZeroVector,
		0.0f,
		false);
	if (!DashMovementTask)
	{
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
		return;
	}

	DashMovementTask->ReadyForActivation();
	World->GetTimerManager().SetTimer(DashTimerHandle, this, &UArenaGameplayAbility_Dash::FinishDash, DashDuration, false);
}

void UArenaGameplayAbility_Dash::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DashTimerHandle);
	}

	if (ActiveDashCharacter.IsValid())
	{
		StopDashMovement(ActiveDashCharacter.Get());
	}

	RemoveDashStateTags();
	ActiveTargetDataTask = nullptr;
	bConsumedTargetData = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UArenaGameplayAbility_Dash::ExtractAndValidateDashDirection(
	const FGameplayAbilityTargetDataHandle& TargetData,
	FVector& OutDirection) const
{
	if (TargetData.Num() != 1)
	{
		return false;
	}

	const FGameplayAbilityTargetData* RawData = TargetData.Get(0);
	if (!RawData || RawData->GetScriptStruct() != FGameplayAbilityTargetData_DashDirection::StaticStruct())
	{
		return false;
	}

	const FGameplayAbilityTargetData_DashDirection* DirectionData =
		static_cast<const FGameplayAbilityTargetData_DashDirection*>(RawData);
	const FVector SubmittedDirection = DirectionData->Direction;
	if (SubmittedDirection.ContainsNaN() || SubmittedDirection.SizeSquared() < 0.25f
		|| FMath::Abs(SubmittedDirection.Z) > 0.1f)
	{
		return false;
	}

	OutDirection = SubmittedDirection.GetSafeNormal2D();
	return !OutDirection.IsNearlyZero();
}

void UArenaGameplayAbility_Dash::PlayDashMontage()
{
	if (!DashMontage)
	{
		return;
	}

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		FName(TEXT("DashMontage")),
		DashMontage,
		DashMontagePlayRate,
		DashMontageStartSection,
		true,
		0.0f);
	if (MontageTask)
	{
		MontageTask->ReadyForActivation();
	}
}

void UArenaGameplayAbility_Dash::ApplyDashStateTags(UAbilitySystemComponent* ASC)
{
	if (!ASC || bAppliedDashStateTags)
	{
		return;
	}

	ASC->AddLooseGameplayTag(ArenaGameplayTags::State_Dashing);
	ASC->AddLooseGameplayTag(ArenaGameplayTags::State_Invincible);
	if (ASC->IsOwnerActorAuthoritative())
	{
		ASC->AddReplicatedLooseGameplayTag(ArenaGameplayTags::State_Dashing);
		ASC->AddReplicatedLooseGameplayTag(ArenaGameplayTags::State_Invincible);
		FGameplayCueParameters CueParameters;
		AActor* CueAvatar = ASC->GetAvatarActor();
		CueParameters.Instigator = CueAvatar;
		CueParameters.EffectCauser = CueAvatar;
		// 持续冲刺特效附着到角色根组件，避免继承 Manny Mesh 的导入旋转和相对位移。
		CueParameters.TargetAttachComponent = CueAvatar ? CueAvatar->GetRootComponent() : nullptr;
		ASC->AddGameplayCue(ArenaGameplayTags::GameplayCue_Ability_Dash_Active, CueParameters);
		bAddedDashGameplayCue = true;
		if (ArenaAbilityNetworkDebug::IsAuditEnabled())
		{
			UE_LOG(LogArenaAbilityNet, Log, TEXT("[%llu] DashCue Added Avatar=%s"),
				ArenaAbilityNetworkDebug::NextServerExecutionSequence(),
				*GetNameSafe(CueAvatar));
		}
	}
	bAppliedDashStateTags = true;
}

void UArenaGameplayAbility_Dash::RemoveDashStateTags()
{
	UAbilitySystemComponent* ASC = ActiveDashASC.Get();
	if (ASC && bAppliedDashStateTags)
	{
		ASC->RemoveLooseGameplayTag(ArenaGameplayTags::State_Dashing);
		ASC->RemoveLooseGameplayTag(ArenaGameplayTags::State_Invincible);
		if (ASC->IsOwnerActorAuthoritative())
		{
			ASC->RemoveReplicatedLooseGameplayTag(ArenaGameplayTags::State_Dashing);
			ASC->RemoveReplicatedLooseGameplayTag(ArenaGameplayTags::State_Invincible);
			if (bAddedDashGameplayCue)
			{
				ASC->RemoveGameplayCue(ArenaGameplayTags::GameplayCue_Ability_Dash_Active);
				if (ArenaAbilityNetworkDebug::IsAuditEnabled())
				{
					UE_LOG(LogArenaAbilityNet, Log, TEXT("[%llu] DashCue Removed Avatar=%s"),
						ArenaAbilityNetworkDebug::NextServerExecutionSequence(),
						*GetNameSafe(ASC->GetAvatarActor()));
				}
			}
		}
	}

	bAppliedDashStateTags = false;
	bAddedDashGameplayCue = false;
	ActiveDashASC.Reset();
	ActiveDashCharacter.Reset();
}

void UArenaGameplayAbility_Dash::FinishDash()
{
	if (ActiveDashCharacter.IsValid())
	{
		StopDashMovement(ActiveDashCharacter.Get());
	}

	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UArenaGameplayAbility_Dash::StopDashMovement(ACharacter* Character) const
{
	if (Character && Character->GetCharacterMovement())
	{
		Character->GetCharacterMovement()->StopMovementImmediately();
	}
}
