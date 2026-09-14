#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ArenaGameplayEffect_BossChargeCooldown.generated.h"

UCLASS(BlueprintType)
class PROJECTARCANEARENA_API UArenaGameplayEffect_BossChargeCooldown : public UGameplayEffect
{
	GENERATED_BODY()

public:
	// 提供 Boss Charge 默认六秒冷却和对应阻断标签，蓝图子类可继续调整数值。
	UArenaGameplayEffect_BossChargeCooldown();
};
