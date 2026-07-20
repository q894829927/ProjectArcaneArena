#include "GAS/ArenaGameplayEffect_BossChargeCooldown.h"

#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

// Charge 冷却使用 Duration GE，使 CommitAbility 统一应用并由 GAS 检查冷却标签。
UArenaGameplayEffect_BossChargeCooldown::UArenaGameplayEffect_BossChargeCooldown()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(6.0f));

	FInheritedTagContainer GrantedTags;
	GrantedTags.AddTag(ArenaGameplayTags::Cooldown_Enemy_Boss_Charge);
	UTargetTagsGameplayEffectComponent* TargetTagsComponent =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComponent"));
	GEComponents.Add(TargetTagsComponent);
	TargetTagsComponent->SetAndApplyTargetTagChanges(GrantedTags);
}
