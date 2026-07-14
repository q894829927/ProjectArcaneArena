#include "Core/ArenaPlayerController.h"

#include "Core/ArenaGameMode.h"
#include "Core/ArenaPlayerState.h"
#include "Core/ArenaGameState.h"
#include "Core/ArenaUpgradeDataAsset.h"
#include "Components/Widget.h"
#include "GameFramework/PawnMovementComponent.h"
#include "UI/ArenaPlayerHUDWidget.h"
#include "UI/ArenaUpgradeSelectionWidget.h"

namespace
{
	// 将 OwnerOnly 候选与 PlayerState 永久层数合并为纯本地 UI 快照，不赋予 Widget 玩法写权限。
	TArray<FArenaUpgradeChoiceViewData> BuildUpgradeChoiceViewData(const AArenaPlayerState* ArenaPlayerState)
	{
		TArray<FArenaUpgradeChoiceViewData> ViewData;
		if (!ArenaPlayerState)
		{
			return ViewData;
		}

		for (UArenaUpgradeDataAsset* Upgrade : ArenaPlayerState->GetUpgradeCandidates())
		{
			if (!Upgrade)
			{
				continue;
			}

			FArenaUpgradeChoiceViewData& Choice = ViewData.AddDefaulted_GetRef();
			Choice.Upgrade = Upgrade;
			Choice.CurrentStacks = FMath::Max(ArenaPlayerState->GetUpgradeStackCount(Upgrade->UpgradeID), 0);
			Choice.MaxStacks = FMath::Max(Upgrade->MaxStacks, 1);
			Choice.ResultingStacks = FMath::Clamp(Choice.CurrentStacks + 1, 1, Choice.MaxStacks);
		}
		return ViewData;
	}
}

// 构造玩家控制器，设置基础鼠标输入并指定可直接使用的原生升级界面类。
AArenaPlayerController::AArenaPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
	DefaultMouseCursor = EMouseCursor::Default;
	UpgradeSelectionWidgetClass = UArenaUpgradeSelectionWidget::StaticClass();
}

// 本地控制器开始时创建 HUD/升级界面，并绑定 PlayerState 与 GameState 数据源。
void AArenaPlayerController::BeginPlay()
{
	Super::BeginPlay();

	CreatePlayerHUD();
	CreateUpgradeSelectionWidget();
	SetThirdPersonInputMode(false);
	TryBindPlayerHUD();
	BindUpgradeState();
	BindGameStateHUD();
}

// PlayerState 在客户端完成复制后重新绑定 GAS HUD 和 OwnerOnly 升级状态。
void AArenaPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	TryBindPlayerHUD();
	BindUpgradeState();
}

// Possess 新 Pawn 后重新绑定 HUD 与升级状态，兼容重生和 PlayerState 稍后就绪。
void AArenaPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	TryBindPlayerHUD();
	BindUpgradeState();
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
		PlayerHUDWidget->SetThirdPersonReticleVisible(bThirdPersonInputMode);
	}
}

// 创建独立升级界面；默认原生 Widget 可直接使用，蓝图子类只需替换布局和视觉。
void AArenaPlayerController::CreateUpgradeSelectionWidget()
{
	if (!IsLocalController() || UpgradeSelectionWidget || !UpgradeSelectionWidgetClass)
	{
		return;
	}

	UpgradeSelectionWidget = CreateWidget<UArenaUpgradeSelectionWidget>(this, UpgradeSelectionWidgetClass);
	if (UpgradeSelectionWidget)
	{
		UpgradeSelectionWidget->AddToViewport(20);
		UpgradeSelectionWidget->OnUpgradeChosen.AddUniqueDynamic(this, &AArenaPlayerController::HandleUpgradeChosen);
	}
}

// 绑定当前 PlayerState 的升级复制委托，并立即用现有快照刷新界面。
void AArenaPlayerController::BindUpgradeState()
{
	if (!IsLocalController())
	{
		return;
	}

	AArenaPlayerState* ArenaPlayerState = GetPlayerState<AArenaPlayerState>();
	if (!ArenaPlayerState)
	{
		return;
	}

	if (BoundUpgradePlayerState.Get() != ArenaPlayerState)
	{
		UnbindUpgradeState();
		BoundUpgradePlayerState = ArenaPlayerState;
		ArenaPlayerState->OnUpgradeStateChanged.AddUniqueDynamic(this, &AArenaPlayerController::HandleUpgradeStateChanged);
	}

	RefreshUpgradeSelectionUI();
}

// 解除旧 PlayerState 的升级委托，避免重生、旅行或重连后重复回调。
void AArenaPlayerController::UnbindUpgradeState()
{
	if (AArenaPlayerState* ArenaPlayerState = BoundUpgradePlayerState.Get())
	{
		ArenaPlayerState->OnUpgradeStateChanged.RemoveDynamic(this, &AArenaPlayerController::HandleUpgradeStateChanged);
	}
	BoundUpgradePlayerState.Reset();
}

