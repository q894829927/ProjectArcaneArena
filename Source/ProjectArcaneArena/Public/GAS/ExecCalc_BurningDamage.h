#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "ExecCalc_BurningDamage.generated.h"

UCLASS()
class PROJECTARCANEARENA_API UExecCalc_BurningDamage : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	// 按 Burning 当前层数输出固定周期伤害，不参与攻击力、防御或暴击计算。
	virtual void Execute_Implementation(
		const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
