#include "UI/ArenaMainMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"

namespace ArenaMainMenuWidget
{
	constexpr int32 FrontPageIndex = 0;
	constexpr int32 MultiplayerPageIndex = 1;
	constexpr int32 LobbyPageIndex = 2;
	constexpr int32 ConnectingPageIndex = 3;

	// 为 fallback 按钮设置统一紧凑尺寸、状态颜色和文字层级。
	void ConfigureMenuButton(UButton* Button, UTextBlock* Label, const FText& LabelText, const int32 FontSize = 20)
	{
		if (!Button || !Label)
		{
			return;
		}

		FButtonStyle ButtonStyle = Button->GetStyle();
		ButtonStyle.Normal.DrawAs = ESlateBrushDrawType::Box;
		ButtonStyle.Normal.TintColor = FSlateColor(FLinearColor(0.055f, 0.075f, 0.13f, 0.96f));
		ButtonStyle.Hovered.DrawAs = ESlateBrushDrawType::Box;
		ButtonStyle.Hovered.TintColor = FSlateColor(FLinearColor(0.12f, 0.19f, 0.34f, 1.0f));
		ButtonStyle.Pressed.DrawAs = ESlateBrushDrawType::Box;
		ButtonStyle.Pressed.TintColor = FSlateColor(FLinearColor(0.035f, 0.055f, 0.11f, 1.0f));
		ButtonStyle.Disabled.DrawAs = ESlateBrushDrawType::Box;
		ButtonStyle.Disabled.TintColor = FSlateColor(FLinearColor(0.025f, 0.03f, 0.05f, 0.72f));
		ButtonStyle.NormalPadding = FMargin(14.0f, 8.0f);
		ButtonStyle.PressedPadding = FMargin(14.0f, 10.0f, 14.0f, 6.0f);
		Button->SetStyle(ButtonStyle);

		FSlateFontInfo LabelFont = Label->GetFont();
		LabelFont.Size = FontSize;
		Label->SetFont(LabelFont);
		Label->SetText(LabelText);
		Label->SetJustification(ETextJustify::Center);
		Label->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.94f, 1.0f, 1.0f)));
		Label->SetShadowOffset(FVector2D(1.0f, 2.0f));
		Label->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
		Button->AddChild(Label);
	}

	// 创建固定宽度按钮并添加到纵向页面，避免页面切换时布局抖动。
	UButton* AddVerticalButton(
		UWidgetTree* WidgetTree,
		UVerticalBox* Parent,
		const FName ButtonName,
		const FName LabelName,
		const FText& LabelText,
		UTextBlock** OutLabel = nullptr)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), ButtonName);
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), LabelName);
		ConfigureMenuButton(Button, Label, LabelText);
		if (OutLabel)
		{
			*OutLabel = Label;
		}

		USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>();
		SizeBox->SetWidthOverride(360.0f);
		SizeBox->SetHeightOverride(58.0f);
		SizeBox->AddChild(Button);
		if (UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(SizeBox))
		{
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
		}
		return Button;
	}

	// 创建页面小标题，保持多人设置、Lobby 和连接页的视觉层级一致。
	UTextBlock* AddPageTitle(UWidgetTree* WidgetTree, UVerticalBox* Parent, const FText& Text)
	{
		UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
		FSlateFontInfo Font = Title->GetFont();
		Font.Size = 28;
		Title->SetFont(Font);
		Title->SetText(Text);
		Title->SetJustification(ETextJustify::Center);
		Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.78f, 0.86f, 1.0f, 1.0f)));
		if (UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Title))
		{
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 24.0f));
		}
		return Title;
	}
}

// 在 Slate 初始化前启用根焦点，WBP 缺少核心切页控件时生成完整多人 fallback。
void UArenaMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	if (!MenuPageSwitcher
		|| !SinglePlayerButton
		|| !MultiplayerButton
		|| !QuitGameButton
		|| !HostMaxPlayersComboBox
		|| !HostLobbyButton
		|| !JoinAddressTextBox
		|| !JoinLobbyButton
		|| !MultiplayerBackButton
		|| !LobbyPlayersText
		|| !LobbyStatusText
		|| !LobbyReadyButton
		|| !LobbyStartButton
		|| !LobbyLeaveButton
		|| !ConnectingStatusText
		|| !MenuErrorText)
	{
		BuildFallbackLayout();
	}
}

