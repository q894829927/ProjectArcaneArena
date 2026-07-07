#include "Core/ArenaPlayerController.h"

#include "Core/ArenaPlayerState.h"
#include "UI/ArenaPlayerHUDWidget.h"

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

	CreatePlayerHUD();
	TryBindPlayerHUD();
}

void AArenaPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	TryBindPlayerHUD();
}

void AArenaPlayerController::CreatePlayerHUD()
{
	if (!IsLocalController() || PlayerHUDWidget || !PlayerHUDWidgetClass)
	{
		return;
	}

	PlayerHUDWidget = CreateWidget<UArenaPlayerHUDWidget>(this, PlayerHUDWidgetClass);
	if (PlayerHUDWidget)
	{
		PlayerHUDWidget->AddToViewport();
	}
}

void AArenaPlayerController::TryBindPlayerHUD()
{
	if (!IsLocalController())
	{
		return;
	}

	if (!PlayerHUDWidget)
	{
		CreatePlayerHUD();
	}

	if (!PlayerHUDWidget)
	{
		return;
	}

	AArenaPlayerState* ArenaPlayerState = Cast<AArenaPlayerState>(PlayerState);
	if (!ArenaPlayerState || !ArenaPlayerState->GetArenaAbilitySystemComponent() || !ArenaPlayerState->GetArenaAttributeSet())
	{
		SchedulePlayerHUDBindingRetry();
		return;
	}

	PlayerHUDWidget->BindToAbilitySystem(ArenaPlayerState->GetArenaAbilitySystemComponent(), ArenaPlayerState->GetArenaAttributeSet());
	ClearPlayerHUDBindingRetry();
}

void AArenaPlayerController::SchedulePlayerHUDBindingRetry()
{
	if (!GetWorld() || GetWorldTimerManager().IsTimerActive(PlayerHUDBindingRetryTimerHandle))
	{
		return;
	}

	// 客户端 PlayerState/ASC 可能稍后复制到位，使用短定时器等待而不是 Tick 轮询。
	GetWorldTimerManager().SetTimer(
		PlayerHUDBindingRetryTimerHandle,
		this,
		&AArenaPlayerController::TryBindPlayerHUD,
		PlayerHUDBindingRetryInterval,
		true);
}

void AArenaPlayerController::ClearPlayerHUDBindingRetry()
{
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(PlayerHUDBindingRetryTimerHandle);
	}
}
