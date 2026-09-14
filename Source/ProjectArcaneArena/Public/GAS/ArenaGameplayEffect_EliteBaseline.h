#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ArenaGameplayEffect_EliteBaseline.generated.h"

UCLASS()
class PROJECTARCANEARENA_API UArenaGameplayEffect_EliteBaseline : public UGameplayEffect
{
	GENERATED_BODY()

public:
	// 通过 SetByCaller 同时扩展精英 MaxHealth、当前 Health、AttackPower 与 Defense。
	UArenaGameplayEffect_EliteBaseline();
};
