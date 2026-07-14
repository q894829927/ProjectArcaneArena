#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ArenaGameplayEffect_UpgradeRecovery.generated.h"

UCLASS()
class PROJECTARCANEARENA_API UArenaGameplayEffect_UpgradeRecovery : public UGameplayEffect
{
	GENERATED_BODY()

public:
	// 配置升级选择后的瞬时生命与能量恢复，具体补充值由服务器通过 SetByCaller 注入。
	UArenaGameplayEffect_UpgradeRecovery();
};
