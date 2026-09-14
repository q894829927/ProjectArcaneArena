#include "GAS/ArenaGameplayEffect_CritChanceUpgrade.h"

#include "GAS/ArenaAttributeSet.h"
#include "GAS/ArenaGameplayTags.h"

// 通过通用升级 SetByCaller 修改 CritChance，避免把每层数值硬编码在效果类中。
UArenaGameplayEffect_CritChanceUpgrade::UArenaGameplayEffect_CritChanceUpgrade()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat UpgradeMagnitude;
	UpgradeMagnitude.DataTag = ArenaGameplayTags::SetByCaller_Upgrade_NumericValue;
	FGameplayModifierInfo& CritChanceModifier = Modifiers.AddDefaulted_GetRef();
	CritChanceModifier.Attribute = UArenaAttributeSet::GetCritChanceAttribute();
	CritChanceModifier.ModifierOp = EGameplayModOp::Additive;
	CritChanceModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(UpgradeMagnitude);
}
