#include "GAS/Targeting/ArenaTargetActor_DashDirection.h"

#include "Abilities/GameplayAbility.h"
#include "Character/ArenaPlayerCharacter.h"
#include "GAS/Targeting/ArenaTargetData_DashDirection.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

AArenaTargetActor_DashDirection::AArenaTargetActor_DashDirection()
{
	PrimaryActorTick.bCanEverTick = false;
	ShouldProduceTargetDataOnServer = false;
	bDestroyOnConfirmation = true;
}

void AArenaTargetActor_DashDirection::StartTargeting(UGameplayAbility* Ability)
{
	Super::StartTargeting(Ability);
	SourceActor = Ability ? Ability->GetAvatarActorFromActorInfo() : nullptr;
}

void AArenaTargetActor_DashDirection::ConfirmTargetingAndContinue()
{
	const FVector DashDirection = ResolveLocalDashDirection();
	if (DashDirection.IsNearlyZero() || DashDirection.ContainsNaN())
	{
		CanceledDelegate.Broadcast(FGameplayAbilityTargetDataHandle());
		return;
	}

	FGameplayAbilityTargetData_DashDirection* DirectionData = new FGameplayAbilityTargetData_DashDirection();
	DirectionData->Direction = FVector_NetQuantizeNormal(DashDirection);
	TargetDataReadyDelegate.Broadcast(FGameplayAbilityTargetDataHandle(DirectionData));
}

FVector AArenaTargetActor_DashDirection::ResolveLocalDashDirection() const
{
	const AActor* AvatarActor = SourceActor.Get();
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

	if (DashDirection.IsNearlyZero())
	{
		if (const AArenaPlayerCharacter* PlayerCharacter = Cast<AArenaPlayerCharacter>(AvatarActor))
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