// 进入视口后幂等绑定全部按钮，View 只负责采集本地输入并广播意图。
void UArenaMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (SinglePlayerButton) SinglePlayerButton->OnClicked.AddUniqueDynamic(this, &UArenaMainMenuWidget::HandleSinglePlayerClicked);
	if (MultiplayerButton) MultiplayerButton->OnClicked.AddUniqueDynamic(this, &UArenaMainMenuWidget::HandleMultiplayerClicked);
	if (HostLobbyButton) HostLobbyButton->OnClicked.AddUniqueDynamic(this, &UArenaMainMenuWidget::HandleHostLobbyClicked);
	if (JoinLobbyButton) JoinLobbyButton->OnClicked.AddUniqueDynamic(this, &UArenaMainMenuWidget::HandleJoinLobbyClicked);
	if (MultiplayerBackButton) MultiplayerBackButton->OnClicked.AddUniqueDynamic(this, &UArenaMainMenuWidget::HandleBackClicked);
	if (LobbyReadyButton) LobbyReadyButton->OnClicked.AddUniqueDynamic(this, &UArenaMainMenuWidget::HandleLobbyReadyClicked);
	if (LobbyStartButton) LobbyStartButton->OnClicked.AddUniqueDynamic(this, &UArenaMainMenuWidget::HandleLobbyStartClicked);
	if (LobbyLeaveButton) LobbyLeaveButton->OnClicked.AddUniqueDynamic(this, &UArenaMainMenuWidget::HandleLobbyLeaveClicked);
	if (QuitGameButton) QuitGameButton->OnClicked.AddUniqueDynamic(this, &UArenaMainMenuWidget::HandleQuitGameClicked);

	SetMenuInteractionEnabled(true);
}

// 对称移除所有按钮委托，支持连接失败或地图旅行后的干净 Widget 生命周期。
void UArenaMainMenuWidget::NativeDestruct()
{
	if (SinglePlayerButton) SinglePlayerButton->OnClicked.RemoveDynamic(this, &UArenaMainMenuWidget::HandleSinglePlayerClicked);
	if (MultiplayerButton) MultiplayerButton->OnClicked.RemoveDynamic(this, &UArenaMainMenuWidget::HandleMultiplayerClicked);
	if (HostLobbyButton) HostLobbyButton->OnClicked.RemoveDynamic(this, &UArenaMainMenuWidget::HandleHostLobbyClicked);
	if (JoinLobbyButton) JoinLobbyButton->OnClicked.RemoveDynamic(this, &UArenaMainMenuWidget::HandleJoinLobbyClicked);
	if (MultiplayerBackButton) MultiplayerBackButton->OnClicked.RemoveDynamic(this, &UArenaMainMenuWidget::HandleBackClicked);
	if (LobbyReadyButton) LobbyReadyButton->OnClicked.RemoveDynamic(this, &UArenaMainMenuWidget::HandleLobbyReadyClicked);
	if (LobbyStartButton) LobbyStartButton->OnClicked.RemoveDynamic(this, &UArenaMainMenuWidget::HandleLobbyStartClicked);
	if (LobbyLeaveButton) LobbyLeaveButton->OnClicked.RemoveDynamic(this, &UArenaMainMenuWidget::HandleLobbyLeaveClicked);
	if (QuitGameButton) QuitGameButton->OnClicked.RemoveDynamic(this, &UArenaMainMenuWidget::HandleQuitGameClicked);

	Super::NativeDestruct();
}

// 全局交互锁覆盖页面按钮；Lobby Start 还要额外满足复制的服务器资格。
void UArenaMainMenuWidget::SetMenuInteractionEnabled(const bool bEnabled)
{
	bMenuInteractionEnabled = bEnabled;
	const TArray<UButton*> Buttons = {
		SinglePlayerButton.Get(),
		MultiplayerButton.Get(),
		QuitGameButton.Get(),
		HostLobbyButton.Get(),
		JoinLobbyButton.Get(),
		MultiplayerBackButton.Get(),
		LobbyReadyButton.Get(),
		LobbyStartButton.Get(),
		LobbyLeaveButton.Get()
	};
	for (UButton* Button : Buttons)
	{
		if (Button)
		{
			Button->SetIsEnabled(bEnabled);
		}
	}
	if (LobbyStartButton)
	{
		LobbyStartButton->SetIsEnabled(bEnabled && CurrentLobbyViewData.bCanHostStart);
	}
	if (HostMaxPlayersComboBox) HostMaxPlayersComboBox->SetIsEnabled(bEnabled);
	if (JoinAddressTextBox) JoinAddressTextBox->SetIsEnabled(bEnabled);
}

