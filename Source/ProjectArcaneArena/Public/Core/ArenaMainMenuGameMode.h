#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ArenaMainMenuGameMode.generated.h"

UCLASS()
class PROJECTARCANEARENA_API AArenaMainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AArenaMainMenuGameMode();

protected:
	// 菜单无需出生点，直接保留 Controller 的默认变换并让登录流程成功完成。
	virtual bool UpdatePlayerStartSpot(AController* Player, const FString& Portal, FString& OutErrorMessage) override;

	// 菜单使用 UMG View，不创建 AHUD Actor。
	virtual void InitializeHUDForPlayer_Implementation(APlayerController* NewPlayer) override;

	// 菜单关卡保留本地 PlayerController，但明确禁止任何重生路径生成 Pawn。
	virtual void RestartPlayer(AController* NewPlayer) override;
};
