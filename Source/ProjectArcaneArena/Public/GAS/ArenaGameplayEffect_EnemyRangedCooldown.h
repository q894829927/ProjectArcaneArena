#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ArenaGameplayEffect_EnemyRangedCooldown.generated.h"

UCLASS(BlueprintType)
class PROJECTARCANEARENA_API UArenaGameplayEffect_EnemyRangedCooldown : public UGameplayEffect
{
	GENERATED_BODY()

public:
	// 提供 1.6 秒默认远程攻击冷却，蓝图子类可继续调节持续时间。
	UArenaGameplayEffect_EnemyRangedCooldown();
};
