#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ArenaGameplayEffect_BossEnrage.generated.h"

UCLASS(BlueprintType)
class PROJECTARCANEARENA_API UArenaGameplayEffect_BossEnrage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	// 配置 Boss 第三阶段的永久属性倍率、Enraged 标签和持续 GameplayCue。
	UArenaGameplayEffect_BossEnrage();
};
