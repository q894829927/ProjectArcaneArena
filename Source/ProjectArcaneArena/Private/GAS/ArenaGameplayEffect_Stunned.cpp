#include "GAS/ArenaGameplayEffect_Stunned.h"

#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

// 提供可直接使用或由 GE_Status_Stunned 蓝图继承的两秒眩晕效果。
UArenaGameplayEffect_Stunned::UArenaGameplayEffect_Stunned()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(2.0f));

	FInheritedTagContainer GrantedTags;
	GrantedTags.AddTag(ArenaGameplayTags::State_Stunned);

	// GameplayEffect CDO 构造期间必须使用具名默认子对象；FindOrAddComponent 内部的 NewObject 仅适用于构造完成后的动态 GE。
	UTargetTagsGameplayEffectComponent* TargetTagsComponent =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComponent"));
	GEComponents.Add(TargetTagsComponent);
	TargetTagsComponent->SetAndApplyTargetTagChanges(GrantedTags);
}
