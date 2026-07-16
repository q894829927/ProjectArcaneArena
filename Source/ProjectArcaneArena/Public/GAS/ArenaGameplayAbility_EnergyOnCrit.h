#pragma once

#include "CoreMinimal.h"
#include "GAS/ArenaGameplayAbility.h"
#include "ArenaGameplayAbility_EnergyOnCrit.generated.h"

class UGameplayEffect;

UCLASS(Blueprintable)
class PROJECTARCANEARENA_API UArenaGameplayAbility_EnergyOnCrit : public UArenaGameplayAbility
{
	GENERATED_BODY()

public:
	// 配置服务器 OnCrit 事件触发以及默认 Energy 恢复效果。
	UArenaGameplayAbility_EnergyOnCrit();

protected:
	// 只响应由权威伤害管线确认、且实际伤害目标为敌人的暴击事件。
	virtual bool ShouldAbilityRespondToEvent(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayEventData* Payload) const override;

	// 按 PlayerState 永久升级层数计算恢复量，并通过 Instant GE 修改自身 Energy。
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Energy On Crit")
	TSubclassOf<UGameplayEffect> EnergyRestoreEffectClass;
};
