#include "GAS/ArenaGameplayEffect_OverloadLockout.h"

#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

// 按来源分别维护一秒限频；同一来源重施加只刷新自身在该目标上的持续时间。
UArenaGameplayEffect_OverloadLockout::UArenaGameplayEffect_OverloadLockout()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.0f));

	StackingType = EGameplayEffectStackingType::AggregateBySource;
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	StackPeriodResetPolicy = EGameplayEffectStackingPeriodPolicy::ResetOnSuccessfulApplication;
	StackExpirationPolicy = EGameplayEffectStackingExpirationPolicy::ClearEntireStack;
	bDenyOverflowApplication = false;
	bClearStackOnOverflow = false;

	FInheritedTagContainer GrantedTags;
	GrantedTags.AddTag(ArenaGameplayTags::Status_Overload_Lockout);
	UTargetTagsGameplayEffectComponent* TargetTagsComponent =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComponent"));
	GEComponents.Add(TargetTagsComponent);
	TargetTagsComponent->SetAndApplyTargetTagChanges(GrantedTags);

	// 敌人死亡时清除限频 GE，不让短生命周期尸体继续持有来源状态。
	UTargetTagRequirementsGameplayEffectComponent* RequirementsComponent =
		CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(TEXT("TargetRequirementsComponent"));
	GEComponents.Add(RequirementsComponent);
	RequirementsComponent->RemovalTagRequirements.RequireTags.AddTag(ArenaGameplayTags::State_Dead);
}