// 根据复制阶段和候选决定是否显示界面，并把 PlayerState 层数整理为只读卡片展示快照。
void AArenaPlayerController::RefreshUpgradeSelectionUI()
{
	if (!IsLocalController())
	{
		return;
	}

	if (!UpgradeSelectionWidget)
	{
		CreateUpgradeSelectionWidget();
	}

	const AArenaPlayerState* ArenaPlayerState = BoundUpgradePlayerState.Get();
	const AArenaGameState* ArenaGameState = GetWorld() ? GetWorld()->GetGameState<AArenaGameState>() : nullptr;
	const bool bShouldShow = UpgradeSelectionWidget && ArenaPlayerState && ArenaGameState
		&& ArenaGameState->GetGamePhase() == EArenaGamePhase::Upgrade
		&& !ArenaPlayerState->HasSelectedUpgrade()
		&& !ArenaPlayerState->GetUpgradeCandidates().IsEmpty();

	if (bShouldShow)
	{
		UpgradeSelectionWidget->ShowUpgradeChoices(BuildUpgradeChoiceViewData(ArenaPlayerState));
	}
	else if (UpgradeSelectionWidget)
	{
		UpgradeSelectionWidget->HideUpgradeChoices();
	}
	SetUpgradeInputMode(bShouldShow);
}

// 升级期间切为 UIOnly 并聚焦首个有效按钮；结束后恢复当前视角输入。
void AArenaPlayerController::SetUpgradeInputMode(bool bEnabled)
{
	if (!IsLocalController() || bUpgradeInputMode == bEnabled)
	{
		return;
	}

	bUpgradeInputMode = bEnabled;
	if (bUpgradeInputMode && UpgradeSelectionWidget)
	{
		// UIOnly 会截断 Enhanced Input 的 Completed/Canceled 事件；先清键并阻止后续移动输入。
		SetIgnoreMoveInput(true);
		FlushPressedKeys();

		if (APawn* ControlledPawn = GetPawn())
		{
			ControlledPawn->ConsumeMovementInputVector();
			if (UPawnMovementComponent* MovementComponent = ControlledPawn->GetMovementComponent())
			{
				MovementComponent->StopMovementImmediately();
			}
		}

		bShowMouseCursor = true;
		FInputModeUIOnly InputMode;
		if (UWidget* InitialFocusTarget = UpgradeSelectionWidget->GetInitialFocusTarget())
		{
			InputMode.SetWidgetToFocus(InitialFocusTarget->TakeWidget());
		}
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
	}
	else
	{
		// 恢复游戏输入前再次清键，避免 UI 期间松开的按键在 Enhanced Input 中保持按下状态。
		FlushPressedKeys();
		SetIgnoreMoveInput(false);
		SetThirdPersonInputMode(bThirdPersonInputMode);
	}
}

// PlayerState 升级状态变化时刷新候选内容和本地输入模式。
void AArenaPlayerController::HandleUpgradeStateChanged()
{
	RefreshUpgradeSelectionUI();
}

// 把 Widget 选择转换为候选 ID 请求，不在客户端应用任何升级结果。
void AArenaPlayerController::HandleUpgradeChosen(FName UpgradeID)
{
	if (!UpgradeID.IsNone())
	{
		ServerSelectUpgrade(UpgradeID);
	}
}

// 服务器 RPC 将选择交给 GameMode 做阶段、候选、标签和层数验证。
void AArenaPlayerController::ServerSelectUpgrade_Implementation(FName UpgradeID)
{
	if (AArenaGameMode* ArenaGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AArenaGameMode>() : nullptr)
	{
		ArenaGameMode->SubmitUpgradeSelection(this, UpgradeID);
	}
}

// Controller 销毁前解除升级、GameState 和 Widget 委托，并停止 HUD 绑定重试。
void AArenaPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UpgradeSelectionWidget)
	{
		UpgradeSelectionWidget->OnUpgradeChosen.RemoveDynamic(this, &AArenaPlayerController::HandleUpgradeChosen);
	}
	UnbindUpgradeState();
	UnbindGameStateHUD();
	ClearPlayerHUDBindingRetry();
	Super::EndPlay(EndPlayReason);
}

