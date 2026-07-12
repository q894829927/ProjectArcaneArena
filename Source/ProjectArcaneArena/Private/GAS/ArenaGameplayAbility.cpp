#include "GAS/ArenaGameplayAbility.h"

#include "AbilitySystemComponent.h"

// 构造项目 GameplayAbility 基类，统一使用按 Actor 实例化的技能状态。
UArenaGameplayAbility::UArenaGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

bool UArenaGameplayAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	return !ASC || !ASC->IsOwnerActorAuthoritative()
		|| !ArenaAbilityNetworkDebug::ConsumeServerRejection(NetworkAbilityId);
}
