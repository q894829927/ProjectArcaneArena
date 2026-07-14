#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ArenaGameplayEffect_OverloadLockout.generated.h"

UCLASS(BlueprintType)
class PROJECTARCANEARENA_API UArenaGameplayEffect_OverloadLockout : public UGameplayEffect
{
	GENERATED_BODY()

public:
	// 提供按来源独立聚合的一秒 Overload 触发限频状态。
	UArenaGameplayEffect_OverloadLockout();
};
