#include "Core/ArenaPlayerController.h"

#include "Camera/CameraActor.h"
#include "Character/ArenaPlayerCharacter.h"
#include "Character/ArenaBossCharacter.h"
#include "Core/ArenaGameMode.h"
#include "Core/ArenaPlayerState.h"
#include "Core/ArenaGameState.h"
#include "Core/ArenaUpgradeDataAsset.h"
#include "Components/Widget.h"
#include "EngineUtils.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Math/RotationMatrix.h"
#include "UI/ArenaPlayerHUDWidget.h"
#include "UI/ArenaUpgradeSelectionWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogArenaBossIntro, Log, All);

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

// 使用本地相机朝向把世界伤害来源转换为屏幕角度，远程玩家不会调用该入口。
void AArenaPlayerController::ShowLocalDamageFeedback(
	const FArenaDamageFeedbackData& DamageFeedback,
	float FeedbackIntensity)
{
	if (!IsLocalController() || !PlayerHUDWidget)
	{
		return;
	}

	const APawn* ControlledPawn = GetPawn();
	bool bHasDirection = DamageFeedback.bHasDamageSourceLocation && ControlledPawn;
	float DirectionAngleDegrees = 0.0f;
	if (bHasDirection)
	{
		FVector ToDamageSource = FVector(DamageFeedback.DamageSourceLocation) - ControlledPawn->GetActorLocation();
		ToDamageSource.Z = 0.0f;
		bHasDirection = ToDamageSource.Normalize();
		if (bHasDirection)
		{
			const FRotator ViewRotation = PlayerCameraManager
				? PlayerCameraManager->GetCameraRotation()
				: GetControlRotation();
			const FRotationMatrix ViewYawRotation(FRotator(0.0f, ViewRotation.Yaw, 0.0f));
			const FVector ViewForward = ViewYawRotation.GetUnitAxis(EAxis::X);
			const FVector ViewRight = ViewYawRotation.GetUnitAxis(EAxis::Y);
			DirectionAngleDegrees = FMath::RadiansToDegrees(FMath::Atan2(
				FVector::DotProduct(ToDamageSource, ViewRight),
				FVector::DotProduct(ToDamageSource, ViewForward)));
		}
	}

	PlayerHUDWidget->ShowDamageFeedback(
		DirectionAngleDegrees,
		bHasDirection,
		FMath::Max(FeedbackIntensity, 0.0f),
		DamageFeedback.FeedbackType);
}

// 开始时为本地端创建界面，并在本地或 Authority 端绑定所需的 GameState 阶段数据。
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

// Controller 销毁前恢复 Intro 本地状态并解除升级、GameState、Widget 与服务器 Hold Timer。
void AArenaPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UpgradeSelectionWidget)
	{
		UpgradeSelectionWidget->OnUpgradeChosen.RemoveDynamic(this, &AArenaPlayerController::HandleUpgradeChosen);
	}
	FinishBossIntroPresentation(true);
	ClearBossIntroSkipHold();
	UnbindUpgradeState();
	UnbindGameStateHUD();
	ClearPlayerHUDBindingRetry();
	Super::EndPlay(EndPlayReason);
}

