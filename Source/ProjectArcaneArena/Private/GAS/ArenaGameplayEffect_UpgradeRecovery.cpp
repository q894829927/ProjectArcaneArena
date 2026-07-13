#include "GAS/ArenaGameplayEffect_UpgradeRecovery.h"

#include "GAS/ArenaAttributeSet.h"
#include "GAS/ArenaGameplayTags.h"

// 创建同时补充 Health 和 Energy 的 Instant GE，避免升级流程直接写 Attribute。
UArenaGameplayEffect_UpgradeRecovery::UArenaGameplayEffect_UpgradeRecovery()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat HealthRecoveryMagnitude;
	HealthRecoveryMagnitude.DataTag = ArenaGameplayTags::SetByCaller_Recovery_Health;
	FGameplayModifierInfo& HealthModifier = Modifiers.AddDefaulted_GetRef();
	HealthModifier.Attribute = UArenaAttributeSet::GetHealthAttribute();
	HealthModifier.ModifierOp = EGameplayModOp::Additive;
	HealthModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(HealthRecoveryMagnitude);

	FSetByCallerFloat EnergyRecoveryMagnitude;
	EnergyRecoveryMagnitude.DataTag = ArenaGameplayTags::SetByCaller_Recovery_Energy;
	FGameplayModifierInfo& EnergyModifier = Modifiers.AddDefaulted_GetRef();
	EnergyModifier.Attribute = UArenaAttributeSet::GetEnergyAttribute();
	EnergyModifier.ModifierOp = EGameplayModOp::Additive;
	EnergyModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(EnergyRecoveryMagnitude);
}
