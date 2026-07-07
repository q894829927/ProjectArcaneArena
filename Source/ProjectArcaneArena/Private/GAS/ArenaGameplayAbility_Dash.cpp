#include "GAS/ArenaGameplayAbility_Dash.h"

#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "Character/ArenaPlayerCharacter.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "TimerManager.h"

UArenaGameplayAbility_Dash::UArenaGameplayAbility_Dash()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InputTag = ArenaGameplayTags::Ability_Dash;

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
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid() || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	if (!Character || DashDistance <= 0.0f || DashDuration <= KINDA_SMALL_NUMBER)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UWorld* World = Character->GetWorld();
	if (!World)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FVector DashDirection = ResolveDashDirection(Character);
	if (DashDirection.IsNearlyZero())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveDashCharacter = Character;
	ActiveDashASC = ActorInfo->AbilitySystemComponent.Get();
	ApplyDashStateTags(ActiveDashASC.Get());
	PlayDashMontage();

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
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	DashMovementTask->ReadyForActivation();

	World->GetTimerManager().SetTimer(
		DashTimerHandle,
		this,
		&UArenaGameplayAbility_Dash::FinishDash,
		DashDuration,
		false);
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

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

FVector UArenaGameplayAbility_Dash::ResolveDashDirection(AActor* AvatarActor) const
{
	if (!AvatarActor)
	{
		return FVector::ZeroVector;
	}

	FVector DashDirection = FVector::ZeroVector;
	if (const ACharacter* Character = Cast<ACharacter>(AvatarActor))
	{
		if (const UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement())
		{
			DashDirection = MovementComponent->GetCurrentAcceleration();
		}
	}

	if (const AArenaPlayerCharacter* PlayerCharacter = Cast<AArenaPlayerCharacter>(AvatarActor))
	{
		if (DashDirection.IsNearlyZero())
		{
			DashDirection = PlayerCharacter->GetLastMovementInputDirection();
		}
	}

	if (DashDirection.IsNearlyZero())
	{
		DashDirection = AvatarActor->GetActorForwardVector();
	}

	DashDirection.Z = 0.0f;
	return DashDirection.GetSafeNormal();
}

void UArenaGameplayAbility_Dash::PlayDashMontage()
{
	if (!DashMontage)
	{
		return;
	}

	// Dash montage is visual only; the movement task controls authoritative dash movement.
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		FName(TEXT("DashMontage")),
		DashMontage,
		DashMontagePlayRate,
		DashMontageStartSection,
		false,
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

	// Ability-owned state tags keep dash immunity scoped to the active dash window.
	ASC->AddLooseGameplayTag(ArenaGameplayTags::State_Dashing);
	ASC->AddLooseGameplayTag(ArenaGameplayTags::State_Invincible);
	ASC->AddReplicatedLooseGameplayTag(ArenaGameplayTags::State_Dashing);
	ASC->AddReplicatedLooseGameplayTag(ArenaGameplayTags::State_Invincible);
	bAppliedDashStateTags = true;
}

void UArenaGameplayAbility_Dash::RemoveDashStateTags()
{
	UAbilitySystemComponent* ASC = ActiveDashASC.Get();
	if (!ASC || !bAppliedDashStateTags)
	{
		return;
	}

	ASC->RemoveLooseGameplayTag(ArenaGameplayTags::State_Dashing);
	ASC->RemoveLooseGameplayTag(ArenaGameplayTags::State_Invincible);
	ASC->RemoveReplicatedLooseGameplayTag(ArenaGameplayTags::State_Dashing);
	ASC->RemoveReplicatedLooseGameplayTag(ArenaGameplayTags::State_Invincible);

	bAppliedDashStateTags = false;
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
	if (!Character)
	{
		return;
	}

	if (UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
	}
}
