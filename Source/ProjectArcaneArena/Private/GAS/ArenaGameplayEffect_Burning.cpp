#include "GAS/ArenaGameplayEffect_Burning.h"

#include "GAS/ArenaGameplayTags.h"
#include "GAS/ExecCalc_BurningDamage.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

// 配置四秒燃烧、每秒一次周期执行、按来源三层堆叠以及状态/Cue 生命周期。
UArenaGameplayEffect_Burning::UArenaGameplayEffect_Burning()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(4.0f));
	Period = FScalableFloat(1.0f);
	bExecutePeriodicEffectOnApplication = true;

	StackingType = EGameplayEffectStackingType::AggregateBySource;
	StackLimitCount = 3;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	StackPeriodResetPolicy = EGameplayEffectStackingPeriodPolicy::ResetOnSuccessfulApplication;
	StackExpirationPolicy = EGameplayEffectStackingExpirationPolicy::ClearEntireStack;
	bDenyOverflowApplication = false;
	bClearStackOnOverflow = false;

	FGameplayEffectExecutionDefinition& BurningExecution = Executions.AddDefaulted_GetRef();
	BurningExecution.CalculationClass = UExecCalc_BurningDamage::StaticClass();

	FInheritedTagContainer AssetTags;
	AssetTags.AddTag(ArenaGameplayTags::Damage_Fire);
	UAssetTagsGameplayEffectComponent* AssetTagsComponent =
		CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(TEXT("AssetTagsComponent"));
	GEComponents.Add(AssetTagsComponent);
	AssetTagsComponent->SetAndApplyAssetTagChanges(AssetTags);

	FInheritedTagContainer GrantedTags;
	GrantedTags.AddTag(ArenaGameplayTags::Status_Burning);
	UTargetTagsGameplayEffectComponent* TargetTagsComponent =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComponent"));
	GEComponents.Add(TargetTagsComponent);
	TargetTagsComponent->SetAndApplyTargetTagChanges(GrantedTags);

	// State.Dead 出现时立即移除 ActiveGE，停止后续周期伤害并结束持续 Cue。
	UTargetTagRequirementsGameplayEffectComponent* RequirementsComponent =
		CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(TEXT("TargetRequirementsComponent"));
	GEComponents.Add(RequirementsComponent);
	RequirementsComponent->RemovalTagRequirements.RequireTags.AddTag(ArenaGameplayTags::State_Dead);

	GameplayCues.Add(FGameplayEffectCue(ArenaGameplayTags::GameplayCue_Status_Burning_Active, 0.0f, 0.0f));
	bSuppressStackingCues = true;
	bRequireModifierSuccessToTriggerCues = false;
}
