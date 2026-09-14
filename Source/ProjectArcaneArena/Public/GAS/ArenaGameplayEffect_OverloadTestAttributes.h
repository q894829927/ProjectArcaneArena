#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ArenaGameplayEffect_OverloadTestAttributes.generated.h"

UCLASS(BlueprintType)
class PROJECTARCANEARENA_API UArenaGameplayEffect_OverloadTestAttributes : public UGameplayEffect
{
	GENERATED_BODY()

public:
	// 为编辑器 Overload 木桩提供固定高血量、零防御和零移动速度的 Instant 属性初始化。
	UArenaGameplayEffect_OverloadTestAttributes();
};
