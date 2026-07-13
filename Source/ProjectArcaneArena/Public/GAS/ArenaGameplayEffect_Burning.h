#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ArenaGameplayEffect_Burning.generated.h"

UCLASS(BlueprintType)
class PROJECTARCANEARENA_API UArenaGameplayEffect_Burning : public UGameplayEffect
{
	GENERATED_BODY()

public:
	// 提供可直接使用或由 GE_Status_Burning 继承的服务器权威周期燃烧配置。
	UArenaGameplayEffect_Burning();
};
