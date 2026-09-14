#include "GAS/ArenaGameplayEffect_EliteFrenzy.h"

#include "GAS/ArenaAttributeSet.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

namespace
{
	// 为狂暴效果追加一个固定乘法属性 Modifier。
	void AddFrenzyMultiplier(UGameplayEffect& Effect, const FGameplayAttribute& Attribute, float Multiplier)
	{
		FGameplayModifierInfo& Modifier = Effect.Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::MultiplyCompound;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Multiplier));
	}
}

// 狂暴只由组件应用一次，死亡 Tag 自动移除效果和持续 Cue。
UArenaGameplayEffect_EliteFrenzy::UArenaGameplayEffect_EliteFrenzy()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddFrenzyMultiplier(*this, UArenaAttributeSet::GetAttackPowerAttribute(), 1.35f);
	AddFrenzyMultiplier(*this, UArenaAttributeSet::GetMoveSpeedAttribute(), 1.2f);

	FInheritedTagContainer GrantedTags;
	GrantedTags.AddTag(ArenaGameplayTags::Enemy_Affix_Frenzy_Active);
	UTargetTagsGameplayEffectComponent* TargetTagsComponent =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComponent"));
	GEComponents.Add(TargetTagsComponent);
	TargetTagsComponent->SetAndApplyTargetTagChanges(GrantedTags);

	UTargetTagRequirementsGameplayEffectComponent* RequirementsComponent =
		CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(TEXT("TargetRequirementsComponent"));
	GEComponents.Add(RequirementsComponent);
	RequirementsComponent->RemovalTagRequirements.RequireTags.AddTag(ArenaGameplayTags::State_Dead);

	GameplayCues.Add(FGameplayEffectCue(ArenaGameplayTags::GameplayCue_Enemy_Affix_Frenzy_Active, 0.0f, 0.0f));
	bRequireModifierSuccessToTriggerCues = false;
}
