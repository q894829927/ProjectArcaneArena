#pragma once

#include "CoreMinimal.h"
#include "GAS/ArenaGameplayAbility.h"
#include "ArenaGameplayAbility_Shield.generated.h"

class UGameplayEffect;

UCLASS(Blueprintable)
class PROJECTARCANEARENA_API UArenaGameplayAbility_Shield : public UArenaGameplayAbility
{
	GENERATED_BODY()

public:
	// 设置 Shield 的输入标签、基础数值和状态阻断规则。
	UArenaGameplayAbility_Shield();

protected:
	// 两端按 PlayerState 永久升级计算护盾量，提交消耗/冷却后对自身应用 SetByCaller GE。
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Shield")
	TSubclassOf<UGameplayEffect> ShieldEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Shield", meta = (ClampMin = "0.0"))
	float BaseShieldAmount = 30.0f;
};
