#include "GAS/ArenaGameplayEffect_BossAttributes.h"

#include "GAS/ArenaAttributeSet.h"

namespace
{
	// 为 Boss 初始化 GE 添加固定 Override，确保每次服务器生成都得到一致基础属性。
	void AddBossAttributeOverride(UGameplayEffect& GameplayEffect, const FGameplayAttribute& Attribute, float Value)
	{
		FGameplayModifierInfo& Modifier = GameplayEffect.Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::Override;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Value));
	}
}

// Boss 默认值只负责第一阶段基线，所有属性仍通过 GAS 初始化而非 Character 直接写入。
UArenaGameplayEffect_BossAttributes::UArenaGameplayEffect_BossAttributes()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	AddBossAttributeOverride(*this, UArenaAttributeSet::GetMaxHealthAttribute(), 1200.0f);
	AddBossAttributeOverride(*this, UArenaAttributeSet::GetHealthAttribute(), 1200.0f);
	AddBossAttributeOverride(*this, UArenaAttributeSet::GetShieldAttribute(), 0.0f);
	AddBossAttributeOverride(*this, UArenaAttributeSet::GetMaxEnergyAttribute(), 0.0f);
	AddBossAttributeOverride(*this, UArenaAttributeSet::GetEnergyAttribute(), 0.0f);
	AddBossAttributeOverride(*this, UArenaAttributeSet::GetAttackPowerAttribute(), 10.0f);
	AddBossAttributeOverride(*this, UArenaAttributeSet::GetDefenseAttribute(), 5.0f);
	AddBossAttributeOverride(*this, UArenaAttributeSet::GetMoveSpeedAttribute(), 300.0f);
	AddBossAttributeOverride(*this, UArenaAttributeSet::GetCritChanceAttribute(), 0.0f);
	AddBossAttributeOverride(*this, UArenaAttributeSet::GetCritDamageAttribute(), 2.0f);
}
