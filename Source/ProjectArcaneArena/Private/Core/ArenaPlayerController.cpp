#include "Core/ArenaPlayerController.h"

#include "AbilitySystemComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Character/ArenaPlayerCharacter.h"
#include "Character/ArenaBossCharacter.h"
#include "Core/ArenaGameMode.h"
#include "Core/ArenaPlayerState.h"
#include "Core/ArenaGameState.h"
#include "Core/ArenaUpgradeDataAsset.h"
#include "Components/Button.h"
#include "Components/Widget.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GAS/ArenaGameplayTags.h"
#include "Item/ArenaInventoryComponent.h"
#include "Item/ArenaInventoryPickupActor.h"
#include "Item/ArenaItemDataAsset.h"
#include "Math/RotationMatrix.h"
#include "UI/ArenaInventoryWidget.h"
#include "UI/ArenaPlayerHUDWidget.h"
#include "UI/ArenaUpgradeSelectionWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogArenaBossPresentation, Log, All);

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

// 构造玩家控制器，设置基础鼠标输入并指定可直接使用的原生升级与背包界面类。
AArenaPlayerController::AArenaPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
	DefaultMouseCursor = EMouseCursor::Default;
	UpgradeSelectionWidgetClass = UArenaUpgradeSelectionWidget::StaticClass();
	InventoryWidgetClass = UArenaInventoryWidget::StaticClass();
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

// 开始时创建 HUD、升级和背包 View，并绑定本地或 Authority 端需要的复制状态。
void AArenaPlayerController::BeginPlay()
{
	Super::BeginPlay();

	CreatePlayerHUD();
	CreateUpgradeSelectionWidget();
	CreateInventoryWidget();
	SetThirdPersonInputMode(false);
	TryBindPlayerHUD();
	BindUpgradeState();
	BindInventoryState();
	BindGameStateHUD();
}

// PlayerState 在客户端完成复制后重新绑定 GAS HUD、OwnerOnly 升级和背包状态。
void AArenaPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	TryBindPlayerHUD();
	BindUpgradeState();
	BindInventoryState();
}

// Possess 新 Pawn 后重新绑定 HUD、升级与背包状态，兼容重生和 PlayerState 稍后就绪。
void AArenaPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	TryBindPlayerHUD();
	BindUpgradeState();
	BindInventoryState();
}

// 仅在本地控制器上创建常驻 HUD；普通阶段整体忽略命中测试，Victory 再临时开放子控件点击。
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
		PlayerHUDWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
		PlayerHUDWidget->SetThirdPersonReticleVisible(bThirdPersonInputMode);
		PlayerHUDWidget->OnVictoryRestartRequested.AddUniqueDynamic(
			this,
			&AArenaPlayerController::HandleVictoryRestartRequested);
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

// 创建独立背包 View；原生 Widget 已提供二十槽分页、筛选、使用和丢弃控制。
void AArenaPlayerController::CreateInventoryWidget()
{
	if (!IsLocalController() || InventoryWidget || !InventoryWidgetClass)
	{
		return;
	}

	InventoryWidget = CreateWidget<UArenaInventoryWidget>(this, InventoryWidgetClass);
	if (!InventoryWidget)
	{
		return;
	}

	InventoryWidget->AddToViewport(30);
	InventoryWidget->OnStackSelected.AddUniqueDynamic(
		this,
		&AArenaPlayerController::HandleInventoryStackSelected);
	InventoryWidget->OnUseRequested.AddUniqueDynamic(
		this,
		&AArenaPlayerController::HandleInventoryUseRequested);
	InventoryWidget->OnDropRequested.AddUniqueDynamic(
		this,
		&AArenaPlayerController::HandleInventoryDropRequested);
	InventoryWidget->OnFilterRequested.AddUniqueDynamic(
		this,
		&AArenaPlayerController::HandleInventoryFilterRequested);
	InventoryWidget->OnClearFiltersRequested.AddUniqueDynamic(
		this,
		&AArenaPlayerController::HandleInventoryClearFiltersRequested);
	InventoryWidget->OnPreviousPageRequested.AddUniqueDynamic(
		this,
		&AArenaPlayerController::HandleInventoryPreviousPageRequested);
	InventoryWidget->OnNextPageRequested.AddUniqueDynamic(
		this,
		&AArenaPlayerController::HandleInventoryNextPageRequested);
	InventoryWidget->OnCloseRequested.AddUniqueDynamic(
		this,
		&AArenaPlayerController::HandleInventoryCloseRequested);
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
		ArenaPlayerState->OnVictoryRestartReadyChanged.AddUniqueDynamic(
			this,
			&AArenaPlayerController::HandleVictoryRestartReadyChanged);
	}

	RefreshUpgradeSelectionUI();
	RefreshVictoryPresentation();
}

// 解除旧 PlayerState 的升级委托，避免重生、旅行或重连后重复回调。
void AArenaPlayerController::UnbindUpgradeState()
{
	if (AArenaPlayerState* ArenaPlayerState = BoundUpgradePlayerState.Get())
	{
		ArenaPlayerState->OnUpgradeStateChanged.RemoveDynamic(this, &AArenaPlayerController::HandleUpgradeStateChanged);
		ArenaPlayerState->OnVictoryRestartReadyChanged.RemoveDynamic(
			this,
			&AArenaPlayerController::HandleVictoryRestartReadyChanged);
	}
	BoundUpgradePlayerState.Reset();
}

// 绑定 PlayerState 上的 OwnerOnly 背包 Model，服务器 Listen Player 与所属客户端共用同一刷新入口。
void AArenaPlayerController::BindInventoryState()
{
	if (!IsLocalController())
	{
		return;
	}

	AArenaPlayerState* ArenaPlayerState = GetPlayerState<AArenaPlayerState>();
	UArenaInventoryComponent* InventoryComponent =
		ArenaPlayerState ? ArenaPlayerState->GetInventoryComponent() : nullptr;
	if (!InventoryComponent)
	{
		UnbindInventoryState();
		if (bInventoryInputMode)
		{
			SetInventoryInputMode(false);
		}
		return;
	}

	if (BoundInventoryComponent.Get() != InventoryComponent)
	{
		UnbindInventoryState();
		BoundInventoryComponent = InventoryComponent;
		InventoryComponent->OnInventoryChanged.AddUniqueDynamic(
			this,
			&AArenaPlayerController::HandleInventoryChanged);
	}

	RefreshInventoryUI();
}

// 解除旧背包 Model 委托并清空筛选、分页和选择，防止旅行或重连后保留失效 View 状态。
void AArenaPlayerController::UnbindInventoryState()
{
	if (UArenaInventoryComponent* InventoryComponent = BoundInventoryComponent.Get())
	{
		InventoryComponent->OnInventoryChanged.RemoveDynamic(
			this,
			&AArenaPlayerController::HandleInventoryChanged);
	}

	BoundInventoryComponent.Reset();
	ActiveInventoryFilters.Reset();
	SelectedInventoryStackId.Invalidate();
	InventoryPageIndex = 0;
}

