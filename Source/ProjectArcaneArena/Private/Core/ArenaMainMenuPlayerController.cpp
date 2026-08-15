#include "Core/ArenaMainMenuPlayerController.h"

#include "Components/Widget.h"
#include "Core/ArenaDirectConnectSubsystem.h"
#include "Core/ArenaLobbyPlayerState.h"
#include "Core/ArenaMainMenuGameMode.h"
#include "Core/ArenaMainMenuGameState.h"
#include "Core/ArenaLobbyTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/PackageName.h"
#include "TimerManager.h"
#include "UI/ArenaMainMenuWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogArenaMainMenu, Log, All);

// 配置无需 Pawn 的本地菜单 Controller 默认 View 和正式战斗地图。
AArenaMainMenuPlayerController::AArenaMainMenuPlayerController()
{
	MainMenuWidgetClass = UArenaMainMenuWidget::StaticClass();
	GameplayMapName = TEXT("/Game/TopDown/Lvl_TopDown");
}

// 菜单只属于本地表现；下一 Tick 刷新允许 GameState 与 PlayerState 完成初始复制。
void AArenaMainMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	CreateMainMenu();
	if (UArenaDirectConnectSubsystem* DirectConnect = GetDirectConnectSubsystem())
	{
		DirectConnect->OnConnectionStateChanged.AddUniqueDynamic(
			this,
			&AArenaMainMenuPlayerController::HandleConnectionStateChanged);
	}
	GetWorldTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &AArenaMainMenuPlayerController::RefreshLobbyView));
}

// Lobby PlayerState 到达后重新绑定个人状态，避免 BeginPlay 时本地成员尚未复制。
void AArenaMainMenuPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	if (IsLocalController())
	{
		RefreshLobbyView();
	}
}

// Host 的优雅退出通知写入跨关卡 Subsystem；缺少 Subsystem 时回退引擎默认行为。
void AArenaMainMenuPlayerController::ClientReturnToMainMenuWithTextReason_Implementation(const FText& ReturnReason)
{
	if (HasAuthority() && IsLocalController())
	{
		Super::ClientReturnToMainMenuWithTextReason_Implementation(ReturnReason);
		return;
	}

	if (UArenaDirectConnectSubsystem* DirectConnect = GetDirectConnectSubsystem())
	{
		DirectConnect->HandleHostClosedLobby(ReturnReason);
		return;
	}

	Super::ClientReturnToMainMenuWithTextReason_Implementation(ReturnReason);
}

