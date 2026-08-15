#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ArenaGameplayEffect_EliteFrenzy.generated.h"

UCLASS()
class PROJECTARCANEARENA_API UArenaGameplayEffect_EliteFrenzy : public UGameplayEffect
{
	GENERATED_BODY()

public:
	// 配置一次触发后持续到死亡的 AttackPower 与 MoveSpeed 狂暴强化。
	UArenaGameplayEffect_EliteFrenzy();
};