// 按固定 Model 顺序执行多 Tag OR 筛选，再计算动态页数并构建最多二十项的 ViewData。
void AArenaPlayerController::RefreshInventoryUI()
{
	if (!IsLocalController() || !bInventoryInputMode)
	{
		return;
	}

	if (!InventoryWidget)
	{
		CreateInventoryWidget();
	}

	UArenaInventoryComponent* InventoryComponent = BoundInventoryComponent.Get();
	if (!InventoryWidget || !InventoryComponent)
	{
		return;
	}

	TArray<FArenaInventoryEntry> FilteredEntries;
	for (const FArenaInventoryEntry& Entry : InventoryComponent->GetInventorySnapshot())
	{
		if (!Entry.ItemData || Entry.Quantity <= 0 || !Entry.StackId.IsValid())
		{
			continue;
		}

		if (!ArenaInventory::MatchesFilters(Entry.ItemData, ActiveInventoryFilters))
		{
			continue;
		}
		FilteredEntries.Add(Entry);
	}

	const int32 TotalPages = ArenaInventory::CalculatePageCount(FilteredEntries.Num());
	InventoryPageIndex = FMath::Clamp(InventoryPageIndex, 0, TotalPages - 1);

	const int32 StartIndex = InventoryPageIndex * ArenaInventory::ItemsPerPage;
	const int32 EndIndex = FMath::Min(
		StartIndex + ArenaInventory::ItemsPerPage,
		FilteredEntries.Num());
	bool bSelectionStillOnCurrentPage = false;
	for (int32 Index = StartIndex; Index < EndIndex; ++Index)
	{
		if (FilteredEntries[Index].StackId == SelectedInventoryStackId)
		{
			bSelectionStillOnCurrentPage = true;
			break;
		}
	}
	if (!bSelectionStillOnCurrentPage)
	{
		SelectedInventoryStackId.Invalidate();
	}

	FArenaInventoryPageViewData PageViewData;
	PageViewData.CurrentPage = InventoryPageIndex + 1;
	PageViewData.TotalPages = TotalPages;
	PageViewData.SelectedStackId = SelectedInventoryStackId;
	PageViewData.ActiveFilters = ActiveInventoryFilters;

	for (int32 Index = StartIndex; Index < EndIndex; ++Index)
	{
		const FArenaInventoryEntry& Entry = FilteredEntries[Index];
		FArenaInventoryItemViewData& ItemViewData = PageViewData.Items.AddDefaulted_GetRef();
		ItemViewData.StackId = Entry.StackId;
		ItemViewData.ItemData = Entry.ItemData;
		ItemViewData.Quantity = Entry.Quantity;
	}

	InventoryWidget->ShowInventoryPage(PageViewData);
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

// Tab 只在 Combat 且玩家可操作时打开背包；已打开时始终允许关闭以恢复输入。
void AArenaPlayerController::ToggleInventory()
{
	if (!IsLocalController())
	{
		return;
	}

	if (bInventoryInputMode)
	{
		SetInventoryInputMode(false);
		return;
	}

	const AArenaGameState* ArenaGameState = GetWorld() ? GetWorld()->GetGameState<AArenaGameState>() : nullptr;
	const AArenaPlayerState* ArenaPlayerState = GetPlayerState<AArenaPlayerState>();
	const UAbilitySystemComponent* AbilitySystemComponent =
		ArenaPlayerState ? ArenaPlayerState->GetAbilitySystemComponent() : nullptr;
	if (bUpgradeInputMode || bBossIntroInputMode || bBossOutroInputMode || bVictoryInputMode
		|| !ArenaGameState || ArenaGameState->GetGamePhase() != EArenaGamePhase::Combat
		|| !AbilitySystemComponent
		|| AbilitySystemComponent->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead)
		|| AbilitySystemComponent->HasMatchingGameplayTag(ArenaGameplayTags::State_Stunned))
	{
		return;
	}

	BindInventoryState();
	if (!BoundInventoryComponent.IsValid())
	{
		return;
	}

	SetInventoryInputMode(true);
	RefreshInventoryUI();
}

// 背包打开时使用 GameAndUI 和可见鼠标，关闭时恢复进入背包前保留的双视角输入表现。
void AArenaPlayerController::SetInventoryInputMode(bool bEnabled)
{
	if (!IsLocalController() || bInventoryInputMode == bEnabled)
	{
		return;
	}

	bInventoryInputMode = bEnabled;
	FlushPressedKeys();
	if (bInventoryInputMode)
	{
		if (AArenaPlayerCharacter* PlayerCharacter = Cast<AArenaPlayerCharacter>(GetPawn()))
		{
			PlayerCharacter->StopSprintingForInventory();
		}
		if (APawn* ControlledPawn = GetPawn())
		{
			ControlledPawn->ConsumeMovementInputVector();
			if (UPawnMovementComponent* MovementComponent = ControlledPawn->GetMovementComponent())
			{
				MovementComponent->StopMovementImmediately();
			}
		}

		SetIgnoreMoveInput(true);
		SetIgnoreLookInput(true);
		bEnableClickEvents = true;
		bEnableMouseOverEvents = true;
		bShowMouseCursor = true;
		if (PlayerHUDWidget)
		{
			PlayerHUDWidget->SetThirdPersonReticleVisible(false);
		}
		RefreshInventoryUI();

		FInputModeGameAndUI InputMode;
		if (InventoryWidget)
		{
			if (UWidget* InitialFocusTarget = InventoryWidget->GetInitialFocusTarget())
			{
				InputMode.SetWidgetToFocus(InitialFocusTarget->TakeWidget());
			}
		}
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
		return;
	}

	if (InventoryWidget)
	{
		InventoryWidget->HideInventory();
	}
	SelectedInventoryStackId.Invalidate();
	SetIgnoreMoveInput(bUpgradeInputMode || bBossIntroInputMode || bBossOutroInputMode || bVictoryInputMode);
	SetIgnoreLookInput(bBossIntroInputMode || bBossOutroInputMode || bVictoryInputMode);
	bEnableClickEvents = bVictoryInputMode;
	bEnableMouseOverEvents = bVictoryInputMode;
	SetThirdPersonInputMode(bThirdPersonInputMode);
}

