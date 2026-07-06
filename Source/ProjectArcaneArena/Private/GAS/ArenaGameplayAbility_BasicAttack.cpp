#include "GAS/ArenaGameplayAbility_BasicAttack.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffect.h"

UArenaGameplayAbility_BasicAttack::UArenaGameplayAbility_BasicAttack()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InputTag = ArenaGameplayTags::Ability_BasicAttack;
	DamageTypeTag = ArenaGameplayTags::Damage_Physical;

	SetAssetTags(FGameplayTagContainer(ArenaGameplayTags::Ability_BasicAttack));
	ActivationBlockedTags.AddTag(ArenaGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(ArenaGameplayTags::State_Stunned);
	ActivationBlockedTags.AddTag(ArenaGameplayTags::Cooldown_BasicAttack);
}

void UArenaGameplayAbility_BasicAttack::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	UWorld* World = AvatarActor->GetWorld();
	if (!World || !DamageEffectClass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FVector Start = AvatarActor->GetActorLocation();
	const FVector End = Start + AvatarActor->GetActorForwardVector() * AttackRange;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ArenaBasicAttack), false, AvatarActor);
	QueryParams.AddIgnoredActor(AvatarActor);

	TArray<FHitResult> HitResults;
	World->SweepMultiByChannel(
		HitResults,
		Start,
		End,
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(AttackRadius),
		QueryParams);

	AActor* BestTarget = nullptr;
	UAbilitySystemComponent* BestTargetASC = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();

	for (const FHitResult& HitResult : HitResults)
	{
		AActor* HitActor = HitResult.GetActor();
		if (!HitActor || HitActor == AvatarActor)
		{
			continue;
		}

		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
		if (!TargetASC)
		{
			continue;
		}

		if (ActorInfo->AbilitySystemComponent.IsValid() && TargetASC == ActorInfo->AbilitySystemComponent.Get())
		{
			continue;
		}

		if (TargetASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(Start, HitActor->GetActorLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestTarget = HitActor;
			BestTargetASC = TargetASC;
		}
	}

	if (BestTarget && BestTargetASC)
	{
		FGameplayEffectSpecHandle DamageSpecHandle = MakeOutgoingGameplayEffectSpec(
			DamageEffectClass,
			GetAbilityLevel(Handle, ActorInfo));

		if (DamageSpecHandle.IsValid())
		{
			FGameplayEffectSpec* DamageSpec = DamageSpecHandle.Data.Get();
			DamageSpec->SetSetByCallerMagnitude(ArenaGameplayTags::SetByCaller_Damage_Base, BaseDamage);
			DamageSpec->SetSetByCallerMagnitude(ArenaGameplayTags::SetByCaller_Damage_SkillMultiplier, SkillMultiplier);

			if (DamageTypeTag.IsValid())
			{
				DamageSpec->AddDynamicAssetTag(DamageTypeTag);
			}

			BestTargetASC->ApplyGameplayEffectSpecToSelf(*DamageSpec);
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
