#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ArenaGameplayEffect_Shocked.generated.h"

UCLASS(BlueprintType)
class PROJECTARCANEARENA_API UArenaGameplayEffect_Shocked : public UGameplayEffect
{
	GENERATED_BODY()

public:
	// 提供四秒、全来源共享且重复命中只刷新的 Shocked 状态配置。
	UArenaGameplayEffect_Shocked();
};