// 从玩家位置而非远端相机位置选择最近可见 Pickup，避免顶视角和第三人称得到不同交互距离。
AArenaInventoryPickupActor* AArenaPlayerController::FindNearestInteractableInventoryPickup() const
{
	const APawn* ControlledPawn = GetPawn();
	const AArenaPlayerState* ArenaPlayerState = GetPlayerState<AArenaPlayerState>();
	UWorld* World = GetWorld();
	if (!ControlledPawn || !ArenaPlayerState || !World)
	{
		return nullptr;
	}

	AArenaInventoryPickupActor* BestPickup = nullptr;
	float BestDistanceSquared = FMath::Square(FMath::Max(InventoryInteractionDistance, 1.0f));
	const FVector TraceStart = ControlledPawn->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);
	for (TActorIterator<AArenaInventoryPickupActor> Iterator(World); Iterator; ++Iterator)
	{
		AArenaInventoryPickupActor* Candidate = *Iterator;
		if (!IsValid(Candidate) || !Candidate->CanBeInteractedBy(ArenaPlayerState))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(
			ControlledPawn->GetActorLocation(),
			Candidate->GetActorLocation());
		if (DistanceSquared > BestDistanceSquared)
		{
			continue;
		}

		FHitResult VisibilityHit;
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ArenaInventoryClientInteraction), false, ControlledPawn);
		const bool bBlocked = World->LineTraceSingleByChannel(
			VisibilityHit,
			TraceStart,
			Candidate->GetActorLocation(),
			ECC_Visibility,
			QueryParams);
		if (bBlocked && VisibilityHit.GetActor() != Candidate)
		{
			continue;
		}

		const bool bIsCloser = DistanceSquared < BestDistanceSquared - KINDA_SMALL_NUMBER;
		const bool bStableTieBreak = BestPickup
			&& FMath::IsNearlyEqual(DistanceSquared, BestDistanceSquared)
			&& Candidate->GetPathName().Compare(BestPickup->GetPathName()) < 0;
		if (!BestPickup || bIsCloser || bStableTieBreak)
		{
			BestPickup = Candidate;
			BestDistanceSquared = DistanceSquared;
		}
	}
	return BestPickup;
}

// G 只提交本地找到的候选 Actor，不能直接改变世界 Pickup 或背包 Model。
void AArenaPlayerController::RequestInteractWithNearestInventoryPickup()
{
	if (!IsLocalController() || bInventoryInputMode)
	{
		return;
	}

	const AArenaGameState* ArenaGameState = GetWorld() ? GetWorld()->GetGameState<AArenaGameState>() : nullptr;
	if (!ArenaGameState || ArenaGameState->GetGamePhase() != EArenaGamePhase::Combat)
	{
		return;
	}

	if (AArenaInventoryPickupActor* PickupActor = FindNearestInteractableInventoryPickup())
	{
		ServerInteractWithInventoryPickup(PickupActor);
	}
}

// InventoryComponent 复制变化时只重建当前筛选页面，不修改服务器顺序或数量。
void AArenaPlayerController::HandleInventoryChanged()
{
	RefreshInventoryUI();
}

// 槽位选择是纯本地 View 状态，使用和丢弃仍以稳定 StackId 走服务器 RPC。
void AArenaPlayerController::HandleInventoryStackSelected(FGuid StackId)
{
	SelectedInventoryStackId = StackId;
	RefreshInventoryUI();
}

// 双击或 Use 按钮只提交当前 StackId，服务器验证恢复资格和共享冷却。
void AArenaPlayerController::HandleInventoryUseRequested(FGuid StackId)
{
	if (bInventoryInputMode && StackId.IsValid())
	{
		ServerUseInventoryItem(StackId);
	}
}

// 丢弃面板只提交正数量，服务器会再次限制到当前权威堆栈数量。
void AArenaPlayerController::HandleInventoryDropRequested(FGuid StackId, int32 Quantity)
{
	if (bInventoryInputMode && StackId.IsValid() && Quantity > 0)
	{
		ServerDropInventoryItem(StackId, Quantity);
	}
}

// 多个筛选 Tag 使用 OR 语义；每次筛选变化回到第一页并清空跨页选择。
void AArenaPlayerController::HandleInventoryFilterRequested(FGameplayTag FilterTag, bool bEnabled)
{
	if (!FilterTag.IsValid())
	{
		return;
	}

	if (bEnabled)
	{
		ActiveInventoryFilters.AddTag(FilterTag);
	}
	else
	{
		ActiveInventoryFilters.RemoveTag(FilterTag);
	}
	InventoryPageIndex = 0;
	SelectedInventoryStackId.Invalidate();
	RefreshInventoryUI();
}

// All 筛选清空所有 Tag，恢复服务器 DisplayOrder 对应的完整列表。
void AArenaPlayerController::HandleInventoryClearFiltersRequested()
{
	ActiveInventoryFilters.Reset();
	InventoryPageIndex = 0;
	SelectedInventoryStackId.Invalidate();
	RefreshInventoryUI();
}

// 上一页只改变本地分页游标，服务器物品顺序保持不变。
void AArenaPlayerController::HandleInventoryPreviousPageRequested()
{
	InventoryPageIndex = FMath::Max(InventoryPageIndex - 1, 0);
	SelectedInventoryStackId.Invalidate();
	RefreshInventoryUI();
}

// 下一页先增加本地游标，再由 RefreshInventoryUI 按最新筛选结果 Clamp。
void AArenaPlayerController::HandleInventoryNextPageRequested()
{
	++InventoryPageIndex;
	SelectedInventoryStackId.Invalidate();
	RefreshInventoryUI();
}

// Close、Tab 和 Escape 共用同一输入恢复入口。
void AArenaPlayerController::HandleInventoryCloseRequested()
{
	SetInventoryInputMode(false);
}

// Authority 校验阶段、状态、距离、视线和拾取保护，多人竞争由 Pickup 消费门闩保证唯一成功。
void AArenaPlayerController::ServerInteractWithInventoryPickup_Implementation(
	AArenaInventoryPickupActor* PickupActor)
{
	APawn* ControlledPawn = GetPawn();
	AArenaPlayerState* ArenaPlayerState = GetPlayerState<AArenaPlayerState>();
	AArenaGameState* ArenaGameState = GetWorld() ? GetWorld()->GetGameState<AArenaGameState>() : nullptr;
	UAbilitySystemComponent* AbilitySystemComponent =
		ArenaPlayerState ? ArenaPlayerState->GetAbilitySystemComponent() : nullptr;
	if (!IsValid(PickupActor) || !ControlledPawn || !ArenaPlayerState || !ArenaGameState
		|| ArenaGameState->GetGamePhase() != EArenaGamePhase::Combat
		|| !AbilitySystemComponent
		|| AbilitySystemComponent->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead)
		|| AbilitySystemComponent->HasMatchingGameplayTag(ArenaGameplayTags::State_Stunned)
		|| !PickupActor->CanBeInteractedBy(ArenaPlayerState)
		|| FVector::DistSquared(ControlledPawn->GetActorLocation(), PickupActor->GetActorLocation())
			> FMath::Square(FMath::Max(InventoryInteractionDistance, 1.0f)))
	{
		return;
	}

	FHitResult VisibilityHit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ArenaInventoryServerInteraction), false, ControlledPawn);
	const FVector TraceStart = ControlledPawn->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);
	const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
		VisibilityHit,
		TraceStart,
		PickupActor->GetActorLocation(),
		ECC_Visibility,
		QueryParams);
	if (bBlocked && VisibilityHit.GetActor() != PickupActor)
	{
		return;
	}

	PickupActor->TryCollect(ArenaPlayerState);
}

// Authority 把使用请求交给 InventoryComponent，资源、冷却和物品扣除保持原子顺序。
void AArenaPlayerController::ServerUseInventoryItem_Implementation(FGuid StackId)
{
	if (AArenaPlayerState* ArenaPlayerState = GetPlayerState<AArenaPlayerState>())
	{
		if (UArenaInventoryComponent* InventoryComponent = ArenaPlayerState->GetInventoryComponent())
		{
			InventoryComponent->TryUseItem(StackId);
		}
	}
}

