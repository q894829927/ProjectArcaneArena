#include "GAS/ArenaGameplayEffect_ShieldGrant.h"

#include "GAS/ArenaAttributeSet.h"
#include "GAS/ArenaGameplayTags.h"

// 创建只修改 Shield 的 Instant GE，使基础值和构筑加成都由 Ability 数据驱动。
UArenaGameplayEffect_ShieldGrant::UArenaGameplayEffect_ShieldGrant()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat ShieldMagnitude;
	ShieldMagnitude.DataTag = ArenaGameplayTags::SetByCaller_Shield_Amount;
	FGameplayModifierInfo& ShieldModifier = Modifiers.AddDefaulted_GetRef();
	ShieldModifier.Attribute = UArenaAttributeSet::GetShieldAttribute();
	ShieldModifier.ModifierOp = EGameplayModOp::Additive;
	ShieldModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(ShieldMagnitude);
}
