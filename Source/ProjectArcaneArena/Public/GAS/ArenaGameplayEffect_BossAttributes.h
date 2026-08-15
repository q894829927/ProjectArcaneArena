#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ArenaGameplayEffect_BossAttributes.generated.h"

UCLASS(BlueprintType)
class PROJECTARCANEARENA_API UArenaGameplayEffect_BossAttributes : public UGameplayEffect
{
	GENERATED_BODY()

public:
	// 提供 Boss Foundation 的可覆盖默认属性，蓝图子类可在后续平衡阶段继续调节。
	UArenaGameplayEffect_BossAttributes();
};