// Authority 以当前 Pawn 为丢弃来源，Pickup 成功生成前不会扣除堆栈。
void AArenaPlayerController::ServerDropInventoryItem_Implementation(FGuid StackId, int32 Quantity)
{
	if (AArenaPlayerState* ArenaPlayerState = GetPlayerState<AArenaPlayerState>())
	{
		if (UArenaInventoryComponent* InventoryComponent = ArenaPlayerState->GetInventoryComponent())
		{
			InventoryComponent->TryDropItem(StackId, Quantity, GetPawn());
		}
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

// Controller 销毁前恢复背包与演出输入，销毁临时镜头并解除全部委托与 Timer。
void AArenaPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UpgradeSelectionWidget)
	{
		UpgradeSelectionWidget->OnUpgradeChosen.RemoveDynamic(this, &AArenaPlayerController::HandleUpgradeChosen);
	}
	if (InventoryWidget)
	{
		InventoryWidget->OnStackSelected.RemoveDynamic(
			this,
			&AArenaPlayerController::HandleInventoryStackSelected);
		InventoryWidget->OnUseRequested.RemoveDynamic(
			this,
			&AArenaPlayerController::HandleInventoryUseRequested);
		InventoryWidget->OnDropRequested.RemoveDynamic(
			this,
			&AArenaPlayerController::HandleInventoryDropRequested);
		InventoryWidget->OnFilterRequested.RemoveDynamic(
			this,
			&AArenaPlayerController::HandleInventoryFilterRequested);
		InventoryWidget->OnClearFiltersRequested.RemoveDynamic(
			this,
			&AArenaPlayerController::HandleInventoryClearFiltersRequested);
		InventoryWidget->OnPreviousPageRequested.RemoveDynamic(
			this,
			&AArenaPlayerController::HandleInventoryPreviousPageRequested);
		InventoryWidget->OnNextPageRequested.RemoveDynamic(
			this,
			&AArenaPlayerController::HandleInventoryNextPageRequested);
		InventoryWidget->OnCloseRequested.RemoveDynamic(
			this,
			&AArenaPlayerController::HandleInventoryCloseRequested);
	}
	if (PlayerHUDWidget)
	{
		PlayerHUDWidget->OnVictoryRestartRequested.RemoveDynamic(
			this,
			&AArenaPlayerController::HandleVictoryRestartRequested);
	}
	FinishBossIntroPresentation(true);
	ReleaseDynamicBossPresentationCamera(DynamicBossIntroCamera, 0.0f);
	ClearBossIntroSkipHold();
	FinishBossOutroPresentation(true);
	ReleaseDynamicBossPresentationCamera(DynamicBossOutroCamera, 0.0f);
	ClearBossOutroSkipHold();
	SetVictoryInputMode(false);
	SetInventoryInputMode(false);
	UnbindInventoryState();
	UnbindUpgradeState();
	UnbindGameStateHUD();
	ClearPlayerHUDBindingRetry();
	Super::EndPlay(EndPlayReason);
}

// 切换双视角鼠标与准星状态；背包、升级、演出或 Victory 激活时保留专属输入表现。
void AArenaPlayerController::SetThirdPersonInputMode(bool bEnableThirdPerson)
{
	if (!IsLocalController())
	{
		return;
	}

	bThirdPersonInputMode = bEnableThirdPerson;
	if (bVictoryInputMode)
	{
		bShowMouseCursor = true;
		return;
	}
	if (bUpgradeInputMode)
	{
		bShowMouseCursor = true;
		return;
	}
	if (bInventoryInputMode)
	{
		bShowMouseCursor = true;
		if (PlayerHUDWidget)
		{
			PlayerHUDWidget->SetThirdPersonReticleVisible(false);
		}
		return;
	}
	if (bBossIntroInputMode || bBossOutroInputMode)
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

// 本地只记录 Outro Hold 进度并提交按住状态，客户端计时不决定是否跳过。
void AArenaPlayerController::SetBossOutroSkipHeld(bool bHeld)
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
		if (bBossOutroSkipHeldLocally
			|| !ArenaGameState
			|| ArenaGameState->GetGamePhase() != EArenaGamePhase::BossOutro)
		{
			return;
		}

		bBossOutroSkipHeldLocally = true;
		LocalBossOutroSkipHoldStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		ServerSetBossOutroSkipHeld(true);
	}
	else if (bBossOutroSkipHeldLocally)
	{
		bBossOutroSkipHeldLocally = false;
		LocalBossOutroSkipHoldStartTime = 0.0f;
		ServerSetBossOutroSkipHeld(false);
	}
}

// 服务器只接受 BossOutro 中首次按下，重复 RPC 不会重置 1.5 秒验证计时。
void AArenaPlayerController::ServerSetBossOutroSkipHeld_Implementation(bool bHeld)
{
	if (!bHeld)
	{
		ClearBossOutroSkipHold();
		return;
	}

	const AArenaGameState* ArenaGameState = GetWorld() ? GetWorld()->GetGameState<AArenaGameState>() : nullptr;
	if (bBossOutroSkipHeldOnServer
		|| !ArenaGameState
		|| ArenaGameState->GetGamePhase() != EArenaGamePhase::BossOutro
		|| !ArenaGameState->GetActiveBoss())
	{
		return;
	}

	bBossOutroSkipHeldOnServer = true;
	GetWorldTimerManager().SetTimer(
		BossOutroSkipHoldTimerHandle,
		this,
		&AArenaPlayerController::CompleteBossOutroSkipHold,
		FMath::Max(BossOutroSkipHoldDuration, 0.1f),
		false);
}

// Outro Hold 完成后交给 GameMode/WaveManager 重新验证参战者、阶段和死亡 Boss。
void AArenaPlayerController::CompleteBossOutroSkipHold()
{
	const bool bCompletedValidHold = bBossOutroSkipHeldOnServer;
	ClearBossOutroSkipHold();
	if (!bCompletedValidHold)
	{
		return;
	}

	if (AArenaGameMode* ArenaGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AArenaGameMode>() : nullptr)
	{
		ArenaGameMode->RequestBossOutroSkip(this);
	}
}

// 松开、阶段结束或销毁时清理服务器 Outro Hold Timer，防止迟到跳过。
void AArenaPlayerController::ClearBossOutroSkipHold()
{
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(BossOutroSkipHoldTimerHandle);
	}
	bBossOutroSkipHeldOnServer = false;
}

