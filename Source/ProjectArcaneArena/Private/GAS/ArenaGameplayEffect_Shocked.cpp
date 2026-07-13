#include "GAS/ArenaGameplayEffect_Shocked.h"

#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

// 配置全来源共享的四秒 Shocked 状态；重复应用只刷新持续时间和易伤数值。
UArenaGameplayEffect_Shocked::UArenaGameplayEffect_Shocked()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(4.0f));

	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	StackPeriodResetPolicy = EGameplayEffectStackingPeriodPolicy::ResetOnSuccessfulApplication;
	StackExpirationPolicy = EGameplayEffectStackingExpirationPolicy::ClearEntireStack;
	bDenyOverflowApplication = false;
	bClearStackOnOverflow = false;

	FInheritedTagContainer GrantedTags;
	GrantedTags.AddTag(ArenaGameplayTags::Status_Shocked);
	UTargetTagsGameplayEffectComponent* TargetTagsComponent =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComponent"));
	GEComponents.Add(TargetTagsComponent);
	TargetTagsComponent->SetAndApplyTargetTagChanges(GrantedTags);

	// 死亡标签出现时立即移除状态，确保持续 Cue 不会残留在尸体上。
	UTargetTagRequirementsGameplayEffectComponent* RequirementsComponent =
		CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(TEXT("TargetRequirementsComponent"));
	GEComponents.Add(RequirementsComponent);
	RequirementsComponent->RemovalTagRequirements.RequireTags.AddTag(ArenaGameplayTags::State_Dead);

	GameplayCues.Add(FGameplayEffectCue(ArenaGameplayTags::GameplayCue_Status_Shocked_Active, 0.0f, 0.0f));
	bSuppressStackingCues = true;
	bRequireModifierSuccessToTriggerCues = false;
}