// 切换双视角鼠标与准星状态；升级或 Intro 激活时保留其专属输入表现并延后恢复。
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
	if (bBossIntroInputMode)
	{
		bShowMouseCursor = false;
		if (PlayerHUDWidget)
		{
			PlayerHUDWidget->SetThirdPersonReticleVisible(false);
		}
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

// 本地按住状态用于 HUD 进度；服务器收到独立状态后自行计满两秒，客户端时间不决定跳过。
void AArenaPlayerController::SetBossIntroSkipHeld(bool bHeld)
{
	if (!IsLocalController())
	{
		return;
	}

	const AArenaGameState* ArenaGameState = BoundArenaGameState.IsValid()
		? BoundArenaGameState.Get()
		: (GetWorld() ? GetWorld()->GetGameState<AArenaGameState>() : nullptr);
	if (bHeld)
	{
		if (bBossIntroSkipHeldLocally
			|| !ArenaGameState
			|| ArenaGameState->GetGamePhase() != EArenaGamePhase::BossIntro)
		{
			return;
		}

		bBossIntroSkipHeldLocally = true;
		LocalBossIntroSkipHoldStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		ServerSetBossIntroSkipHeld(true);
	}
	else if (bBossIntroSkipHeldLocally)
	{
		bBossIntroSkipHeldLocally = false;
		LocalBossIntroSkipHoldStartTime = 0.0f;
		ServerSetBossIntroSkipHeld(false);
	}
}

// 服务器只接受 BossIntro 中首次按下，重复 RPC 不会重置计时器或缩短 Hold 要求。
void AArenaPlayerController::ServerSetBossIntroSkipHeld_Implementation(bool bHeld)
{
	if (!bHeld)
	{
		ClearBossIntroSkipHold();
		return;
	}

	const AArenaGameState* ArenaGameState = GetWorld() ? GetWorld()->GetGameState<AArenaGameState>() : nullptr;
	if (bBossIntroSkipHeldOnServer
		|| !ArenaGameState
		|| ArenaGameState->GetGamePhase() != EArenaGamePhase::BossIntro
		|| !ArenaGameState->GetActiveBoss())
	{
		return;
	}

	bBossIntroSkipHeldOnServer = true;
	GetWorldTimerManager().SetTimer(
		BossIntroSkipHoldTimerHandle,
		this,
		&AArenaPlayerController::CompleteBossIntroSkipHold,
		FMath::Max(BossIntroSkipHoldDuration, 0.1f),
		false);
}

// 服务器 Hold 完成时让 GameMode/WaveManager 重验参战者、阶段与 Boss 存活状态。
void AArenaPlayerController::CompleteBossIntroSkipHold()
{
	const bool bCompletedValidHold = bBossIntroSkipHeldOnServer;
	ClearBossIntroSkipHold();
	if (!bCompletedValidHold)
	{
		return;
	}

	if (AArenaGameMode* ArenaGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AArenaGameMode>() : nullptr)
	{
		ArenaGameMode->RequestBossIntroSkip(this);
	}
}

// 松开、自然结束或销毁时清理服务器计时器，防止迟到跳过改变下一阶段。
void AArenaPlayerController::ClearBossIntroSkipHold()
{
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(BossIntroSkipHoldTimerHandle);
	}
	bBossIntroSkipHeldOnServer = false;
}

// 组合复制阶段、Boss 与时序判断 Intro 是否真正就绪，容忍各属性 OnRep 到达顺序不同。
void AArenaPlayerController::RefreshBossIntroPresentation()
{
	if (!IsLocalController())
	{
		return;
	}

	AArenaGameState* ArenaGameState = BoundArenaGameState.Get();
	AArenaBossCharacter* Boss = ArenaGameState ? ArenaGameState->GetActiveBoss() : nullptr;
	const FArenaBossIntroTiming Timing = ArenaGameState
		? ArenaGameState->GetBossIntroTiming()
		: FArenaBossIntroTiming();
	const bool bIntroReady = ArenaGameState
		&& ArenaGameState->GetGamePhase() == EArenaGamePhase::BossIntro
		&& Boss
		&& Timing.EndServerTimeSeconds > ArenaGameState->GetServerWorldTimeSeconds();

	if (bIntroReady)
	{
		if (!bBossIntroInputMode)
		{
			StartBossIntroPresentation(Boss, Timing);
		}
		UpdateBossIntroPresentation();
	}
	else if (bBossIntroInputMode)
	{
		const bool bInterrupted = !ArenaGameState || ArenaGameState->GetGamePhase() != EArenaGamePhase::Combat;
		FinishBossIntroPresentation(bInterrupted);
	}
}

// 启动每个客户端独立的镜头、输入和 HUD 表现，CameraActor 的 Transform 从不参与复制。
void AArenaPlayerController::StartBossIntroPresentation(
	AArenaBossCharacter* Boss,
	const FArenaBossIntroTiming& Timing)
{
	if (!IsLocalController() || bBossIntroInputMode || !Boss)
	{
		return;
	}

	bBossIntroInputMode = true;
	bBossIntroCameraBlendingOut = false;
	bBossIntroSkipHeldLocally = false;
	LocalBossIntroSkipHoldStartTime = 0.0f;
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
	FlushPressedKeys();
	bShowMouseCursor = false;
	if (PlayerHUDWidget)
	{
		PlayerHUDWidget->SetThirdPersonReticleVisible(false);
	}

	ActiveBossIntroCamera = FindBossIntroCamera(Boss);
	if (ACameraActor* IntroCamera = ActiveBossIntroCamera.Get())
	{
		SetViewTargetWithBlend(
			IntroCamera,
			FMath::Max(BossIntroCameraBlendDuration, 0.0f),
			EViewTargetBlendFunction::VTBlend_Cubic);
	}
	else if (!bWarnedMissingBossIntroCamera)
	{
		bWarnedMissingBossIntroCamera = true;
		UE_LOG(LogArenaBossIntro, Warning,
			TEXT("No CameraActor tagged %s was found for Boss %s; Intro will keep the current player camera."),
			*BossIntroCameraActorTag.ToString(),
			*GetNameSafe(Boss));
	}

	GetWorldTimerManager().SetTimer(
		BossIntroPresentationTimerHandle,
		this,
		&AArenaPlayerController::UpdateBossIntroPresentation,
		FMath::Max(BossIntroPresentationTickInterval, 0.01f),
		true);
	K2_OnBossIntroStarted(Boss, FMath::Max(
		Timing.EndServerTimeSeconds - (BoundArenaGameState.IsValid()
			? BoundArenaGameState->GetServerWorldTimeSeconds()
			: 0.0f),
		0.0f));
}