// 首页恢复常规交互并隐藏旧错误以外的 Lobby 临时内容。
void UArenaMainMenuWidget::ShowFrontPage()
{
	CurrentPageIndex = ArenaMainMenuWidget::FrontPageIndex;
	if (MenuPageSwitcher) MenuPageSwitcher->SetActiveWidgetIndex(CurrentPageIndex);
	SetMenuInteractionEnabled(true);
}

// 多人设置页保留上一次地址与容量，方便连接失败后快速重试。
void UArenaMainMenuWidget::ShowMultiplayerSetupPage()
{
	CurrentPageIndex = ArenaMainMenuWidget::MultiplayerPageIndex;
	if (MenuPageSwitcher) MenuPageSwitcher->SetActiveWidgetIndex(CurrentPageIndex);
	SetMenuInteractionEnabled(true);
}

// Connecting 页只显示状态，所有网络按钮保持不可操作直到新世界确认或失败返回。
void UArenaMainMenuWidget::ShowConnectingPage(const FText& StatusText)
{
	CurrentPageIndex = ArenaMainMenuWidget::ConnectingPageIndex;
	if (ConnectingStatusText) ConnectingStatusText->SetText(StatusText);
	if (MenuPageSwitcher) MenuPageSwitcher->SetActiveWidgetIndex(CurrentPageIndex);
	SetMenuInteractionEnabled(false);
}

