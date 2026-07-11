#include "Core/ArenaPlayerController.h"

#include "Core/ArenaPlayerState.h"
#include "UI/ArenaPlayerHUDWidget.h"

// 构造玩家控制器，设置鼠标显示和基础输入交互选项。
AArenaPlayerController::AArenaPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
	DefaultMouseCursor = EMouseCursor::Default;
}

// 本地控制器开始时设置输入模式，并创建/绑定玩家 HUD。
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

// Possess 新 Pawn 后再次尝试绑定 HUD，处理 PlayerState 或 ASC 稍后就绪的情况。
void AArenaPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	TryBindPlayerHUD();
}

// 仅在本地控制器上创建玩家 HUD，并加入视口。
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

// 尝试把 HUD 绑定到 PlayerState 上的 ASC 和 AttributeSet。
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

// 当客户端 GAS 数据尚未复制完成时，安排短间隔重试绑定 HUD。
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

// HUD 成功绑定或不再需要重试时清理定时器。
void AArenaPlayerController::ClearPlayerHUDBindingRetry()
{
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(PlayerHUDBindingRetryTimerHandle);
	}
}
