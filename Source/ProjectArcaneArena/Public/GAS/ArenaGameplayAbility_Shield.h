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
	// 设置 Shield 的输入标签和状态阻断规则，具体数值由 GameplayEffect 资产配置。
	UArenaGameplayAbility_Shield();

protected:
	// 服务端提交消耗/冷却后，对自身 ASC 应用 GE_Shield。
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Shield")
	TSubclassOf<UGameplayEffect> ShieldEffectClass;
};
