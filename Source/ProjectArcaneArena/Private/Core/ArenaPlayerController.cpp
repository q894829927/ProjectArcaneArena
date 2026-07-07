#include "Core/ArenaPlayerController.h"

AArenaPlayerController::AArenaPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
	DefaultMouseCursor = EMouseCursor::Default;
}

void AArenaPlayerController::BeginPlay()
{
	Super::BeginPlay();

	FInputModeGameOnly InputMode;
	// 鼠标第一次点击可能同时用于捕获视口，不能吞掉这次战斗输入。
	InputMode.SetConsumeCaptureMouseDown(false);
	SetInputMode(InputMode);
}
