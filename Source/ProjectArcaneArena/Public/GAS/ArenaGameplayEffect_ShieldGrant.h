#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ArenaGameplayEffect_ShieldGrant.generated.h"

UCLASS()
class PROJECTARCANEARENA_API UArenaGameplayEffect_ShieldGrant : public UGameplayEffect
{
	GENERATED_BODY()

public:
	// 配置通用瞬时 Shield 增加，实际数值由 Shield Ability 通过 SetByCaller 注入。
	UArenaGameplayEffect_ShieldGrant();
};
