#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class AActor;
class UAbilitySystemComponent;
struct FGameplayAbilitySpec;

namespace ArenaBossAI
{
	// 按精确 Ability AssetTag 查找唯一已授予 Spec；零个或多个匹配都会返回空指针。
	FGameplayAbilitySpec* FindUniqueAbilitySpecByTag(
		UAbilitySystemComponent* AbilitySystemComponent,
		const FGameplayTag& AbilityTag,
		int32& OutMatchCount);

	// 只接受拥有 ASC 且未进入 State.Dead 的有效 Actor，供 Boss Service 与节点统一目标规则。
	bool IsLivingCombatTarget(const AActor* Candidate);
}
