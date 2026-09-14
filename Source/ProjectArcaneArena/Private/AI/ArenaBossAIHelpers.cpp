#include "AI/ArenaBossAIHelpers.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GAS/ArenaGameplayTags.h"

// 按精确 AssetTag 扫描已授予 Ability，避免 BT Task 等待同标签层级下的错误 Spec。
FGameplayAbilitySpec* ArenaBossAI::FindUniqueAbilitySpecByTag(
	UAbilitySystemComponent* AbilitySystemComponent,
	const FGameplayTag& AbilityTag,
	int32& OutMatchCount)
{
	OutMatchCount = 0;
	FGameplayAbilitySpec* MatchingSpec = nullptr;
	if (!AbilitySystemComponent || !AbilityTag.IsValid())
	{
		return nullptr;
	}

	for (FGameplayAbilitySpec& AbilitySpec : AbilitySystemComponent->GetActivatableAbilities())
	{
		const UGameplayAbility* Ability = AbilitySpec.Ability;
		if (!Ability || !Ability->GetAssetTags().HasTagExact(AbilityTag))
		{
			continue;
		}

		++OutMatchCount;
		MatchingSpec = &AbilitySpec;
	}

	return OutMatchCount == 1 ? MatchingSpec : nullptr;
}

// 统一检查目标 Actor、ASC 和死亡标签，使 Service、Decorator 与 Task 不产生资格分叉。
bool ArenaBossAI::IsLivingCombatTarget(const AActor* Candidate)
{
	const UAbilitySystemComponent* TargetASC = Candidate
		? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<AActor*>(Candidate))
		: nullptr;
	return IsValid(Candidate)
		&& TargetASC
		&& !TargetASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead);
}
