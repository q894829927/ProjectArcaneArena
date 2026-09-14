#include "GAS/ArenaGameplayEffect_EnergyRestore.h"

#include "GAS/ArenaAttributeSet.h"
#include "GAS/ArenaGameplayTags.h"

// 创建只修改 Energy 的 Instant GE，避免触发升级直接写 Attribute。
UArenaGameplayEffect_EnergyRestore::UArenaGameplayEffect_EnergyRestore()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat EnergyRecoveryMagnitude;
	EnergyRecoveryMagnitude.DataTag = ArenaGameplayTags::SetByCaller_Recovery_Energy;
	FGameplayModifierInfo& EnergyModifier = Modifiers.AddDefaulted_GetRef();
	EnergyModifier.Attribute = UArenaAttributeSet::GetEnergyAttribute();
	EnergyModifier.ModifierOp = EGameplayModOp::Additive;
	EnergyModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(EnergyRecoveryMagnitude);
}
