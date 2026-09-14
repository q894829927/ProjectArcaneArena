#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ArenaGameplayEffect_BossFireZoneCooldown.generated.h"

UCLASS()
class PROJECTARCANEARENA_API UArenaGameplayEffect_BossFireZoneCooldown : public UGameplayEffect
{
	GENERATED_BODY()

public:
	// 创建 FireZone 的默认八秒 Duration 冷却并授予对应标签；蓝图派生 GE 可覆盖持续时间。
	UArenaGameplayEffect_BossFireZoneCooldown();
};
