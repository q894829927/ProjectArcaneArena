#pragma once

#include "CoreMinimal.h"
#include "GAS/ArenaGameplayAbility.h"
#include "ArenaGameplayAbility_EnergyOnKill.generated.h"

class UGameplayEffect;

UCLASS(Blueprintable)
class PROJECTARCANEARENA_API UArenaGameplayAbility_EnergyOnKill : public UArenaGameplayAbility
{
	GENERATED_BODY()

public:
	// 配置服务器 OnKill 事件触发以及默认 Energy 恢复效果。
	UArenaGameplayAbility_EnergyOnKill();

protected:
	// 只响应由权威伤害管线确认的敌人击杀事件。
	virtual bool ShouldAbilityRespondToEvent(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayEventData* Payload) const override;

	// 按 PlayerState 永久升级层数计算恢复量，并通过 Instant GE 修改自身 Energy。
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Energy On Kill")
	TSubclassOf<UGameplayEffect> EnergyRestoreEffectClass;
};
