#include "Core/ArenaMainMenuPlayerController.h"

#include "Components/Widget.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/PackageName.h"
#include "UI/ArenaMainMenuWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogArenaMainMenu, Log, All);

// 配置无需 Pawn 的本地菜单 Controller 默认 View 和正式战斗地图。
AArenaMainMenuPlayerController::AArenaMainMenuPlayerController()
{
	MainMenuWidgetClass = UArenaMainMenuWidget::StaticClass();
	GameplayMapName = TEXT("/Game/TopDown/Lvl_TopDown");
}

// 菜单只属于本地表现，服务器或非本地 Controller 不创建 Widget 和输入焦点。
void AArenaMainMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	CreateMainMenu();
}

// 对称清理菜单委托与视口内容，避免关卡旅行期间保留失效 View 回调。
void AArenaMainMenuPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (MainMenuWidget)
	{
		MainMenuWidget->OnStartGameRequested.RemoveDynamic(this, &AArenaMainMenuPlayerController::HandleStartGameRequested);
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

	MainMenuWidget->OnStartGameRequested.AddUniqueDynamic(this, &AArenaMainMenuPlayerController::HandleStartGameRequested);
	MainMenuWidget->OnQuitGameRequested.AddUniqueDynamic(this, &AArenaMainMenuPlayerController::HandleQuitGameRequested);
	MainMenuWidget->AddToViewport(1000);

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

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

// 验证目标地图存在并只接受一次开始请求，随后由引擎执行无 Listen 参数的本地旅行。
void AArenaMainMenuPlayerController::HandleStartGameRequested()
{
	if (bTravelRequested || GameplayMapName.IsNone())
	{
		return;
	}

	const FString GameplayPackageName = GameplayMapName.ToString();
	if (!FPackageName::DoesPackageExist(GameplayPackageName))
	{
		UE_LOG(LogArenaMainMenu, Error, TEXT("Gameplay map package does not exist: %s"), *GameplayPackageName);
		return;
	}

	bTravelRequested = true;
	if (MainMenuWidget)
	{
		MainMenuWidget->SetMenuInteractionEnabled(false);
	}
	UGameplayStatics::OpenLevel(this, GameplayMapName, true);
}

// 防止重复请求后调用统一退出接口；PIE 会结束会话，Standalone 会关闭游戏窗口。
void AArenaMainMenuPlayerController::HandleQuitGameRequested()
{
	if (bTravelRequested)
	{
		return;
	}

	bTravelRequested = true;
	if (MainMenuWidget)
	{
		MainMenuWidget->SetMenuInteractionEnabled(false);
	}
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}
