#include "Core/ArenaMainMenuGameMode.h"

#include "Core/ArenaMainMenuPlayerController.h"

// 菜单关卡仅创建专用 Controller，并同时禁用默认 Pawn、SpectatorPawn、HUD 和战斗框架对象。
AArenaMainMenuGameMode::AArenaMainMenuGameMode()
{
	PlayerControllerClass = AArenaMainMenuPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
	SpectatorClass = nullptr;
	HUDClass = nullptr;
	bStartPlayersAsSpectators = true;
}

// 菜单没有 PlayerStart；清空错误并保留 Controller 原点即可完成本地登录初始化。
bool AArenaMainMenuGameMode::UpdatePlayerStartSpot(AController* Player, const FString& Portal, FString& OutErrorMessage)
{
	(void)Portal;
	OutErrorMessage.Reset();

	if (Player)
	{
		Player->SetInitialLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
	}

	return Player != nullptr;
}

// 菜单 View 由专用 PlayerController 创建，跳过默认 AHUD Actor 的生成 RPC。
void AArenaMainMenuGameMode::InitializeHUDForPlayer_Implementation(APlayerController* NewPlayer)
{
	(void)NewPlayer;
}

// 菜单只需要 Controller 驱动 UI，跳过 AGameModeBase 的出生点查找和 Pawn 生成流程。
void AArenaMainMenuGameMode::RestartPlayer(AController* NewPlayer)
{
	(void)NewPlayer;
}
