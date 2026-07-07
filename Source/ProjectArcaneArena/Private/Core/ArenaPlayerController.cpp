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
	//让 PlayerController 只接收游戏输入，并且鼠标第一次点击用于捕获视口时，不吞掉这次点击事件
	InputMode.SetConsumeCaptureMouseDown(false); 
	SetInputMode(InputMode);
}
