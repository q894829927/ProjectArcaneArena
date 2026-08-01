#include "UI/ArenaMainMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace ArenaMainMenuWidget
{
	// 为原生 fallback 按钮设置统一尺寸、状态颜色和文字层级。
	void ConfigureMenuButton(UButton* Button, UTextBlock* Label, const FText& LabelText)
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
		ButtonStyle.NormalPadding = FMargin(16.0f, 10.0f);
		ButtonStyle.PressedPadding = FMargin(16.0f, 12.0f, 16.0f, 8.0f);
		Button->SetStyle(ButtonStyle);

		FSlateFontInfo LabelFont = Label->GetFont();
		LabelFont.Size = 22;
		Label->SetFont(LabelFont);
		Label->SetText(LabelText);
		Label->SetJustification(ETextJustify::Center);
		Label->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.94f, 1.0f, 1.0f)));
		Label->SetShadowOffset(FVector2D(1.0f, 2.0f));
		Label->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
		Button->AddChild(Label);
	}
}

// 在 Slate 初始化前启用根焦点，并在 WBP 缺少约定按钮时生成完整 fallback。
void UArenaMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	SetIsFocusable(true);
	if (!StartGameButton || !QuitGameButton)
	{
		BuildFallbackLayout();
	}
}

// 进入视口后幂等绑定按钮，View 只负责广播用户意图。
void UArenaMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (StartGameButton)
	{
		StartGameButton->OnClicked.AddUniqueDynamic(this, &UArenaMainMenuWidget::HandleStartGameClicked);
	}
	if (QuitGameButton)
	{
		QuitGameButton->OnClicked.AddUniqueDynamic(this, &UArenaMainMenuWidget::HandleQuitGameClicked);
	}
	SetMenuInteractionEnabled(true);
}

// 离开视口时对称移除动态委托，支持旅行失败或 Widget 重建后的干净生命周期。
void UArenaMainMenuWidget::NativeDestruct()
{
	if (StartGameButton)
	{
		StartGameButton->OnClicked.RemoveDynamic(this, &UArenaMainMenuWidget::HandleStartGameClicked);
	}
	if (QuitGameButton)
	{
		QuitGameButton->OnClicked.RemoveDynamic(this, &UArenaMainMenuWidget::HandleQuitGameClicked);
	}

	Super::NativeDestruct();
}

// 同步切换两个操作按钮，防止连续点击产生重复 OpenLevel 或退出请求。
void UArenaMainMenuWidget::SetMenuInteractionEnabled(const bool bEnabled)
{
	if (StartGameButton)
	{
		StartGameButton->SetIsEnabled(bEnabled);
	}
	if (QuitGameButton)
	{
		QuitGameButton->SetIsEnabled(bEnabled);
	}
}

// 优先把焦点交给开始按钮，缺少可聚焦按钮时仍保持根 Widget 可接收键盘输入。
UWidget* UArenaMainMenuWidget::GetInitialFocusTarget() const
{
	if (StartGameButton && StartGameButton->GetIsEnabled() && StartGameButton->GetIsFocusable())
	{
		return StartGameButton;
	}

	return const_cast<UArenaMainMenuWidget*>(this);
}

// 构建不依赖 Blueprint WidgetTree 的全屏菜单，确保生成的空壳 WBP 也能直接运行。
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
	if (UCanvasPanelSlot* BackgroundSlot = RootCanvas->AddChildToCanvas(Background))
	{
		BackgroundSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		BackgroundSlot->SetOffsets(FMargin(0.0f));
	}

	UBorder* CenterBackdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MainMenuCenterBackdrop"));
	CenterBackdrop->SetBrushColor(FLinearColor(0.025f, 0.035f, 0.075f, 0.82f));
	CenterBackdrop->SetPadding(FMargin(54.0f, 42.0f));
	if (UCanvasPanelSlot* BackdropSlot = RootCanvas->AddChildToCanvas(CenterBackdrop))
	{
		BackdropSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		BackdropSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		BackdropSlot->SetAutoSize(true);
	}

	UVerticalBox* MenuColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainMenuColumn"));
	CenterBackdrop->AddChild(MenuColumn);

	UTextBlock* TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MainMenuTitleText"));
	FSlateFontInfo TitleFont = TitleText->GetFont();
	TitleFont.Size = 46;
	TitleText->SetFont(TitleFont);
	TitleText->SetText(NSLOCTEXT("ArenaMainMenu", "Title", "PROJECT ARCANE ARENA"));
	TitleText->SetJustification(ETextJustify::Center);
	TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.78f, 0.86f, 1.0f, 1.0f)));
	TitleText->SetShadowOffset(FVector2D(2.0f, 3.0f));
	TitleText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));
	if (UVerticalBoxSlot* TitleSlot = MenuColumn->AddChildToVerticalBox(TitleText))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 46.0f));
	}

	StartGameButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("StartGameButton"));
	UTextBlock* StartLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StartGameButtonText"));
	ArenaMainMenuWidget::ConfigureMenuButton(
		StartGameButton,
		StartLabel,
		NSLOCTEXT("ArenaMainMenu", "StartGame", "开始游戏"));
	USizeBox* StartSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("StartGameSizeBox"));
	StartSizeBox->SetWidthOverride(360.0f);
	StartSizeBox->SetHeightOverride(68.0f);
	StartSizeBox->AddChild(StartGameButton);
	if (UVerticalBoxSlot* StartSlot = MenuColumn->AddChildToVerticalBox(StartSizeBox))
	{
		StartSlot->SetHorizontalAlignment(HAlign_Center);
		StartSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));
	}

	QuitGameButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("QuitGameButton"));
	UTextBlock* QuitLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("QuitGameButtonText"));
	ArenaMainMenuWidget::ConfigureMenuButton(
		QuitGameButton,
		QuitLabel,
		NSLOCTEXT("ArenaMainMenu", "QuitGame", "退出游戏"));
	USizeBox* QuitSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("QuitGameSizeBox"));
	QuitSizeBox->SetWidthOverride(360.0f);
	QuitSizeBox->SetHeightOverride(68.0f);
	QuitSizeBox->AddChild(QuitGameButton);
	if (UVerticalBoxSlot* QuitSlot = MenuColumn->AddChildToVerticalBox(QuitSizeBox))
	{
		QuitSlot->SetHorizontalAlignment(HAlign_Center);
	}
}

// 广播开始游戏请求，具体旅行由本地 MainMenu PlayerController 决定。
void UArenaMainMenuWidget::HandleStartGameClicked()
{
	OnStartGameRequested.Broadcast();
}

// 广播退出游戏请求，具体平台行为由本地 MainMenu PlayerController 决定。
void UArenaMainMenuWidget::HandleQuitGameClicked()
{
	OnQuitGameRequested.Broadcast();
}
