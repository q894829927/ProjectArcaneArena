#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ArenaPlayerController.generated.h"

UCLASS()
class PROJECTARCANEARENA_API AArenaPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AArenaPlayerController();

protected:
	// 初始化本地输入模式，确保第一次鼠标点击不会被视口捕获吞掉。
	virtual void BeginPlay() override;
};
