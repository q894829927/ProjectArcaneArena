#include "GAS/ArenaGameplayEffect_BossSummonMinionsCooldown.h"

#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

namespace ArenaBossSummonMinionsCooldown
{
	// 蓝图 GE 可以覆盖该值；原生常量保证资产缺失时仍有稳定的冷却防线。
	constexpr float DefaultDurationSeconds = 14.0f;
}

// 召唤冷却使用标准 Duration GE，使 CommitAbility 和 BT Decorator 共享同一资格结果。
UArenaGameplayEffect_BossSummonMinionsCooldown::UArenaGameplayEffect_BossSummonMinionsCooldown()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(
		FScalableFloat(ArenaBossSummonMinionsCooldown::DefaultDurationSeconds));

	FInheritedTagContainer GrantedTags;
	GrantedTags.AddTag(ArenaGameplayTags::Cooldown_Enemy_Boss_SummonMinions);
	UTargetTagsGameplayEffectComponent* TargetTagsComponent =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComponent"));
	GEComponents.Add(TargetTagsComponent);
	TargetTagsComponent->SetAndApplyTargetTagChanges(GrantedTags);
}
