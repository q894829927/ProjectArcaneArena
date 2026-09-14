#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ArenaGameplayEffect_CritChanceUpgrade.generated.h"

UCLASS()
class PROJECTARCANEARENA_API UArenaGameplayEffect_CritChanceUpgrade : public UGameplayEffect
{
	GENERATED_BODY()

public:
	// 配置由升级 DataAsset NumericValue 驱动的瞬时暴击率加成。
	UArenaGameplayEffect_CritChanceUpgrade();
};
