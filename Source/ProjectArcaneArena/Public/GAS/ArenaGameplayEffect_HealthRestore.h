#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ArenaGameplayEffect_HealthRestore.generated.h"

UCLASS()
class PROJECTARCANEARENA_API UArenaGameplayEffect_HealthRestore : public UGameplayEffect
{
	GENERATED_BODY()

public:
	// 配置通用瞬时 Health 恢复，通过 Healing Meta Attribute 进入统一结算路径。
	UArenaGameplayEffect_HealthRestore();
};
