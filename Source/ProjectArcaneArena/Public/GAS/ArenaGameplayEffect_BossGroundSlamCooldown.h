#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ArenaGameplayEffect_BossGroundSlamCooldown.generated.h"

UCLASS(BlueprintType)
class PROJECTARCANEARENA_API UArenaGameplayEffect_BossGroundSlamCooldown : public UGameplayEffect
{
	GENERATED_BODY()

public:
	// 提供 GroundSlam 五秒默认冷却及阻断标签，蓝图子类可继续做数值平衡。
	UArenaGameplayEffect_BossGroundSlamCooldown();
};
