#include "GAS/ArenaGameplayEffect_BossEnrage.h"

#include "GAS/ArenaAttributeSet.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

namespace
{
	// 向狂暴效果添加复合乘算，确保多个来源不会被误当作加法百分比。
	void AddEnrageMultiplier(UGameplayEffect& GameplayEffect, const FGameplayAttribute& Attribute, float Multiplier)
	{
		FGameplayModifierInfo& Modifier = GameplayEffect.Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::MultiplyCompound;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Multiplier));
	}
}

// 狂暴效果保持 Infinite，由 Boss 阶段状态机保存 Handle 并在死亡或离开 Combat 时移除。
UArenaGameplayEffect_BossEnrage::UArenaGameplayEffect_BossEnrage()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	AddEnrageMultiplier(*this, UArenaAttributeSet::GetAttackPowerAttribute(), 1.30f);
	AddEnrageMultiplier(*this, UArenaAttributeSet::GetMoveSpeedAttribute(), 1.20f);

	FInheritedTagContainer GrantedTags;
	GrantedTags.AddTag(ArenaGameplayTags::Boss_State_Enraged);
	UTargetTagsGameplayEffectComponent* TargetTagsComponent =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComponent"));
	GEComponents.Add(TargetTagsComponent);
	TargetTagsComponent->SetAndApplyTargetTagChanges(GrantedTags);

	UTargetTagRequirementsGameplayEffectComponent* RequirementsComponent =
		CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(TEXT("TargetRequirementsComponent"));
	GEComponents.Add(RequirementsComponent);
	RequirementsComponent->RemovalTagRequirements.RequireTags.AddTag(ArenaGameplayTags::State_Dead);

	GameplayCues.Add(FGameplayEffectCue(ArenaGameplayTags::GameplayCue_Boss_Enraged_Active, 0.0f, 0.0f));
	bRequireModifierSuccessToTriggerCues = false;
}
