#include "GAS/ArenaGameplayEffect_EnemyRangedCooldown.h"

#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

// 配置远程攻击默认冷却时长与 Granted Tag，供原生 Ability 和蓝图 GE 共用。
UArenaGameplayEffect_EnemyRangedCooldown::UArenaGameplayEffect_EnemyRangedCooldown()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.6f));

	FInheritedTagContainer GrantedTags;
	GrantedTags.AddTag(ArenaGameplayTags::Cooldown_Enemy_RangedAttack);
	UTargetTagsGameplayEffectComponent* TargetTagsComponent =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComponent"));
	GEComponents.Add(TargetTagsComponent);
	TargetTagsComponent->SetAndApplyTargetTagChanges(GrantedTags);
}
