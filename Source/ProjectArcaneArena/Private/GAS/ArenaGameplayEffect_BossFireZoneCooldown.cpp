#include "GAS/ArenaGameplayEffect_BossFireZoneCooldown.h"

#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

namespace ArenaBossFireZoneCooldown
{
	// 蓝图 GE 可以覆盖该值；原生常量仅提供资产缺失时仍可工作的安全默认值。
	constexpr float DefaultDurationSeconds = 8.0f;
}

// FireZone 冷却使用可由蓝图覆盖的 Duration GE，使 CommitAbility 统一应用并由 GAS 检查冷却标签。
UArenaGameplayEffect_BossFireZoneCooldown::UArenaGameplayEffect_BossFireZoneCooldown()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(
		FScalableFloat(ArenaBossFireZoneCooldown::DefaultDurationSeconds));

	FInheritedTagContainer GrantedTags;
	GrantedTags.AddTag(ArenaGameplayTags::Cooldown_Enemy_Boss_FireZone);
	UTargetTagsGameplayEffectComponent* TargetTagsComponent =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComponent"));
	GEComponents.Add(TargetTagsComponent);
	TargetTagsComponent->SetAndApplyTargetTagChanges(GrantedTags);
}