// Lobby ViewData 已由 Controller 从复制状态生成，Widget 只格式化成员列表和按钮可见性。
void UArenaMainMenuWidget::ShowLobbyPage(const FArenaLobbyViewData& ViewData)
{
	CurrentLobbyViewData = ViewData;
	CurrentPageIndex = ArenaMainMenuWidget::LobbyPageIndex;
	if (MenuPageSwitcher) MenuPageSwitcher->SetActiveWidgetIndex(CurrentPageIndex);

	FString PlayerLines;
	for (const FArenaLobbyPlayerViewData& Player : ViewData.Players)
	{
		const FString RoleText = Player.bIsHost
			? TEXT("HOST")
			: (Player.bIsReady ? TEXT("READY") : TEXT("NOT READY"));
		PlayerLines += FString::Printf(
			TEXT("%s%s    [%s]\n"),
			*Player.DisplayName.ToString(),
			Player.bIsLocalPlayer ? TEXT(" (You)") : TEXT(""),
			*RoleText);
	}
	if (LobbyPlayersText) LobbyPlayersText->SetText(FText::FromString(PlayerLines));

	if (LobbyStatusText)
	{
		LobbyStatusText->SetText(ViewData.bTravelStarting
			? NSLOCTEXT("ArenaMainMenu", "LobbyTraveling", "正在进入战斗...")
			: FText::Format(
				NSLOCTEXT("ArenaMainMenu", "LobbyCapacity", "玩家 {0}/{1}"),
				FText::AsNumber(ViewData.Players.Num()),
				FText::AsNumber(ViewData.MaxPlayers)));
	}
	if (LobbyReadyButton)
	{
		LobbyReadyButton->SetVisibility(ViewData.bLocalPlayerIsHost ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (LobbyReadyButtonText)
	{
		LobbyReadyButtonText->SetText(ViewData.bLocalPlayerIsReady
			? NSLOCTEXT("ArenaMainMenu", "CancelReady", "取消准备")
			: NSLOCTEXT("ArenaMainMenu", "Ready", "准备"));
	}
	if (LobbyStartButton)
	{
		LobbyStartButton->SetVisibility(ViewData.bLocalPlayerIsHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	SetMenuInteractionEnabled(!ViewData.bTravelStarting);
}

// 空错误隐藏提示；非空错误保持可换行显示，不改变当前页面或玩法状态。
void UArenaMainMenuWidget::SetErrorMessage(const FText& ErrorMessage)
{
	if (!MenuErrorText)
	{
		return;
	}
	MenuErrorText->SetText(ErrorMessage);
	MenuErrorText->SetVisibility(ErrorMessage.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
}

// 根据当前页优先选择实际可操作按钮，保证键盘焦点不会落到不可聚焦根容器。
UWidget* UArenaMainMenuWidget::GetInitialFocusTarget() const
{
	UButton* PreferredButton = nullptr;
	switch (CurrentPageIndex)
	{
	case ArenaMainMenuWidget::MultiplayerPageIndex:
		PreferredButton = HostLobbyButton;
		break;
	case ArenaMainMenuWidget::LobbyPageIndex:
		PreferredButton = CurrentLobbyViewData.bLocalPlayerIsHost ? LobbyStartButton : LobbyReadyButton;
		if (!PreferredButton || PreferredButton->GetVisibility() == ESlateVisibility::Collapsed || !PreferredButton->GetIsEnabled())
		{
			PreferredButton = LobbyLeaveButton;
		}
		break;
	case ArenaMainMenuWidget::FrontPageIndex:
	default:
		PreferredButton = SinglePlayerButton;
		break;
	}

	if (PreferredButton && PreferredButton->GetIsEnabled() && PreferredButton->GetIsFocusable())
	{
		return PreferredButton;
	}
	return const_cast<UArenaMainMenuWidget*>(this);
}

// 构建单个居中面板内的四个互斥页面，避免多人信息与首页按钮同时拥挤显示。
void UArenaMainMenuWidget::BuildFallbackLayout()
{
	if (!WidgetTree)
	{
		return;
	}

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MainMenuRootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MainMenuBackground"));
	Background->SetBrushColor(FLinearColor(0.006f, 0.009f, 0.025f, 1.0f));
	if (UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(Background))
	{
		CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		CanvasSlot->SetOffsets(FMargin(0.0f));
	}

	UBorder* CenterBackdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MainMenuCenterBackdrop"));
	CenterBackdrop->SetBrushColor(FLinearColor(0.025f, 0.035f, 0.075f, 0.92f));
	CenterBackdrop->SetPadding(FMargin(42.0f, 34.0f));
	if (UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(CenterBackdrop))
	{
		CanvasSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		CanvasSlot->SetAutoSize(true);
	}

	UVerticalBox* RootColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainMenuRootColumn"));
	CenterBackdrop->AddChild(RootColumn);

	UTextBlock* TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MainMenuTitleText"));
	FSlateFontInfo TitleFont = TitleText->GetFont();
	TitleFont.Size = 42;
	TitleText->SetFont(TitleFont);
	TitleText->SetText(NSLOCTEXT("ArenaMainMenu", "Title", "PROJECT ARCANE ARENA"));
	TitleText->SetJustification(ETextJustify::Center);
	TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.78f, 0.86f, 1.0f, 1.0f)));
	if (UVerticalBoxSlot* VerticalSlot = RootColumn->AddChildToVerticalBox(TitleText))
	{
		VerticalSlot->SetHorizontalAlignment(HAlign_Center);
		VerticalSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 28.0f));
	}

	MenuPageSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("MenuPageSwitcher"));
	if (UVerticalBoxSlot* VerticalSlot = RootColumn->AddChildToVerticalBox(MenuPageSwitcher))
	{
		VerticalSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	UVerticalBox* FrontPage = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FrontPage"));
	SinglePlayerButton = ArenaMainMenuWidget::AddVerticalButton(WidgetTree, FrontPage, TEXT("SinglePlayerButton"), TEXT("SinglePlayerButtonText"), NSLOCTEXT("ArenaMainMenu", "SinglePlayer", "单人游戏"));
	MultiplayerButton = ArenaMainMenuWidget::AddVerticalButton(WidgetTree, FrontPage, TEXT("MultiplayerButton"), TEXT("MultiplayerButtonText"), NSLOCTEXT("ArenaMainMenu", "Multiplayer", "多人游戏"));
	QuitGameButton = ArenaMainMenuWidget::AddVerticalButton(WidgetTree, FrontPage, TEXT("QuitGameButton"), TEXT("QuitGameButtonText"), NSLOCTEXT("ArenaMainMenu", "QuitGame", "退出游戏"));
	MenuPageSwitcher->AddChild(FrontPage);

	UVerticalBox* MultiplayerPage = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MultiplayerSetupPage"));
	ArenaMainMenuWidget::AddPageTitle(WidgetTree, MultiplayerPage, NSLOCTEXT("ArenaMainMenu", "DirectIPTitle", "DIRECT IP MULTIPLAYER"));
	UTextBlock* CapacityLabel = WidgetTree->ConstructWidget<UTextBlock>();
	CapacityLabel->SetText(NSLOCTEXT("ArenaMainMenu", "CapacityLabel", "Host Capacity"));
	CapacityLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	MultiplayerPage->AddChildToVerticalBox(CapacityLabel)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
	HostMaxPlayersComboBox = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("HostMaxPlayersComboBox"));
	HostMaxPlayersComboBox->AddOption(TEXT("2"));
	HostMaxPlayersComboBox->AddOption(TEXT("3"));
	HostMaxPlayersComboBox->AddOption(TEXT("4"));
	HostMaxPlayersComboBox->SetSelectedOption(TEXT("2"));
	MultiplayerPage->AddChildToVerticalBox(HostMaxPlayersComboBox)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
	HostLobbyButton = ArenaMainMenuWidget::AddVerticalButton(WidgetTree, MultiplayerPage, TEXT("HostLobbyButton"), TEXT("HostLobbyButtonText"), NSLOCTEXT("ArenaMainMenu", "CreateLobby", "创建房间"));
	UTextBlock* AddressLabel = WidgetTree->ConstructWidget<UTextBlock>();
	AddressLabel->SetText(NSLOCTEXT("ArenaMainMenu", "AddressLabel", "IPv4[:Port]"));
	AddressLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	MultiplayerPage->AddChildToVerticalBox(AddressLabel)->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 6.0f));
	JoinAddressTextBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("JoinAddressTextBox"));
	JoinAddressTextBox->SetText(FText::FromString(TEXT("127.0.0.1:7777")));
	JoinAddressTextBox->SetHintText(NSLOCTEXT("ArenaMainMenu", "AddressHint", "192.168.1.10:7777"));
	MultiplayerPage->AddChildToVerticalBox(JoinAddressTextBox)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
	JoinLobbyButton = ArenaMainMenuWidget::AddVerticalButton(WidgetTree, MultiplayerPage, TEXT("JoinLobbyButton"), TEXT("JoinLobbyButtonText"), NSLOCTEXT("ArenaMainMenu", "JoinLobby", "加入房间"));
	MultiplayerBackButton = ArenaMainMenuWidget::AddVerticalButton(WidgetTree, MultiplayerPage, TEXT("MultiplayerBackButton"), TEXT("MultiplayerBackButtonText"), NSLOCTEXT("ArenaMainMenu", "Back", "返回"));
	MenuPageSwitcher->AddChild(MultiplayerPage);

	UVerticalBox* LobbyPage = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LobbyPage"));
	ArenaMainMenuWidget::AddPageTitle(WidgetTree, LobbyPage, NSLOCTEXT("ArenaMainMenu", "LobbyTitle", "LOBBY"));
	LobbyStatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LobbyStatusText"));
	LobbyStatusText->SetJustification(ETextJustify::Center);
	LobbyStatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.82f, 1.0f, 1.0f)));
	LobbyPage->AddChildToVerticalBox(LobbyStatusText)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
	LobbyPlayersText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LobbyPlayersText"));
	FSlateFontInfo PlayerFont = LobbyPlayersText->GetFont();
	PlayerFont.Size = 18;
	LobbyPlayersText->SetFont(PlayerFont);
	LobbyPlayersText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	USizeBox* PlayerListSize = WidgetTree->ConstructWidget<USizeBox>();
	PlayerListSize->SetWidthOverride(500.0f);
	PlayerListSize->SetHeightOverride(170.0f);
	PlayerListSize->AddChild(LobbyPlayersText);
	LobbyPage->AddChildToVerticalBox(PlayerListSize)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));
	UTextBlock* ReadyLabel = nullptr;
	LobbyReadyButton = ArenaMainMenuWidget::AddVerticalButton(WidgetTree, LobbyPage, TEXT("LobbyReadyButton"), TEXT("LobbyReadyButtonText"), NSLOCTEXT("ArenaMainMenu", "Ready", "准备"), &ReadyLabel);
	LobbyReadyButtonText = ReadyLabel;
	LobbyStartButton = ArenaMainMenuWidget::AddVerticalButton(WidgetTree, LobbyPage, TEXT("LobbyStartButton"), TEXT("LobbyStartButtonText"), NSLOCTEXT("ArenaMainMenu", "StartMatch", "开始战斗"));
	LobbyLeaveButton = ArenaMainMenuWidget::AddVerticalButton(WidgetTree, LobbyPage, TEXT("LobbyLeaveButton"), TEXT("LobbyLeaveButtonText"), NSLOCTEXT("ArenaMainMenu", "LeaveLobby", "离开房间"));
	MenuPageSwitcher->AddChild(LobbyPage);

	UVerticalBox* ConnectingPage = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ConnectingPage"));
	ArenaMainMenuWidget::AddPageTitle(WidgetTree, ConnectingPage, NSLOCTEXT("ArenaMainMenu", "ConnectingTitle", "CONNECTING"));
	ConnectingStatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ConnectingStatusText"));
	ConnectingStatusText->SetText(NSLOCTEXT("ArenaMainMenu", "Connecting", "正在连接..."));
	ConnectingStatusText->SetJustification(ETextJustify::Center);
	ConnectingStatusText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	ConnectingPage->AddChildToVerticalBox(ConnectingStatusText);
	MenuPageSwitcher->AddChild(ConnectingPage);

	MenuErrorText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MenuErrorText"));
	MenuErrorText->SetAutoWrapText(true);
	MenuErrorText->SetJustification(ETextJustify::Center);
	MenuErrorText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.32f, 0.28f, 1.0f)));
	MenuErrorText->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* VerticalSlot = RootColumn->AddChildToVerticalBox(MenuErrorText))
	{
		VerticalSlot->SetPadding(FMargin(0.0f, 16.0f, 0.0f, 0.0f));
		VerticalSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	MenuPageSwitcher->SetActiveWidgetIndex(ArenaMainMenuWidget::FrontPageIndex);
}

