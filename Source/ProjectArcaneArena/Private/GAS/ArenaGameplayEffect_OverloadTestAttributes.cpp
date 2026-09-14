#include "GAS/ArenaGameplayEffect_OverloadTestAttributes.h"

#include "GAS/ArenaAttributeSet.h"

namespace
{
	// 为测试属性 GE 添加固定 Override Modifier，确保重复进入 PIE 时得到一致初值。
	void AddAttributeOverride(UGameplayEffect& GameplayEffect, const FGameplayAttribute& Attribute, const float Value)
	{
		FGameplayModifierInfo& Modifier = GameplayEffect.Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::Override;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Value));
	}
}

// 木桩只承载受击、状态和范围测试，不拥有输出伤害或移动能力。
UArenaGameplayEffect_OverloadTestAttributes::UArenaGameplayEffect_OverloadTestAttributes()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	AddAttributeOverride(*this, UArenaAttributeSet::GetMaxHealthAttribute(), 5000.0f);
	AddAttributeOverride(*this, UArenaAttributeSet::GetHealthAttribute(), 5000.0f);
	AddAttributeOverride(*this, UArenaAttributeSet::GetShieldAttribute(), 0.0f);
	AddAttributeOverride(*this, UArenaAttributeSet::GetDefenseAttribute(), 0.0f);
	AddAttributeOverride(*this, UArenaAttributeSet::GetMoveSpeedAttribute(), 0.0f);
	AddAttributeOverride(*this, UArenaAttributeSet::GetAttackPowerAttribute(), 0.0f);
}
