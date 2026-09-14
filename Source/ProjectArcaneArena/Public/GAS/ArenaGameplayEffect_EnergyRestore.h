#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ArenaGameplayEffect_EnergyRestore.generated.h"

UCLASS()
class PROJECTARCANEARENA_API UArenaGameplayEffect_EnergyRestore : public UGameplayEffect
{
	GENERATED_BODY()

public:
	// 配置通用瞬时 Energy 恢复，具体数值由服务器通过 SetByCaller 注入。
	UArenaGameplayEffect_EnergyRestore();
};
