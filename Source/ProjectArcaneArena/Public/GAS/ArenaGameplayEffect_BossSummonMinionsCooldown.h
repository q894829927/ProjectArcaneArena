#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ArenaGameplayEffect_BossSummonMinionsCooldown.generated.h"

UCLASS()
class PROJECTARCANEARENA_API UArenaGameplayEffect_BossSummonMinionsCooldown : public UGameplayEffect
{
	GENERATED_BODY()

public:
	// 创建召唤物技能默认十四秒 Duration 冷却并授予对应标签，蓝图派生 GE 可覆盖持续时间。
	UArenaGameplayEffect_BossSummonMinionsCooldown();
};
