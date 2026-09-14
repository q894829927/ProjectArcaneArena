#pragma once

#include "CoreMinimal.h"
#include "GAS/ArenaGameplayAbility.h"
#include "ArenaGameplayAbility_EnergyOnAbilityCast.generated.h"

class UGameplayEffect;

UCLASS(Blueprintable)
class PROJECTARCANEARENA_API UArenaGameplayAbility_EnergyOnAbilityCast : public UArenaGameplayAbility
{
	GENERATED_BODY()

public:
	// 配置服务器 OnAbilityCast 触发和默认 Energy 恢复效果。
	UArenaGameplayAbility_EnergyOnAbilityCast();

protected:
	// 只响应当前玩家成功提交、且同时标记为主动技能与 EnergySkill 的施放事件。
	virtual bool ShouldAbilityRespondToEvent(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayEventData* Payload) const override;

	// 按 PlayerState 永久升级层数计算恢复量，并通过 Instant GE 修改自身 Energy。
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Energy On Ability Cast")
	TSubclassOf<UGameplayEffect> EnergyRestoreEffectClass;
};