// 使用同步服务器时间更新尾段回切和 Hold 进度，避免不同客户端本地帧率改变 Intro 时序。
void AArenaPlayerController::UpdateBossIntroPresentation()
{
	if (!bBossIntroInputMode)
	{
		return;
	}

	AArenaGameState* ArenaGameState = BoundArenaGameState.Get();
	AArenaBossCharacter* Boss = ArenaGameState ? ArenaGameState->GetActiveBoss() : nullptr;
	if (!ArenaGameState
		|| ArenaGameState->GetGamePhase() != EArenaGamePhase::BossIntro
		|| !Boss)
	{
		FinishBossIntroPresentation(true);
		return;
	}

	const FArenaBossIntroTiming Timing = ArenaGameState->GetBossIntroTiming();
	const float RemainingTime = ArenaGameState->GetBossIntroRemainingTime();
	if (RemainingTime <= Timing.BlendOutDuration + KINDA_SMALL_NUMBER)
	{
		BeginBossIntroCameraBlendOut(Timing.BlendOutDuration);
	}

	float SkipProgress = 0.0f;
	if (bBossIntroSkipHeldLocally && GetWorld())
	{
		SkipProgress = FMath::Clamp(
			(GetWorld()->GetTimeSeconds() - LocalBossIntroSkipHoldStartTime)
				/ FMath::Max(BossIntroSkipHoldDuration, 0.1f),
			0.0f,
			1.0f);
	}
	if (PlayerHUDWidget)
	{
		PlayerHUDWidget->SetBossIntroPresentation(
			true,
			Boss->GetBossDisplayName(),
			RemainingTime,
			SkipProgress);
	}
}

// 尾段只执行一次 ViewTarget 回切，控制仍保持冻结直到服务器阶段真正进入 Combat。
void AArenaPlayerController::BeginBossIntroCameraBlendOut(float BlendOutDuration)
{
	if (!bBossIntroInputMode || bBossIntroCameraBlendingOut)
	{
		return;
	}

	bBossIntroCameraBlendingOut = true;
	if (APawn* ControlledPawn = GetPawn())
	{
		SetViewTargetWithBlend(
			ControlledPawn,
			FMath::Max(BlendOutDuration, 0.0f),
			EViewTargetBlendFunction::VTBlend_Cubic);
	}
}

// 正常 Combat 与异常终局共用恢复入口，保证鼠标、准星、ViewTarget 和 Hold 状态不残留。
void AArenaPlayerController::FinishBossIntroPresentation(bool bWasInterrupted)
{
	if (!bBossIntroInputMode && !bBossIntroSkipHeldLocally)
	{
		return;
	}

	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(BossIntroPresentationTimerHandle);
	}
	if (bBossIntroSkipHeldLocally)
	{
		SetBossIntroSkipHeld(false);
	}
	ClearBossIntroSkipHold();

	if (!bBossIntroCameraBlendingOut)
	{
		if (APawn* ControlledPawn = GetPawn())
		{
			SetViewTargetWithBlend(
				ControlledPawn,
				FMath::Max(BossIntroCameraBlendDuration, 0.0f),
				EViewTargetBlendFunction::VTBlend_Cubic);
		}
	}

	bBossIntroInputMode = false;
	bBossIntroCameraBlendingOut = false;
	bBossIntroSkipHeldLocally = false;
	LocalBossIntroSkipHoldStartTime = 0.0f;
	ActiveBossIntroCamera.Reset();
	SetIgnoreMoveInput(bUpgradeInputMode);
	SetIgnoreLookInput(false);
	SetThirdPersonInputMode(bThirdPersonInputMode);
	if (PlayerHUDWidget)
	{
		PlayerHUDWidget->SetBossIntroPresentation(false, FText::GetEmpty(), 0.0f, 0.0f);
	}
	K2_OnBossIntroEnded(bWasInterrupted);
}

