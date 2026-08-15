#include "GAS/ArenaGameplayEffect_EliteBaseline.h"

#include "GAS/ArenaAttributeSet.h"
#include "GAS/ArenaGameplayTags.h"

namespace
{
	// 为一个精英属性追加由 GameplayTag 驱动的 SetByCaller 加法 Modifier。
	void AddEliteSetByCallerModifier(
		UGameplayEffect& Effect,
		const FGameplayAttribute& Attribute,
		const FGameplayTag& DataTag)
	{
		FSetByCallerFloat Magnitude;
		Magnitude.DataTag = DataTag;
		FGameplayModifierInfo& Modifier = Effect.Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
	}
}

// 使用单个 Instant GE 原子应用精英基础差额，避免 Character 直接写属性。
UArenaGameplayEffect_EliteBaseline::UArenaGameplayEffect_EliteBaseline()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	AddEliteSetByCallerModifier(*this, UArenaAttributeSet::GetMaxHealthAttribute(), ArenaGameplayTags::SetByCaller_Elite_MaxHealthDelta);
	AddEliteSetByCallerModifier(*this, UArenaAttributeSet::GetHealthAttribute(), ArenaGameplayTags::SetByCaller_Elite_HealthDelta);
	AddEliteSetByCallerModifier(*this, UArenaAttributeSet::GetAttackPowerAttribute(), ArenaGameplayTags::SetByCaller_Elite_AttackPowerDelta);
	AddEliteSetByCallerModifier(*this, UArenaAttributeSet::GetDefenseAttribute(), ArenaGameplayTags::SetByCaller_Elite_DefenseBonus);
}
