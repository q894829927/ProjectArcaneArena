#include "GAS/ArenaGameplayEffect_BossPlayerCountScaling.h"

#include "GAS/ArenaAttributeSet.h"
#include "GAS/ArenaGameplayTags.h"

// 创建只修改 MaxHealth 基础值的 Instant GE，当前 Health 由独立恢复 GE 补满。
UArenaGameplayEffect_BossPlayerCountScaling::UArenaGameplayEffect_BossPlayerCountScaling()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat MaxHealthDeltaMagnitude;
	MaxHealthDeltaMagnitude.DataTag = ArenaGameplayTags::SetByCaller_Boss_MaxHealthDelta;
	FGameplayModifierInfo& MaxHealthModifier = Modifiers.AddDefaulted_GetRef();
	MaxHealthModifier.Attribute = UArenaAttributeSet::GetMaxHealthAttribute();
	MaxHealthModifier.ModifierOp = EGameplayModOp::Additive;
	MaxHealthModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(MaxHealthDeltaMagnitude);
}