// 切换双视角鼠标与准星状态；升级 UI 激活时保留可点击鼠标并延后恢复游戏输入。
void AArenaPlayerController::SetThirdPersonInputMode(bool bEnableThirdPerson)
{
	if (!IsLocalController())
	{
		return;
	}

	bThirdPersonInputMode = bEnableThirdPerson;
	if (bUpgradeInputMode)
	{
		bShowMouseCursor = true;
		return;
	}
	bShowMouseCursor = !bThirdPersonInputMode;

	FInputModeGameOnly InputMode;
	// 顶视角保留第一次鼠标点击，第三人称由 GameOnly 模式持续捕获鼠标增量。
	InputMode.SetConsumeCaptureMouseDown(false);
	SetInputMode(InputMode);

	if (!bThirdPersonInputMode)
	{
		int32 ViewportSizeX = 0;
		int32 ViewportSizeY = 0;
		GetViewportSize(ViewportSizeX, ViewportSizeY);
		if (ViewportSizeX > 0 && ViewportSizeY > 0)
		{
			SetMouseLocation(ViewportSizeX / 2, ViewportSizeY / 2);
		}
	}

	if (PlayerHUDWidget)
	{
		PlayerHUDWidget->SetThirdPersonReticleVisible(bThirdPersonInputMode);
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
	BindGameStateHUD();
	ClearPlayerHUDBindingRetry();
}

// 绑定 GameState 的复制变化并先用当前快照刷新一次 HUD。
void AArenaPlayerController::BindGameStateHUD()
{
	if (!IsLocalController() || !PlayerHUDWidget)
	{
		return;
	}

	AArenaGameState* ArenaGameState = GetWorld() ? GetWorld()->GetGameState<AArenaGameState>() : nullptr;
	if (!ArenaGameState)
	{
		return;
	}

	if (BoundArenaGameState.Get() != ArenaGameState)
	{
		UnbindGameStateHUD();
		BoundArenaGameState = ArenaGameState;
		ArenaGameState->OnGamePhaseChanged.AddUniqueDynamic(this, &AArenaPlayerController::HandleGamePhaseChanged);
		ArenaGameState->OnCurrentWaveIndexChanged.AddUniqueDynamic(this, &AArenaPlayerController::HandleWaveIndexChanged);
		ArenaGameState->OnRemainingEnemyCountChanged.AddUniqueDynamic(this, &AArenaPlayerController::HandleRemainingEnemyCountChanged);
		ArenaGameState->OnUpgradeRandomSeedChanged.AddUniqueDynamic(this, &AArenaPlayerController::HandleUpgradeRandomSeedChanged);
	}

	PlayerHUDWidget->SetGamePhase(ArenaGameState->GetGamePhase());
	PlayerHUDWidget->SetWaveState(ArenaGameState->GetCurrentWaveIndex(), ArenaGameState->GetRemainingEnemyCount());
	PlayerHUDWidget->SetUpgradeRandomSeed(ArenaGameState->GetUpgradeRandomSeed());
}

// 解除 GameState 阶段、波次和随机种子委托，防止世界切换后引用旧状态对象。
void AArenaPlayerController::UnbindGameStateHUD()
{
	if (AArenaGameState* ArenaGameState = BoundArenaGameState.Get())
	{
		ArenaGameState->OnGamePhaseChanged.RemoveDynamic(this, &AArenaPlayerController::HandleGamePhaseChanged);
		ArenaGameState->OnCurrentWaveIndexChanged.RemoveDynamic(this, &AArenaPlayerController::HandleWaveIndexChanged);
		ArenaGameState->OnRemainingEnemyCountChanged.RemoveDynamic(this, &AArenaPlayerController::HandleRemainingEnemyCountChanged);
		ArenaGameState->OnUpgradeRandomSeedChanged.RemoveDynamic(this, &AArenaPlayerController::HandleUpgradeRandomSeedChanged);
	}
	BoundArenaGameState.Reset();
}

// 阶段变化时同步 HUD，并驱动 Upgrade 界面的显示或关闭。
void AArenaPlayerController::HandleGamePhaseChanged(EArenaGamePhase OldPhase, EArenaGamePhase NewPhase)
{
	if (PlayerHUDWidget)
	{
		PlayerHUDWidget->SetGamePhase(NewPhase);
	}
	RefreshUpgradeSelectionUI();
}

// 当前波次复制变化时使用同一 GameState 快照刷新 HUD。
void AArenaPlayerController::HandleWaveIndexChanged(int32 OldValue, int32 NewValue)
{
	if (PlayerHUDWidget)
	{
		const AArenaGameState* ArenaGameState = BoundArenaGameState.Get();
		PlayerHUDWidget->SetWaveState(NewValue, ArenaGameState ? ArenaGameState->GetRemainingEnemyCount() : 0);
	}
}

// 剩余敌人数复制变化时使用同一 GameState 快照刷新 HUD。
void AArenaPlayerController::HandleRemainingEnemyCountChanged(int32 OldValue, int32 NewValue)
{
	if (PlayerHUDWidget)
	{
		const AArenaGameState* ArenaGameState = BoundArenaGameState.Get();
		PlayerHUDWidget->SetWaveState(ArenaGameState ? ArenaGameState->GetCurrentWaveIndex() : 0, NewValue);
	}
}

// 随机种子复制完成后刷新右上角显示，客户端不会使用该值自行抽取候选。
void AArenaPlayerController::HandleUpgradeRandomSeedChanged(int32 OldValue, int32 NewValue)
{
	if (PlayerHUDWidget)
	{
		PlayerHUDWidget->SetUpgradeRandomSeed(NewValue);
	}
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
