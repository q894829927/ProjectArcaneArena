#include "GAS/ExecCalc_BurningDamage.h"

#include "AbilitySystemComponent.h"
#include "GAS/ArenaAttributeSet.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffect.h"

// 服务端按 Burning Spec 层数结算固定伤害，并复用 AttributeSet 的护盾优先伤害入口。
void UExecCalc_BurningDamage::Execute_Implementation(
	const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();
	if (!TargetASC || !TargetASC->IsOwnerActorAuthoritative()
		|| TargetASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead)
		|| TargetASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Invincible))
	{
		return;
	}

	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	const float DamagePerStack = FMath::Max(
		Spec.GetSetByCallerMagnitude(ArenaGameplayTags::SetByCaller_Damage_Burning, false, 0.0f),
		0.0f);
	const int32 StackCount = FMath::Max(Spec.GetStackCount(), 1);
	const float FinalDamage = DamagePerStack * static_cast<float>(StackCount);
	if (FinalDamage <= 0.0f)
	{
		return;
	}

	OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
		UArenaAttributeSet::GetDamageAttribute(),
		EGameplayModOp::Additive,
		FinalDamage));
}
