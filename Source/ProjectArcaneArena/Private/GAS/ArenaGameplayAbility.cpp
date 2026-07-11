#include "GAS/ArenaGameplayAbility.h"

// 构造项目 GameplayAbility 基类，统一使用按 Actor 实例化的技能状态。
UArenaGameplayAbility::UArenaGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}
