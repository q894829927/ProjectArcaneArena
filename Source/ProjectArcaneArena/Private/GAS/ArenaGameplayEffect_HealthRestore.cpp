#include "GAS/ArenaGameplayEffect_HealthRestore.h"

#include "GAS/ArenaAttributeSet.h"
#include "GAS/ArenaGameplayTags.h"

// 创建写入 Healing Meta Attribute 的 Instant GE，避免拾取物直接修改 Health。
UArenaGameplayEffect_HealthRestore::UArenaGameplayEffect_HealthRestore()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat HealthRecoveryMagnitude;
	HealthRecoveryMagnitude.DataTag = ArenaGameplayTags::SetByCaller_Recovery_Health;
	FGameplayModifierInfo& HealthModifier = Modifiers.AddDefaulted_GetRef();
	HealthModifier.Attribute = UArenaAttributeSet::GetHealingAttribute();
	HealthModifier.ModifierOp = EGameplayModOp::Additive;
	HealthModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(HealthRecoveryMagnitude);
}