// 在所有带标签相机中选择离 Boss 最近者，同距离按对象路径排序以保证客户端选择稳定。
ACameraActor* AArenaPlayerController::FindBossIntroCamera(const AArenaBossCharacter* Boss) const
{
	if (!GetWorld() || !Boss || BossIntroCameraActorTag.IsNone())
	{
		return nullptr;
	}

	ACameraActor* BestCamera = nullptr;
	double BestDistanceSquared = TNumericLimits<double>::Max();
	FString BestPath;
	for (TActorIterator<ACameraActor> It(GetWorld()); It; ++It)
	{
		ACameraActor* Candidate = *It;
		if (!Candidate || !Candidate->ActorHasTag(BossIntroCameraActorTag))
		{
			continue;
		}

		const double DistanceSquared = FVector::DistSquared(
			Candidate->GetActorLocation(),
			Boss->GetActorLocation());
		const FString CandidatePath = Candidate->GetPathName();
		if (!BestCamera
			|| DistanceSquared < BestDistanceSquared - static_cast<double>(KINDA_SMALL_NUMBER)
			|| (FMath::IsNearlyEqual(DistanceSquared, BestDistanceSquared)
				&& CandidatePath.Compare(BestPath, ESearchCase::CaseSensitive) < 0))
		{
			BestCamera = Candidate;
			BestDistanceSquared = DistanceSquared;
			BestPath = CandidatePath;
		}
	}
	return BestCamera;
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

// 本地端绑定 HUD/Intro 快照，Authority 端也绑定阶段变化以立即清理 Skip Hold Timer。
void AArenaPlayerController::BindGameStateHUD()
{
	if (!IsLocalController() && !HasAuthority())
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
		ArenaGameState->OnActiveBossChanged.AddUniqueDynamic(this, &AArenaPlayerController::HandleActiveBossChanged);
		ArenaGameState->OnBossIntroTimingChanged.AddUniqueDynamic(this, &AArenaPlayerController::HandleBossIntroTimingChanged);
	}

	if (PlayerHUDWidget)
	{
		PlayerHUDWidget->SetGamePhase(ArenaGameState->GetGamePhase());
		PlayerHUDWidget->SetWaveState(ArenaGameState->GetCurrentWaveIndex(), ArenaGameState->GetRemainingEnemyCount());
		PlayerHUDWidget->SetUpgradeRandomSeed(ArenaGameState->GetUpgradeRandomSeed());
		PlayerHUDWidget->BindToBoss(ArenaGameState->GetActiveBoss());
	}
	RefreshBossIntroPresentation();
}

// 解除 GameState 阶段、波次、Boss 与 Intro 委托，并恢复所有本地演出状态。
void AArenaPlayerController::UnbindGameStateHUD()
{
	if (AArenaGameState* ArenaGameState = BoundArenaGameState.Get())
	{
		ArenaGameState->OnGamePhaseChanged.RemoveDynamic(this, &AArenaPlayerController::HandleGamePhaseChanged);
		ArenaGameState->OnCurrentWaveIndexChanged.RemoveDynamic(this, &AArenaPlayerController::HandleWaveIndexChanged);
		ArenaGameState->OnRemainingEnemyCountChanged.RemoveDynamic(this, &AArenaPlayerController::HandleRemainingEnemyCountChanged);
		ArenaGameState->OnUpgradeRandomSeedChanged.RemoveDynamic(this, &AArenaPlayerController::HandleUpgradeRandomSeedChanged);
		ArenaGameState->OnActiveBossChanged.RemoveDynamic(this, &AArenaPlayerController::HandleActiveBossChanged);
		ArenaGameState->OnBossIntroTimingChanged.RemoveDynamic(this, &AArenaPlayerController::HandleBossIntroTimingChanged);
	}
	FinishBossIntroPresentation(true);
	if (PlayerHUDWidget)
	{
		PlayerHUDWidget->BindToBoss(nullptr);
	}
	BoundArenaGameState.Reset();
}

// 阶段变化时先清理 Authority Hold Timer，再同步本地 HUD、Upgrade 与 Intro 表现。
void AArenaPlayerController::HandleGamePhaseChanged(EArenaGamePhase OldPhase, EArenaGamePhase NewPhase)
{
	if (OldPhase == EArenaGamePhase::BossIntro && NewPhase != EArenaGamePhase::BossIntro)
	{
		ClearBossIntroSkipHold();
	}
	if (PlayerHUDWidget)
	{
		PlayerHUDWidget->SetGamePhase(NewPhase);
	}
	RefreshUpgradeSelectionUI();
	RefreshBossIntroPresentation();
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

// 每个本地 Controller 只把复制 Boss 绑定到自己的 HUD，并在 OnRep 顺序允许后启动本地 Intro。
void AArenaPlayerController::HandleActiveBossChanged(AArenaBossCharacter* OldBoss, AArenaBossCharacter* NewBoss)
{
	if (PlayerHUDWidget)
	{
		PlayerHUDWidget->BindToBoss(NewBoss);
	}
	RefreshBossIntroPresentation();
}

// Intro 时序首次复制或被跳过缩短时立即评估尾段回切，不等待下一次普通阶段复制。
void AArenaPlayerController::HandleBossIntroTimingChanged(
	FArenaBossIntroTiming OldTiming,
	FArenaBossIntroTiming NewTiming)
{
	RefreshBossIntroPresentation();
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
