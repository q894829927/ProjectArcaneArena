#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ArenaGameplayEffect_BossPlayerCountScaling.generated.h"

UCLASS()
class PROJECTARCANEARENA_API UArenaGameplayEffect_BossPlayerCountScaling : public UGameplayEffect
{
	GENERATED_BODY()

public:
	// 配置 Boss 初始化 MaxHealth 增量，由服务器人数快照通过 SetByCaller 提供具体数值。
	UArenaGameplayEffect_BossPlayerCountScaling();
};