// 单人请求不携带网络参数，Controller 继续使用原本的本地 OpenLevel 路径。
void UArenaMainMenuWidget::HandleSinglePlayerClicked()
{
	OnSinglePlayerRequested.Broadcast();
}

// 多人首页按钮只请求页面切换，不直接创建会话。
void UArenaMainMenuWidget::HandleMultiplayerClicked()
{
	OnMultiplayerRequested.Broadcast();
}

// 下拉框异常时默认使用两人容量，Subsystem 和 GameMode 仍会再次限制范围。
void UArenaMainMenuWidget::HandleHostLobbyClicked()
{
	const FString SelectedOption = HostMaxPlayersComboBox ? HostMaxPlayersComboBox->GetSelectedOption() : TEXT("2");
	OnHostLobbyRequested.Broadcast(FMath::Clamp(FCString::Atoi(*SelectedOption), 2, 4));
}

// 地址保持原始文本交给 DirectConnectSubsystem 做唯一规范化和错误提示。
void UArenaMainMenuWidget::HandleJoinLobbyClicked()
{
	OnJoinLobbyRequested.Broadcast(JoinAddressTextBox ? JoinAddressTextBox->GetText().ToString() : FString());
}

// 返回意图由 Controller 统一处理，以便连接状态和错误文本同步复位。
void UArenaMainMenuWidget::HandleBackClicked()
{
	OnBackRequested.Broadcast();
}

// Ready 按钮发送目标状态而不是本地翻转显示，等待服务器复制确认。
void UArenaMainMenuWidget::HandleLobbyReadyClicked()
{
	OnLobbyReadyRequested.Broadcast(!CurrentLobbyViewData.bLocalPlayerIsReady);
}

// Host Start 只发送一次操作意图，按钮资格来自服务器状态的只读投影。
void UArenaMainMenuWidget::HandleLobbyStartClicked()
{
	OnLobbyStartRequested.Broadcast();
}

// 离开房间由本地 Subsystem 执行断开，Widget 不持有网络连接对象。
void UArenaMainMenuWidget::HandleLobbyLeaveClicked()
{
	OnLobbyLeaveRequested.Broadcast();
}

// 退出行为继续由 Controller 调用平台统一接口，View 不区分 PIE 与 Standalone。
void UArenaMainMenuWidget::HandleQuitGameClicked()
{
	OnQuitGameRequested.Broadcast();
}
