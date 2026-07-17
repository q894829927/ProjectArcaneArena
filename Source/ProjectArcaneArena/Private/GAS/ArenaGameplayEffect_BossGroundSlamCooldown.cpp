#include "GAS/ArenaGameplayEffect_BossGroundSlamCooldown.h"

#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

// GroundSlam 冷却使用 Duration GE，让现有 CommitAbility 自动应用和检查冷却。
UArenaGameplayEffect_BossGroundSlamCooldown::UArenaGameplayEffect_BossGroundSlamCooldown()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(5.0f));

	FInheritedTagContainer GrantedTags;
	GrantedTags.AddTag(ArenaGameplayTags::Cooldown_Enemy_Boss_GroundSlam);
	UTargetTagsGameplayEffectComponent* TargetTagsComponent =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComponent"));
	GEComponents.Add(TargetTagsComponent);
	TargetTagsComponent->SetAndApplyTargetTagChanges(GrantedTags);
}