// Victory Ready RPC 只转发到服务器 GameMode，由规则层重新验证人数和阶段。
void AArenaPlayerController::ServerSetVictoryRestartReady_Implementation(bool bReady)
{
	if (AArenaGameMode* ArenaGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AArenaGameMode>() : nullptr)
	{
		ArenaGameMode->SetVictoryRestartReady(this, bReady);
	}
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

// 启动每个客户端独立的 Boss 出生镜头；默认动态构图，可选使用关卡相机覆盖。
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

	ReleaseDynamicBossPresentationCamera(DynamicBossIntroCamera, 0.0f);
	ACameraActor* IntroCamera = bUsePlacedBossIntroCameraOverride
		? FindBossIntroCamera(Boss)
		: nullptr;
	if (!IntroCamera)
	{
		IntroCamera = CreateDynamicBossPresentationCamera(
			Boss,
			Boss->GetActorLocation(),
			Boss->GetActorForwardVector(),
			BossIntroDynamicCameraDistance,
			BossIntroDynamicCameraHeight,
			CalculateBossIntroLookAtHeight(Boss),
			BossIntroDynamicCameraFieldOfView,
			BossIntroDynamicCameraCollisionRadius,
			BossIntroDynamicCameraMinimumDistance);
		DynamicBossIntroCamera = IntroCamera;
	}
	if (!IntroCamera && !bUsePlacedBossIntroCameraOverride)
	{
		IntroCamera = FindBossIntroCamera(Boss);
	}

	ActiveBossIntroCamera = IntroCamera;
	if (ACameraActor* ActiveIntroCamera = ActiveBossIntroCamera.Get())
	{
		SetViewTargetWithBlend(
			ActiveIntroCamera,
			FMath::Max(BossIntroCameraBlendDuration, 0.0f),
			EViewTargetBlendFunction::VTBlend_Cubic);
	}
	else if (!bWarnedMissingBossIntroCamera)
	{
		bWarnedMissingBossIntroCamera = true;
		UE_LOG(LogArenaBossPresentation, Warning,
			TEXT("Dynamic Boss Intro camera creation failed and no CameraActor tagged %s was found for Boss %s; Intro keeps the player camera."),
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

// 尾段只执行一次 ViewTarget 回切，并在 Blend 完成后释放本地动态 Intro 镜头。
void AArenaPlayerController::BeginBossIntroCameraBlendOut(float BlendOutDuration)
{
	if (!bBossIntroInputMode || bBossIntroCameraBlendingOut)
	{
		return;
	}

	bBossIntroCameraBlendingOut = true;
	if (APawn* ControlledPawn = GetPawn())
	{
		const float SafeBlendOutDuration = FMath::Max(BlendOutDuration, 0.0f);
		SetViewTargetWithBlend(
			ControlledPawn,
			SafeBlendOutDuration,
			EViewTargetBlendFunction::VTBlend_Cubic);
		ReleaseDynamicBossPresentationCamera(
			DynamicBossIntroCamera,
			SafeBlendOutDuration + 0.1f);
	}
	else
	{
		ReleaseDynamicBossPresentationCamera(DynamicBossIntroCamera, 0.0f);
	}
}

// 正常 Combat 与异常终局共用恢复入口，保证动态镜头、输入、ViewTarget 和 Hold 不残留。
void AArenaPlayerController::FinishBossIntroPresentation(bool bWasInterrupted)
{
	if (!bBossIntroInputMode && !bBossIntroSkipHeldLocally)
	{
		ReleaseDynamicBossPresentationCamera(DynamicBossIntroCamera, 0.0f);
		ActiveBossIntroCamera.Reset();
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
			const float SafeBlendDuration = FMath::Max(BossIntroCameraBlendDuration, 0.0f);
			SetViewTargetWithBlend(
				ControlledPawn,
				SafeBlendDuration,
				EViewTargetBlendFunction::VTBlend_Cubic);
			ReleaseDynamicBossPresentationCamera(
				DynamicBossIntroCamera,
				SafeBlendDuration + 0.1f);
		}
		else
		{
			ReleaseDynamicBossPresentationCamera(DynamicBossIntroCamera, 0.0f);
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

// 使用碰撞包围盒下半部作为注视点，使 Boss 全身位于底部 HUD 上方的安全构图区。
float AArenaPlayerController::CalculateBossIntroLookAtHeight(
	const AArenaBossCharacter* Boss) const
{
	if (!Boss)
	{
		return BossIntroDynamicCameraLookAtHeightOffset;
	}

	FVector BoundsOrigin = Boss->GetActorLocation();
	FVector BoundsExtent = FVector::ZeroVector;
	Boss->GetActorBounds(true, BoundsOrigin, BoundsExtent);
	const float BoundsCenterOffset = BoundsOrigin.Z - Boss->GetActorLocation().Z;
	const float SafeFramingBias = FMath::Clamp(
		BossIntroDynamicCameraVerticalFramingBias,
		0.0f,
		1.0f);
	return BoundsCenterOffset
		- BoundsExtent.Z * SafeFramingBias
		+ BossIntroDynamicCameraLookAtHeightOffset;
}

// 组合复制阶段、死亡 Boss 和 Outro Timing，容忍各字段 OnRep 到达顺序不同。
void AArenaPlayerController::RefreshBossOutroPresentation()
{
	if (!IsLocalController())
	{
		return;
	}

	AArenaGameState* ArenaGameState = BoundArenaGameState.Get();
	AArenaBossCharacter* Boss = ArenaGameState ? ArenaGameState->GetActiveBoss() : nullptr;
	const FArenaBossOutroTiming Timing = ArenaGameState
		? ArenaGameState->GetBossOutroTiming()
		: FArenaBossOutroTiming();
	const bool bOutroReady = ArenaGameState
		&& ArenaGameState->GetGamePhase() == EArenaGamePhase::BossOutro
		&& Boss
		&& Timing.EndServerTimeSeconds > ArenaGameState->GetServerWorldTimeSeconds();

	if (bOutroReady)
	{
		if (!bBossOutroInputMode)
		{
			StartBossOutroPresentation(Boss, Timing);
		}
		UpdateBossOutroPresentation();
	}
	else if (bBossOutroInputMode)
	{
		const bool bInterrupted = !ArenaGameState
			|| (ArenaGameState->GetGamePhase() != EArenaGamePhase::Victory
				&& ArenaGameState->GetGamePhase() != EArenaGamePhase::BossOutro);
		FinishBossOutroPresentation(bInterrupted);
	}
}

// 启动每个客户端独立的 Boss 死亡镜头；默认动态构图，可选使用关卡镜头覆盖。
void AArenaPlayerController::StartBossOutroPresentation(
	AArenaBossCharacter* Boss,
	const FArenaBossOutroTiming& Timing)
{
	if (!IsLocalController() || bBossOutroInputMode || !Boss)
	{
		return;
	}

	bBossOutroInputMode = true;
	bBossOutroCameraBlendingOut = false;
	bBossOutroSkipHeldLocally = false;
	LocalBossOutroSkipHoldStartTime = 0.0f;
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
	FlushPressedKeys();
	bShowMouseCursor = false;
	if (PlayerHUDWidget)
	{
		PlayerHUDWidget->SetThirdPersonReticleVisible(false);
	}

	ReleaseDynamicBossPresentationCamera(DynamicBossOutroCamera, 0.0f);
	ACameraActor* OutroCamera = bUsePlacedBossVictoryCameraOverride
		? FindBossOutroCamera(Timing.BossDeathLocation)
		: nullptr;
	if (!OutroCamera)
	{
		OutroCamera = CreateDynamicBossPresentationCamera(
			Boss,
			Timing.BossDeathLocation,
			FVector::ZeroVector,
			BossOutroDynamicCameraDistance,
			BossOutroDynamicCameraHeight,
			BossOutroDynamicCameraLookAtHeight,
			BossOutroDynamicCameraFieldOfView,
			BossOutroDynamicCameraCollisionRadius,
			BossOutroDynamicCameraMinimumDistance);
		DynamicBossOutroCamera = OutroCamera;
	}
	if (!OutroCamera && !bUsePlacedBossVictoryCameraOverride)
	{
		OutroCamera = FindBossOutroCamera(Timing.BossDeathLocation);
	}
	if (!OutroCamera)
	{
		OutroCamera = FindBossIntroCamera(Boss);
	}

	ActiveBossOutroCamera = OutroCamera;
	if (ACameraActor* ActiveOutroCamera = ActiveBossOutroCamera.Get())
	{
		SetViewTargetWithBlend(
			ActiveOutroCamera,
			FMath::Max(BossOutroCameraBlendDuration, 0.0f),
			EViewTargetBlendFunction::VTBlend_Cubic);
	}
	else if (!bWarnedMissingBossOutroCamera)
	{
		bWarnedMissingBossOutroCamera = true;
		UE_LOG(LogArenaBossPresentation, Warning,
			TEXT("Dynamic Boss Outro camera creation failed and no CameraActor tagged %s or %s was found; Outro keeps the player camera."),
			*BossVictoryCameraActorTag.ToString(),
			*BossIntroCameraActorTag.ToString());
	}

	GetWorldTimerManager().SetTimer(
		BossOutroPresentationTimerHandle,
		this,
		&AArenaPlayerController::UpdateBossOutroPresentation,
		FMath::Max(BossOutroPresentationTickInterval, 0.01f),
		true);
	K2_OnBossOutroStarted(Boss, FMath::Max(
		Timing.EndServerTimeSeconds - (BoundArenaGameState.IsValid()
			? BoundArenaGameState->GetServerWorldTimeSeconds()
			: 0.0f),
		0.0f));
}

// 使用同步服务器时间更新 Outro 回切窗口、Hold 进度和 HUD。
void AArenaPlayerController::UpdateBossOutroPresentation()
{
	if (!bBossOutroInputMode)
	{
		return;
	}

	AArenaGameState* ArenaGameState = BoundArenaGameState.Get();
	if (!ArenaGameState
		|| ArenaGameState->GetGamePhase() != EArenaGamePhase::BossOutro
		|| !ArenaGameState->GetActiveBoss())
	{
		FinishBossOutroPresentation(true);
		return;
	}

	const FArenaBossOutroTiming Timing = ArenaGameState->GetBossOutroTiming();
	const float RemainingTime = ArenaGameState->GetBossOutroRemainingTime();
	if (RemainingTime <= Timing.BlendOutDuration + KINDA_SMALL_NUMBER)
	{
		BeginBossOutroCameraBlendOut(Timing.BlendOutDuration);
	}

	float SkipProgress = 0.0f;
	if (bBossOutroSkipHeldLocally && GetWorld())
	{
		SkipProgress = FMath::Clamp(
			(GetWorld()->GetTimeSeconds() - LocalBossOutroSkipHoldStartTime)
				/ FMath::Max(BossOutroSkipHoldDuration, 0.1f),
			0.0f,
			1.0f);
	}
	if (PlayerHUDWidget)
	{
		PlayerHUDWidget->SetBossOutroPresentation(true, RemainingTime, SkipProgress);
	}
}

// Outro 尾段只执行一次 ViewTarget 回切，并在 Blend 完成后释放本地动态镜头。
void AArenaPlayerController::BeginBossOutroCameraBlendOut(float BlendOutDuration)
{
	if (!bBossOutroInputMode || bBossOutroCameraBlendingOut)
	{
		return;
	}

	bBossOutroCameraBlendingOut = true;
	if (APawn* ControlledPawn = GetPawn())
	{
		const float SafeBlendOutDuration = FMath::Max(BlendOutDuration, 0.0f);
		SetViewTargetWithBlend(
			ControlledPawn,
			SafeBlendOutDuration,
			EViewTargetBlendFunction::VTBlend_Cubic);
		ReleaseDynamicBossPresentationCamera(
			DynamicBossOutroCamera,
			SafeBlendOutDuration + 0.1f);
	}
	else
	{
		ReleaseDynamicBossPresentationCamera(DynamicBossOutroCamera, 0.0f);
	}
}

// 正常 Victory 与异常退出共用清理入口，确保动态镜头、Hold、输入和 HUD 不残留。
void AArenaPlayerController::FinishBossOutroPresentation(bool bWasInterrupted)
{
	if (!bBossOutroInputMode && !bBossOutroSkipHeldLocally)
	{
		ReleaseDynamicBossPresentationCamera(DynamicBossOutroCamera, 0.0f);
		ActiveBossOutroCamera.Reset();
		return;
	}

	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(BossOutroPresentationTimerHandle);
	}
	if (bBossOutroSkipHeldLocally)
	{
		SetBossOutroSkipHeld(false);
	}
	ClearBossOutroSkipHold();

	if (!bBossOutroCameraBlendingOut)
	{
		if (APawn* ControlledPawn = GetPawn())
		{
			const float SafeBlendDuration = FMath::Max(BossOutroCameraBlendDuration, 0.0f);
			SetViewTargetWithBlend(
				ControlledPawn,
				SafeBlendDuration,
				EViewTargetBlendFunction::VTBlend_Cubic);
			ReleaseDynamicBossPresentationCamera(
				DynamicBossOutroCamera,
				SafeBlendDuration + 0.1f);
		}
		else
		{
			ReleaseDynamicBossPresentationCamera(DynamicBossOutroCamera, 0.0f);
		}
	}

	bBossOutroInputMode = false;
	bBossOutroCameraBlendingOut = false;
	bBossOutroSkipHeldLocally = false;
	LocalBossOutroSkipHoldStartTime = 0.0f;
	ActiveBossOutroCamera.Reset();
	SetIgnoreMoveInput(bUpgradeInputMode || bVictoryInputMode);
	SetIgnoreLookInput(false);
	if (!bVictoryInputMode)
	{
		SetThirdPersonInputMode(bThirdPersonInputMode);
	}
	if (PlayerHUDWidget)
	{
		PlayerHUDWidget->SetBossOutroPresentation(false, 0.0f, 0.0f);
	}
	K2_OnBossOutroEnded(bWasInterrupted);
}

// 选择距离死亡点最近的 BossVictoryCamera 美术覆盖，并以路径名打破同距离平局。
ACameraActor* AArenaPlayerController::FindBossOutroCamera(const FVector& BossDeathLocation) const
{
	if (!GetWorld() || BossVictoryCameraActorTag.IsNone())
	{
		return nullptr;
	}

	ACameraActor* BestCamera = nullptr;
	double BestDistanceSquared = TNumericLimits<double>::Max();
	FString BestPath;
	for (TActorIterator<ACameraActor> It(GetWorld()); It; ++It)
	{
		ACameraActor* Candidate = *It;
		if (!Candidate || !Candidate->ActorHasTag(BossVictoryCameraActorTag))
		{
			continue;
		}

		const double DistanceSquared = FVector::DistSquared(Candidate->GetActorLocation(), BossDeathLocation);
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

// 为 Intro 与 Outro 生成本地临时镜头，优先采用调用方方向并通过多角度候选避开遮挡。
ACameraActor* AArenaPlayerController::CreateDynamicBossPresentationCamera(
	const AArenaBossCharacter* Boss,
	const FVector& FocusLocation,
	const FVector& PreferredCameraDirection,
	float CameraDistance,
	float CameraHeight,
	float LookAtHeight,
	float FieldOfView,
	float CollisionRadius,
	float MinimumDistance)
{
	UWorld* World = GetWorld();
	if (!World || !IsLocalController())
	{
		return nullptr;
	}

	FVector CurrentViewLocation = FVector::ZeroVector;
	FRotator CurrentViewRotation = FRotator::ZeroRotator;
	GetPlayerViewPoint(CurrentViewLocation, CurrentViewRotation);

	FVector BaseDirection = PreferredCameraDirection;
	BaseDirection.Z = 0.0f;
	if (!BaseDirection.Normalize())
	{
		BaseDirection = CurrentViewLocation - FocusLocation;
		BaseDirection.Z = 0.0f;
		if (!BaseDirection.Normalize())
		{
			BaseDirection = -CurrentViewRotation.Vector();
			BaseDirection.Z = 0.0f;
			if (!BaseDirection.Normalize())
			{
				BaseDirection = FVector::BackwardVector;
			}
		}
	}

	const FVector LookAtLocation = FocusLocation
		+ FVector::UpVector * LookAtHeight;
	const float SafeCameraDistance = FMath::Max(CameraDistance, 100.0f);
	const float SafeCollisionRadius = FMath::Max(CollisionRadius, 0.0f);
	const float SafeMinimumDistance = FMath::Clamp(
		MinimumDistance,
		0.0f,
		SafeCameraDistance);
	constexpr float CandidateYawOffsets[] = {0.0f, 35.0f, -35.0f, 70.0f, -70.0f, 180.0f};

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ArenaBossPresentationCamera), false);
	QueryParams.bFindInitialOverlaps = false;
	if (Boss)
	{
		QueryParams.AddIgnoredActor(Boss);
	}
	for (TActorIterator<APawn> PawnIt(World); PawnIt; ++PawnIt)
	{
		QueryParams.AddIgnoredActor(*PawnIt);
	}

	FVector BestLocation = FVector::ZeroVector;
	float BestScore = -TNumericLimits<float>::Max();
	for (const float YawOffset : CandidateYawOffsets)
	{
		const FVector CandidateDirection = FRotator(0.0f, YawOffset, 0.0f)
			.RotateVector(BaseDirection)
			.GetSafeNormal2D();
		const FVector DesiredLocation = FocusLocation
			+ CandidateDirection * SafeCameraDistance
			+ FVector::UpVector * FMath::Max(CameraHeight, 0.0f);

		FHitResult BlockingHit;
		const bool bBlocked = SafeCollisionRadius > KINDA_SMALL_NUMBER
			&& World->SweepSingleByChannel(
				BlockingHit,
				LookAtLocation,
				DesiredLocation,
				FQuat::Identity,
				ECC_Camera,
				FCollisionShape::MakeSphere(SafeCollisionRadius),
				QueryParams);
		const FVector ResolvedLocation = bBlocked ? BlockingHit.Location : DesiredLocation;
		const float ResolvedDistance = FVector::Distance(LookAtLocation, ResolvedLocation);
		const bool bMeetsMinimumDistance =
			ResolvedDistance + KINDA_SMALL_NUMBER >= SafeMinimumDistance;
		const float CandidateScore = (bMeetsMinimumDistance ? 100000.0f : 0.0f)
			+ ResolvedDistance
			- FMath::Abs(YawOffset) * 1.5f;
		if (CandidateScore > BestScore)
		{
			BestScore = CandidateScore;
			BestLocation = ResolvedLocation;
		}
	}

	if (BestScore <= -TNumericLimits<float>::Max() / 2.0f)
	{
		return nullptr;
	}

	const FRotator CameraRotation = (LookAtLocation - BestLocation).Rotation();
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParameters.ObjectFlags |= RF_Transient;
	ACameraActor* DynamicCamera = World->SpawnActor<ACameraActor>(
		ACameraActor::StaticClass(),
		BestLocation,
		CameraRotation,
		SpawnParameters);
	if (!DynamicCamera)
	{
		return nullptr;
	}

	DynamicCamera->SetReplicates(false);
	DynamicCamera->SetActorEnableCollision(false);
	if (UCameraComponent* CameraComponent = DynamicCamera->GetCameraComponent())
	{
		CameraComponent->SetFieldOfView(FMath::Clamp(
			FieldOfView,
			5.0f,
			170.0f));
	}
	return DynamicCamera;
}

// 延迟销毁指定的本地动态镜头，保证 ViewTarget 混合结束前 Actor 始终有效。
void AArenaPlayerController::ReleaseDynamicBossPresentationCamera(
	TWeakObjectPtr<ACameraActor>& DynamicCameraReference,
	float DelaySeconds)
{
	ACameraActor* DynamicCamera = DynamicCameraReference.Get();
	DynamicCameraReference.Reset();
	if (!IsValid(DynamicCamera))
	{
		return;
	}

	const float SafeDelay = FMath::Max(DelaySeconds, 0.0f);
	if (SafeDelay > KINDA_SMALL_NUMBER)
	{
		DynamicCamera->SetLifeSpan(SafeDelay);
	}
	else
	{
		DynamicCamera->Destroy();
	}
}

// 根据复制 Victory 状态切换 UIOnly 输入并刷新本地 Ready 展示。
void AArenaPlayerController::RefreshVictoryPresentation()
{
	if (!IsLocalController())
	{
		return;
	}

	const AArenaGameState* ArenaGameState = BoundArenaGameState.Get();
	const AArenaPlayerState* ArenaPlayerState = GetPlayerState<AArenaPlayerState>();
	const bool bVisible = ArenaGameState && ArenaGameState->GetGamePhase() == EArenaGamePhase::Victory;
	if (PlayerHUDWidget)
	{
		PlayerHUDWidget->SetVictoryPresentation(
			bVisible,
			ArenaPlayerState && ArenaPlayerState->IsVictoryRestartReady(),
			ArenaGameState ? ArenaGameState->GetVictoryRestartReadyCount() : 0,
			ArenaGameState ? ArenaGameState->GetVictoryRestartRequiredCount() : 0);
	}
	SetVictoryInputMode(bVisible);
}

// Victory 临时开放 HUD 子控件命中并使用无键盘焦点的 GameAndUI，确保鼠标直接点击重开按钮。
void AArenaPlayerController::SetVictoryInputMode(bool bEnabled)
{
	if (!IsLocalController() || bVictoryInputMode == bEnabled)
	{
		return;
	}

	bVictoryInputMode = bEnabled;
	if (bVictoryInputMode)
	{
		SetIgnoreMoveInput(true);
		SetIgnoreLookInput(true);
		FlushPressedKeys();
		bShowMouseCursor = true;
		if (PlayerHUDWidget)
		{
			PlayerHUDWidget->SetIsEnabled(true);
			PlayerHUDWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			PlayerHUDWidget->SetThirdPersonReticleVisible(false);
		}
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
	}
	else
	{
		FlushPressedKeys();
		if (PlayerHUDWidget)
		{
			PlayerHUDWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		SetIgnoreMoveInput(bUpgradeInputMode || bBossIntroInputMode || bBossOutroInputMode);
		SetIgnoreLookInput(bBossIntroInputMode || bBossOutroInputMode);
		if (!bUpgradeInputMode && !bBossIntroInputMode && !bBossOutroInputMode)
		{
			SetThirdPersonInputMode(bThirdPersonInputMode);
			if (PlayerHUDWidget)
			{
				PlayerHUDWidget->SetThirdPersonReticleVisible(bThirdPersonInputMode);
			}
		}
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

// 本地端绑定 HUD、Boss 演出和 Victory 快照，Authority 端也监听阶段以清理 Skip Hold Timer。
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
		ArenaGameState->OnBossOutroTimingChanged.AddUniqueDynamic(this, &AArenaPlayerController::HandleBossOutroTimingChanged);
		ArenaGameState->OnVictoryRestartReadyCountChanged.AddUniqueDynamic(
			this,
			&AArenaPlayerController::HandleVictoryRestartCountChanged);
		ArenaGameState->OnVictoryRestartRequiredCountChanged.AddUniqueDynamic(
			this,
			&AArenaPlayerController::HandleVictoryRestartCountChanged);
	}

	if (PlayerHUDWidget)
	{
		PlayerHUDWidget->SetGamePhase(ArenaGameState->GetGamePhase());
		PlayerHUDWidget->SetWaveState(ArenaGameState->GetCurrentWaveIndex(), ArenaGameState->GetRemainingEnemyCount());
		PlayerHUDWidget->SetUpgradeRandomSeed(ArenaGameState->GetUpgradeRandomSeed());
		PlayerHUDWidget->BindToBoss(ArenaGameState->GetActiveBoss());
	}
	RefreshBossIntroPresentation();
	RefreshBossOutroPresentation();
	RefreshVictoryPresentation();
}

// 解除 GameState 阶段、波次、Boss、演出与 Ready 委托，并恢复全部本地输入状态。
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
		ArenaGameState->OnBossOutroTimingChanged.RemoveDynamic(this, &AArenaPlayerController::HandleBossOutroTimingChanged);
		ArenaGameState->OnVictoryRestartReadyCountChanged.RemoveDynamic(
			this,
			&AArenaPlayerController::HandleVictoryRestartCountChanged);
		ArenaGameState->OnVictoryRestartRequiredCountChanged.RemoveDynamic(
			this,
			&AArenaPlayerController::HandleVictoryRestartCountChanged);
	}
	FinishBossIntroPresentation(true);
	FinishBossOutroPresentation(true);
	SetVictoryInputMode(false);
	if (PlayerHUDWidget)
	{
		PlayerHUDWidget->BindToBoss(nullptr);
	}
	BoundArenaGameState.Reset();
}

// 阶段变化时先关闭仅 Combat 可用的背包，再同步 HUD、Upgrade、演出和 Victory 表现。
void AArenaPlayerController::HandleGamePhaseChanged(EArenaGamePhase OldPhase, EArenaGamePhase NewPhase)
{
	if (NewPhase != EArenaGamePhase::Combat && bInventoryInputMode)
	{
		SetInventoryInputMode(false);
	}
	if (OldPhase == EArenaGamePhase::BossIntro && NewPhase != EArenaGamePhase::BossIntro)
	{
		ClearBossIntroSkipHold();
	}
	if (OldPhase == EArenaGamePhase::BossOutro && NewPhase != EArenaGamePhase::BossOutro)
	{
		ClearBossOutroSkipHold();
	}
	if (PlayerHUDWidget)
	{
		PlayerHUDWidget->SetGamePhase(NewPhase);
	}
	RefreshUpgradeSelectionUI();
	RefreshBossIntroPresentation();
	RefreshBossOutroPresentation();
	RefreshVictoryPresentation();
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

// 每个本地 Controller 只绑定自己的 Boss HUD，并在 OnRep 顺序允许后启动 Intro 或 Outro。
void AArenaPlayerController::HandleActiveBossChanged(AArenaBossCharacter* OldBoss, AArenaBossCharacter* NewBoss)
{
	if (PlayerHUDWidget)
	{
		PlayerHUDWidget->BindToBoss(NewBoss);
	}
	RefreshBossIntroPresentation();
	RefreshBossOutroPresentation();
}

// Intro 时序首次复制或被跳过缩短时立即评估尾段回切，不等待下一次普通阶段复制。
void AArenaPlayerController::HandleBossIntroTimingChanged(
	FArenaBossIntroTiming OldTiming,
	FArenaBossIntroTiming NewTiming)
{
	RefreshBossIntroPresentation();
}

// Outro Timing 首次复制或被跳过缩短时立即刷新本地镜头和 HUD。
void AArenaPlayerController::HandleBossOutroTimingChanged(
	FArenaBossOutroTiming OldTiming,
	FArenaBossOutroTiming NewTiming)
{
	RefreshBossOutroPresentation();
}

// 任一 Victory Ready 计数变化时刷新本地终局状态。
void AArenaPlayerController::HandleVictoryRestartCountChanged(int32 OldValue, int32 NewValue)
{
	RefreshVictoryPresentation();
}

// 本地 PlayerState Ready 状态复制后刷新按钮文案。
void AArenaPlayerController::HandleVictoryRestartReadyChanged(bool bIsReady)
{
	RefreshVictoryPresentation();
}

// Victory 按钮提交 Restart/Ready；只有多人尚未全员确认时才允许取消个人 Ready。
void AArenaPlayerController::HandleVictoryRestartRequested()
{
	const AArenaGameState* ArenaGameState = BoundArenaGameState.Get();
	const AArenaPlayerState* ArenaPlayerState = GetPlayerState<AArenaPlayerState>();
	if (!IsLocalController()
		|| !ArenaGameState
		|| ArenaGameState->GetGamePhase() != EArenaGamePhase::Victory
		|| !ArenaPlayerState)
	{
		return;
	}

	const int32 RequiredCount = ArenaGameState->GetVictoryRestartRequiredCount();
	const int32 ReadyCount = ArenaGameState->GetVictoryRestartReadyCount();
	const bool bAllPlayersReady = RequiredCount > 0 && ReadyCount >= RequiredCount;
	const bool bLocalReady = ArenaPlayerState->IsVictoryRestartReady();
	const bool bCanCancelReady = RequiredCount > 1 && bLocalReady && !bAllPlayersReady;
	if (bLocalReady && !bCanCancelReady)
	{
		return;
	}

	ServerSetVictoryRestartReady(!bLocalReady);
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
