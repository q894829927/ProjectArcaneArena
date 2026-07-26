#include "GAS/ArenaGameplayAbility.h"

#include "AbilitySystemComponent.h"
#include "Core/ArenaGameState.h"
#include "GAS/ArenaGameplayTags.h"

// 构造项目 GameplayAbility 基类，统一使用按 Actor 实例化的技能状态。
UArenaGameplayAbility::UArenaGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

// 先执行 GAS 原生资格判断，再以复制阶段阻止 Intro 战斗技能并保留开发环境的服务器拒绝注入。
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

	const AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const AArenaGameState* ArenaGameState = AvatarActor && AvatarActor->GetWorld()
		? AvatarActor->GetWorld()->GetGameState<AArenaGameState>()
		: nullptr;
	if (ArenaGameState && ArenaGameState->GetGamePhase() == EArenaGamePhase::BossIntro)
	{
		const FGameplayTagContainer& AbilityAssetTags = GetAssetTags();
		const bool bIsPlayerActiveAbility =
			AbilityAssetTags.HasTagExact(ArenaGameplayTags::Ability_Type_PlayerActive);
		const bool bIsEnemyCombatAbility = AbilityAssetTags.HasTag(ArenaGameplayTags::Ability_Enemy);
		if (bIsPlayerActiveAbility || bIsEnemyCombatAbility)
		{
			return false;
		}
	}

	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	return !ASC || !ASC->IsOwnerActorAuthoritative()
		|| !ArenaAbilityNetworkDebug::ConsumeServerRejection(NetworkAbilityId);
}
