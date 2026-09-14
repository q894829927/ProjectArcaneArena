#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ArenaGameplayEffect_ConsumableCooldown.generated.h"

UCLASS()
class PROJECTARCANEARENA_API UArenaGameplayEffect_ConsumableCooldown : public UGameplayEffect
{
	GENERATED_BODY()

public:
	// 创建一秒共享消耗品冷却，Health 与 Energy Potion 不能瞬间连续使用。
	UArenaGameplayEffect_ConsumableCooldown();
};