// 对称清理菜单和全部动态委托，避免 Seamless Travel 后旧菜单 Controller 继续刷新 View。
void AArenaMainMenuPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (BoundLobbyGameState.IsValid())
	{
		BoundLobbyGameState->OnLobbyStateChanged.RemoveDynamic(this, &AArenaMainMenuPlayerController::RefreshLobbyView);
	}
	BoundLobbyGameState.Reset();
	for (const TWeakObjectPtr<AArenaLobbyPlayerState>& BoundPlayerState : BoundLobbyPlayerStates)
	{
		if (BoundPlayerState.IsValid())
		{
			BoundPlayerState->OnLobbyPlayerStateChanged.RemoveDynamic(this, &AArenaMainMenuPlayerController::RefreshLobbyView);
		}
	}
	BoundLobbyPlayerStates.Reset();

	if (UArenaDirectConnectSubsystem* DirectConnect = GetDirectConnectSubsystem())
	{
		DirectConnect->OnConnectionStateChanged.RemoveDynamic(
			this,
			&AArenaMainMenuPlayerController::HandleConnectionStateChanged);
	}

	if (MainMenuWidget)
	{
		MainMenuWidget->OnSinglePlayerRequested.RemoveDynamic(this, &AArenaMainMenuPlayerController::HandleSinglePlayerRequested);
		MainMenuWidget->OnMultiplayerRequested.RemoveDynamic(this, &AArenaMainMenuPlayerController::HandleMultiplayerRequested);
		MainMenuWidget->OnHostLobbyRequested.RemoveDynamic(this, &AArenaMainMenuPlayerController::HandleHostLobbyRequested);
		MainMenuWidget->OnJoinLobbyRequested.RemoveDynamic(this, &AArenaMainMenuPlayerController::HandleJoinLobbyRequested);
		MainMenuWidget->OnBackRequested.RemoveDynamic(this, &AArenaMainMenuPlayerController::HandleBackRequested);
		MainMenuWidget->OnLobbyReadyRequested.RemoveDynamic(this, &AArenaMainMenuPlayerController::HandleLobbyReadyRequested);
		MainMenuWidget->OnLobbyStartRequested.RemoveDynamic(this, &AArenaMainMenuPlayerController::HandleLobbyStartRequested);
		MainMenuWidget->OnLobbyLeaveRequested.RemoveDynamic(this, &AArenaMainMenuPlayerController::HandleLobbyLeaveRequested);
		MainMenuWidget->OnQuitGameRequested.RemoveDynamic(this, &AArenaMainMenuPlayerController::HandleQuitGameRequested);
		MainMenuWidget->RemoveFromParent();
		MainMenuWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

// 创建菜单并把鼠标、点击和键盘焦点统一交给 View，不启动任何战斗系统。
void AArenaMainMenuPlayerController::CreateMainMenu()
{
	if (MainMenuWidget || !IsLocalController())
	{
		return;
	}

	TSubclassOf<UArenaMainMenuWidget> ResolvedWidgetClass = MainMenuWidgetClass;
	if (!ResolvedWidgetClass)
	{
		ResolvedWidgetClass = UArenaMainMenuWidget::StaticClass();
	}
	MainMenuWidget = CreateWidget<UArenaMainMenuWidget>(this, ResolvedWidgetClass);
	if (!MainMenuWidget)
	{
		UE_LOG(LogArenaMainMenu, Error, TEXT("Failed to create the main menu widget from %s."), *GetNameSafe(ResolvedWidgetClass.Get()));
		return;
	}

	MainMenuWidget->OnSinglePlayerRequested.AddUniqueDynamic(this, &AArenaMainMenuPlayerController::HandleSinglePlayerRequested);
	MainMenuWidget->OnMultiplayerRequested.AddUniqueDynamic(this, &AArenaMainMenuPlayerController::HandleMultiplayerRequested);
	MainMenuWidget->OnHostLobbyRequested.AddUniqueDynamic(this, &AArenaMainMenuPlayerController::HandleHostLobbyRequested);
	MainMenuWidget->OnJoinLobbyRequested.AddUniqueDynamic(this, &AArenaMainMenuPlayerController::HandleJoinLobbyRequested);
	MainMenuWidget->OnBackRequested.AddUniqueDynamic(this, &AArenaMainMenuPlayerController::HandleBackRequested);
	MainMenuWidget->OnLobbyReadyRequested.AddUniqueDynamic(this, &AArenaMainMenuPlayerController::HandleLobbyReadyRequested);
	MainMenuWidget->OnLobbyStartRequested.AddUniqueDynamic(this, &AArenaMainMenuPlayerController::HandleLobbyStartRequested);
	MainMenuWidget->OnLobbyLeaveRequested.AddUniqueDynamic(this, &AArenaMainMenuPlayerController::HandleLobbyLeaveRequested);
	MainMenuWidget->OnQuitGameRequested.AddUniqueDynamic(this, &AArenaMainMenuPlayerController::HandleQuitGameRequested);
	MainMenuWidget->AddToViewport(1000);

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	ApplyMenuInputFocus();
}

// 从复制对象生成稳定排序的纯 ViewData，并为后续成员变化补齐 PlayerState 委托。
void AArenaMainMenuPlayerController::RefreshLobbyView()
{
	if (!IsLocalController() || !MainMenuWidget)
	{
		return;
	}

	AArenaMainMenuGameState* MenuGameState = GetWorld() ? GetWorld()->GetGameState<AArenaMainMenuGameState>() : nullptr;
	if (BoundLobbyGameState.Get() != MenuGameState)
	{
		if (BoundLobbyGameState.IsValid())
		{
			BoundLobbyGameState->OnLobbyStateChanged.RemoveDynamic(this, &AArenaMainMenuPlayerController::RefreshLobbyView);
		}
		BoundLobbyGameState = MenuGameState;
		if (MenuGameState)
		{
			MenuGameState->OnLobbyStateChanged.AddUniqueDynamic(this, &AArenaMainMenuPlayerController::RefreshLobbyView);
		}
	}

	if (!MenuGameState || !MenuGameState->IsLobbyActive())
	{
		MainMenuWidget->ShowFrontPage();
		if (UArenaDirectConnectSubsystem* DirectConnect = GetDirectConnectSubsystem())
		{
			MainMenuWidget->SetErrorMessage(DirectConnect->ConsumePendingError());
		}
		ApplyMenuInputFocus();
		return;
	}

	if (UArenaDirectConnectSubsystem* DirectConnect = GetDirectConnectSubsystem())
	{
		DirectConnect->NotifyEnteredLobby();
		if (MenuGameState->IsTravelStarting())
		{
			DirectConnect->NotifyMatchTravelStarting();
		}
	}
	if (MenuGameState->IsTravelStarting())
	{
		MainMenuWidget->ShowConnectingPage(NSLOCTEXT("ArenaMainMenu", "EnteringMatch", "正在进入战斗..."));
		return;
	}

	for (const TWeakObjectPtr<AArenaLobbyPlayerState>& OldPlayerState : BoundLobbyPlayerStates)
	{
		if (OldPlayerState.IsValid() && !MenuGameState->PlayerArray.Contains(OldPlayerState.Get()))
		{
			OldPlayerState->OnLobbyPlayerStateChanged.RemoveDynamic(this, &AArenaMainMenuPlayerController::RefreshLobbyView);
		}
	}
	BoundLobbyPlayerStates.Reset();

	const AArenaLobbyPlayerState* LocalLobbyPlayerState = GetPlayerState<AArenaLobbyPlayerState>();
	FArenaLobbyViewData ViewData;
	ViewData.MaxPlayers = MenuGameState->GetLobbyMaxPlayers();
	ViewData.bTravelStarting = MenuGameState->IsTravelStarting();
	for (APlayerState* MenuPlayerState : MenuGameState->PlayerArray)
	{
		AArenaLobbyPlayerState* LobbyPlayerState = Cast<AArenaLobbyPlayerState>(MenuPlayerState);
		if (!LobbyPlayerState)
		{
			continue;
		}

		LobbyPlayerState->OnLobbyPlayerStateChanged.AddUniqueDynamic(this, &AArenaMainMenuPlayerController::RefreshLobbyView);
		BoundLobbyPlayerStates.Add(LobbyPlayerState);
		FArenaLobbyPlayerViewData PlayerView;
		PlayerView.PlayerId = LobbyPlayerState->GetPlayerId();
		PlayerView.DisplayName = FText::FromString(
			LobbyPlayerState->GetPlayerName().IsEmpty()
				? FString::Printf(TEXT("Player %d"), LobbyPlayerState->GetPlayerId())
				: LobbyPlayerState->GetPlayerName());
		PlayerView.bIsHost = LobbyPlayerState->IsLobbyHost();
		PlayerView.bIsReady = LobbyPlayerState->IsLobbyReady();
		PlayerView.bIsLocalPlayer = LobbyPlayerState == LocalLobbyPlayerState;
		ViewData.Players.Add(PlayerView);
	}
	ViewData.Players.Sort(
		[](const FArenaLobbyPlayerViewData& Left, const FArenaLobbyPlayerViewData& Right)
		{
			if (Left.bIsHost != Right.bIsHost)
			{
				return Left.bIsHost;
			}
			return Left.PlayerId < Right.PlayerId;
		});

	ViewData.bLocalPlayerIsHost = LocalLobbyPlayerState && LocalLobbyPlayerState->IsLobbyHost();
	ViewData.bLocalPlayerIsReady = LocalLobbyPlayerState && LocalLobbyPlayerState->IsLobbyReady();
	ViewData.bCanHostStart = ViewData.bLocalPlayerIsHost
		&& ViewData.Players.Num() >= 2
		&& !ViewData.bTravelStarting;
	for (const FArenaLobbyPlayerViewData& PlayerViewData : ViewData.Players)
	{
		if (!PlayerViewData.bIsHost && !PlayerViewData.bIsReady)
		{
			ViewData.bCanHostStart = false;
			break;
		}
	}

	MainMenuWidget->SetErrorMessage(FText::GetEmpty());
	MainMenuWidget->ShowLobbyPage(ViewData);
	ApplyMenuInputFocus();
}

// UIOnly 焦点始终指向可聚焦按钮，避免根 SObjectWidget 触发 Non-Focusable 警告。
void AArenaMainMenuPlayerController::ApplyMenuInputFocus()
{
	if (!IsLocalController() || !MainMenuWidget)
	{
		return;
	}

	UWidget* FocusTarget = MainMenuWidget->GetInitialFocusTarget();
	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetWidgetToFocus((FocusTarget ? FocusTarget : MainMenuWidget.Get())->TakeWidget());
	SetInputMode(InputMode);
	if (FocusTarget)
	{
		FocusTarget->SetUserFocus(this);
	}
}

// 单人旅行保持无 Listen 参数，且只接受一次终止当前菜单的请求。
void AArenaMainMenuPlayerController::HandleSinglePlayerRequested()
{
	if (bLocalTerminalRequestStarted || GameplayMapName.IsNone())
	{
		return;
	}

	const FString GameplayPackageName = GameplayMapName.ToString();
	if (!FPackageName::DoesPackageExist(GameplayPackageName))
	{
		UE_LOG(LogArenaMainMenu, Error, TEXT("Gameplay map package does not exist: %s"), *GameplayPackageName);
		MainMenuWidget->SetErrorMessage(NSLOCTEXT("ArenaMainMenu", "MissingGameplayMap", "正式战斗地图不存在。"));
		return;
	}

	bLocalTerminalRequestStarted = true;
	MainMenuWidget->SetMenuInteractionEnabled(false);
	UGameplayStatics::OpenLevel(this, GameplayMapName, true);
}

// 多人入口只切换页面，便于玩家选择 Host 容量或输入 Direct IP。
void AArenaMainMenuPlayerController::HandleMultiplayerRequested()
{
	if (MainMenuWidget)
	{
		MainMenuWidget->SetErrorMessage(FText::GetEmpty());
		MainMenuWidget->ShowMultiplayerSetupPage();
		ApplyMenuInputFocus();
	}
}

// Host 请求成功后立即显示创建状态；失败则留在设置页展示 Subsystem 的精确错误。
void AArenaMainMenuPlayerController::HandleHostLobbyRequested(const int32 MaxPlayers)
{
	UArenaDirectConnectSubsystem* DirectConnect = GetDirectConnectSubsystem();
	if (!DirectConnect || !MainMenuWidget)
	{
		return;
	}

	if (DirectConnect->HostLobby(MaxPlayers))
	{
		MainMenuWidget->ShowConnectingPage(NSLOCTEXT("ArenaMainMenu", "CreatingLobby", "正在创建房间..."));
	}
	else
	{
		MainMenuWidget->ShowMultiplayerSetupPage();
		MainMenuWidget->SetErrorMessage(DirectConnect->ConsumePendingError());
		ApplyMenuInputFocus();
	}
}

// Join 请求成功后等待网络结果；地址错误不会触发旅行并可立即修正重试。
void AArenaMainMenuPlayerController::HandleJoinLobbyRequested(const FString& Address)
{
	UArenaDirectConnectSubsystem* DirectConnect = GetDirectConnectSubsystem();
	if (!DirectConnect || !MainMenuWidget)
	{
		return;
	}

	if (DirectConnect->JoinLobby(Address))
	{
		MainMenuWidget->ShowConnectingPage(NSLOCTEXT("ArenaMainMenu", "JoiningLobby", "正在加入房间..."));
	}
	else
	{
		MainMenuWidget->ShowMultiplayerSetupPage();
		MainMenuWidget->SetErrorMessage(DirectConnect->ConsumePendingError());
		ApplyMenuInputFocus();
	}
}

// 返回首页只清理本地提示与焦点，不影响尚未建立的任何网络连接。
void AArenaMainMenuPlayerController::HandleBackRequested()
{
	if (!MainMenuWidget)
	{
		return;
	}
	if (UArenaDirectConnectSubsystem* DirectConnect = GetDirectConnectSubsystem())
	{
		DirectConnect->ConsumePendingError();
	}
	MainMenuWidget->SetErrorMessage(FText::GetEmpty());
	MainMenuWidget->ShowFrontPage();
	ApplyMenuInputFocus();
}

// Ready RPC 始终发送目标状态，服务器 PlayerState 复制是唯一显示确认来源。
void AArenaMainMenuPlayerController::HandleLobbyReadyRequested(const bool bReady)
{
	ServerSetLobbyReady(bReady);
}

// Host Start RPC 不在客户端预先锁定规则，等待服务器设置 bTravelStarting。
void AArenaMainMenuPlayerController::HandleLobbyStartRequested()
{
	ServerStartLobbyMatch();
}

// Subsystem 会让 Listen Host 通知全房返回，而 Client 只断开自己的连接。
void AArenaMainMenuPlayerController::HandleLobbyLeaveRequested()
{
	if (UArenaDirectConnectSubsystem* DirectConnect = GetDirectConnectSubsystem())
	{
		DirectConnect->LeaveNetworkGame();
	}
}

// 防止重复请求后调用统一退出接口；PIE 会结束会话，Standalone 会关闭游戏窗口。
void AArenaMainMenuPlayerController::HandleQuitGameRequested()
{
	if (bLocalTerminalRequestStarted)
	{
		return;
	}

	bLocalTerminalRequestStarted = true;
	if (MainMenuWidget)
	{
		MainMenuWidget->SetMenuInteractionEnabled(false);
	}
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

// 状态机只即时展示非失败中的长操作；失败会重建菜单并消费跨关卡错误。
void AArenaMainMenuPlayerController::HandleConnectionStateChanged(
	const EArenaDirectConnectState OldState,
	const EArenaDirectConnectState NewState)
{
	(void)OldState;
	if (!MainMenuWidget)
	{
		return;
	}

	if (NewState == EArenaDirectConnectState::Hosting)
	{
		MainMenuWidget->ShowConnectingPage(NSLOCTEXT("ArenaMainMenu", "CreatingLobby", "正在创建房间..."));
	}
	else if (NewState == EArenaDirectConnectState::Joining)
	{
		MainMenuWidget->ShowConnectingPage(NSLOCTEXT("ArenaMainMenu", "JoiningLobby", "正在加入房间..."));
	}
	else if (NewState == EArenaDirectConnectState::Traveling)
	{
		MainMenuWidget->ShowConnectingPage(NSLOCTEXT("ArenaMainMenu", "EnteringMatch", "正在进入战斗..."));
	}
}

// GameInstanceSubsystem 跨菜单和正式地图存在，Controller 不缓存其 UObject 生命周期。
UArenaDirectConnectSubsystem* AArenaMainMenuPlayerController::GetDirectConnectSubsystem() const
{
	UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UArenaDirectConnectSubsystem>() : nullptr;
}

// Server RPC 只转发给当前菜单 GameMode，所有身份和阶段规则在权威侧重验。
void AArenaMainMenuPlayerController::ServerSetLobbyReady_Implementation(const bool bReady)
{
	if (AArenaMainMenuGameMode* MenuGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AArenaMainMenuGameMode>() : nullptr)
	{
		MenuGameMode->SetLobbyPlayerReady(this, bReady);
	}
}

// Server RPC 只转发开始意图，GameMode 决定是否锁定并执行 Seamless Travel。
void AArenaMainMenuPlayerController::ServerStartLobbyMatch_Implementation()
{
	if (AArenaMainMenuGameMode* MenuGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AArenaMainMenuGameMode>() : nullptr)
	{
		MenuGameMode->TryStartLobbyMatch(this);
	}
}
