#include "GAS/ExecCalc_Damage.h"

#include "AbilitySystemComponent.h"
#include "GAS/ArenaAttributeSet.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffect.h"

struct FArenaDamageStatics
{
	FGameplayEffectAttributeCaptureDefinition AttackPowerDef;
	FGameplayEffectAttributeCaptureDefinition CritChanceDef;
	FGameplayEffectAttributeCaptureDefinition CritDamageDef;
	FGameplayEffectAttributeCaptureDefinition DefenseDef;

	FArenaDamageStatics()
	{
		AttackPowerDef = FGameplayEffectAttributeCaptureDefinition(
			UArenaAttributeSet::GetAttackPowerAttribute(),
			EGameplayEffectAttributeCaptureSource::Source,
			false);
		CritChanceDef = FGameplayEffectAttributeCaptureDefinition(
			UArenaAttributeSet::GetCritChanceAttribute(),
			EGameplayEffectAttributeCaptureSource::Source,
			false);
		CritDamageDef = FGameplayEffectAttributeCaptureDefinition(
			UArenaAttributeSet::GetCritDamageAttribute(),
			EGameplayEffectAttributeCaptureSource::Source,
			false);
		DefenseDef = FGameplayEffectAttributeCaptureDefinition(
			UArenaAttributeSet::GetDefenseAttribute(),
			EGameplayEffectAttributeCaptureSource::Target,
			false);
	}
};

static const FArenaDamageStatics& DamageStatics()
{
	static FArenaDamageStatics Statics;
	return Statics;
}

UExecCalc_Damage::UExecCalc_Damage()
{
	RelevantAttributesToCapture.Add(DamageStatics().AttackPowerDef);
	RelevantAttributesToCapture.Add(DamageStatics().CritChanceDef);
	RelevantAttributesToCapture.Add(DamageStatics().CritDamageDef);
	RelevantAttributesToCapture.Add(DamageStatics().DefenseDef);
}

void UExecCalc_Damage::Execute_Implementation(
	const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();
	if (!TargetASC || TargetASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Invincible))
	{
		return;
	}

	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	FAggregatorEvaluateParameters EvaluateParameters;
	EvaluateParameters.SourceTags = SourceTags;
	EvaluateParameters.TargetTags = TargetTags;

	float AttackPower = 0.0f;
	float CritChance = 0.0f;
	float CritDamage = 1.0f;
	float Defense = 0.0f;

	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
		DamageStatics().AttackPowerDef,
		EvaluateParameters,
		AttackPower);
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
		DamageStatics().CritChanceDef,
		EvaluateParameters,
		CritChance);
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
		DamageStatics().CritDamageDef,
		EvaluateParameters,
		CritDamage);
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
		DamageStatics().DefenseDef,
		EvaluateParameters,
		Defense);

	AttackPower = FMath::Max(AttackPower, 0.0f);
	CritChance = FMath::Clamp(CritChance, 0.0f, 1.0f);
	CritDamage = FMath::Max(CritDamage, 1.0f);
	Defense = FMath::Max(Defense, 0.0f);

	const float BaseDamage = FMath::Max(
		Spec.GetSetByCallerMagnitude(ArenaGameplayTags::SetByCaller_Damage_Base, false, 0.0f),
		0.0f);
	const float SkillMultiplier = FMath::Max(
		Spec.GetSetByCallerMagnitude(ArenaGameplayTags::SetByCaller_Damage_SkillMultiplier, false, 1.0f),
		0.0f);
	const float CritMultiplier = FMath::FRand() <= CritChance ? CritDamage : 1.0f;
	const float DefenseReduction = 100.0f / (100.0f + Defense);

	const float FinalDamage = (BaseDamage + AttackPower) * SkillMultiplier * CritMultiplier * DefenseReduction;
	if (FinalDamage <= 0.0f)
	{
		return;
	}

	OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
		UArenaAttributeSet::GetDamageAttribute(),
		EGameplayModOp::Additive,
		FinalDamage));
}
