#include "UI/ArenaPauseMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "InputCoreTypes.h"

namespace ArenaPauseMenuWidget
{
	// 为 fallback 按钮设置统一尺寸和高对比交互状态。
	UButton* AddMenuButton(
		UWidgetTree* WidgetTree,
		UVerticalBox* Parent,
		const FName ButtonName,
		const FName LabelName,
		const FText& LabelText)
	{
		if (!WidgetTree || !Parent)
		{
			return nullptr;
		}

		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), ButtonName);
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), LabelName);
		FButtonStyle ButtonStyle = Button->GetStyle();
		ButtonStyle.Normal.DrawAs = ESlateBrushDrawType::Box;
		ButtonStyle.Normal.TintColor = FSlateColor(FLinearColor(0.055f, 0.075f, 0.13f, 0.98f));
		ButtonStyle.Hovered.DrawAs = ESlateBrushDrawType::Box;
		ButtonStyle.Hovered.TintColor = FSlateColor(FLinearColor(0.12f, 0.22f, 0.38f, 1.0f));
		ButtonStyle.Pressed.DrawAs = ESlateBrushDrawType::Box;
		ButtonStyle.Pressed.TintColor = FSlateColor(FLinearColor(0.03f, 0.05f, 0.1f, 1.0f));
		ButtonStyle.NormalPadding = FMargin(12.0f, 8.0f);
		ButtonStyle.PressedPadding = FMargin(12.0f, 10.0f, 12.0f, 6.0f);
		Button->SetStyle(ButtonStyle);

		FSlateFontInfo LabelFont = Label->GetFont();
		LabelFont.Size = 20;
		Label->SetFont(LabelFont);
		Label->SetText(LabelText);
		Label->SetJustification(ETextJustify::Center);
		Label->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.96f, 1.0f, 1.0f)));
		Button->AddChild(Label);

		USizeBox* ButtonSize = WidgetTree->ConstructWidget<USizeBox>();
		ButtonSize->SetWidthOverride(320.0f);
		ButtonSize->SetHeightOverride(54.0f);
		ButtonSize->AddChild(Button);
		if (UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(ButtonSize))
		{
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
		}
		return Button;
	}
}

// 初始化可聚焦根节点，并在 Blueprint 控件不完整时使用稳定原生布局。
void UArenaPauseMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	if (!ResumeButton
		|| !SettingsButton
		|| !ReturnToMainMenuButton
		|| !QuitGameButton
		|| !SettingsPlaceholderText)
	{
		BuildFallbackLayout();
	}
}

// 每次进入视口时幂等绑定按钮，避免重复 AddToViewport 产生多次广播。
void UArenaPauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (ResumeButton) ResumeButton->OnClicked.AddUniqueDynamic(this, &UArenaPauseMenuWidget::HandleResumeClicked);
	if (SettingsButton) SettingsButton->OnClicked.AddUniqueDynamic(this, &UArenaPauseMenuWidget::HandleSettingsClicked);
	if (ReturnToMainMenuButton) ReturnToMainMenuButton->OnClicked.AddUniqueDynamic(this, &UArenaPauseMenuWidget::HandleReturnToMainMenuClicked);
	if (QuitGameButton) QuitGameButton->OnClicked.AddUniqueDynamic(this, &UArenaPauseMenuWidget::HandleQuitGameClicked);
}

// Widget 销毁前对称解绑按钮，旅行后新 Controller 不会收到旧界面回调。
void UArenaPauseMenuWidget::NativeDestruct()
{
	if (ResumeButton) ResumeButton->OnClicked.RemoveDynamic(this, &UArenaPauseMenuWidget::HandleResumeClicked);
	if (SettingsButton) SettingsButton->OnClicked.RemoveDynamic(this, &UArenaPauseMenuWidget::HandleSettingsClicked);
	if (ReturnToMainMenuButton) ReturnToMainMenuButton->OnClicked.RemoveDynamic(this, &UArenaPauseMenuWidget::HandleReturnToMainMenuClicked);
	if (QuitGameButton) QuitGameButton->OnClicked.RemoveDynamic(this, &UArenaPauseMenuWidget::HandleQuitGameClicked);
	Super::NativeDestruct();
}

// UIOnly 下 Escape 由菜单根节点直接处理，点击和键盘关闭走同一 Controller 入口。
FReply UArenaPauseMenuWidget::NativeOnPreviewKeyDown(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		OnResumeRequested.Broadcast();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

// 显示菜单时隐藏旧设置占位提示并恢复继续按钮的初始焦点资格。
void UArenaPauseMenuWidget::ShowPauseMenu()
{
	if (SettingsPlaceholderText)
	{
		SettingsPlaceholderText->SetVisibility(ESlateVisibility::Collapsed);
	}
	SetVisibility(ESlateVisibility::Visible);
}

// 隐藏菜单并清理提示，输入与鼠标恢复由 Controller 按当前阶段统一决定。
void UArenaPauseMenuWidget::HidePauseMenu()
{
	if (SettingsPlaceholderText)
	{
		SettingsPlaceholderText->SetVisibility(ESlateVisibility::Collapsed);
	}
	SetVisibility(ESlateVisibility::Collapsed);
}

// 优先聚焦继续按钮，使 Enter/Space 和 Escape 都有稳定键盘行为。
UWidget* UArenaPauseMenuWidget::GetInitialFocusTarget() const
{
	return ResumeButton
		? static_cast<UWidget*>(ResumeButton.Get())
		: const_cast<UArenaPauseMenuWidget*>(this);
}

// 创建全屏遮罩与居中菜单，避免把页面区块包装成多层嵌套卡片。
void UArenaPauseMenuWidget::BuildFallbackLayout()
{
	if (!WidgetTree)
	{
		return;
	}

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PauseMenuRootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PauseMenuBackdrop"));
	Backdrop->SetBrushColor(FLinearColor(0.005f, 0.008f, 0.015f, 0.72f));
	if (UCanvasPanelSlot* BackdropSlot = RootCanvas->AddChildToCanvas(Backdrop))
	{
		BackdropSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		BackdropSlot->SetOffsets(FMargin(0.0f));
	}

	UVerticalBox* MenuColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PauseMenuColumn"));
	if (UCanvasPanelSlot* MenuSlot = RootCanvas->AddChildToCanvas(MenuColumn))
	{
		MenuSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		MenuSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		MenuSlot->SetAutoSize(true);
	}

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PauseMenuTitle"));
	FSlateFontInfo TitleFont = Title->GetFont();
	TitleFont.Size = 36;
	Title->SetFont(TitleFont);
	Title->SetText(NSLOCTEXT("ArenaPauseMenu", "Title", "游戏菜单"));
	Title->SetJustification(ETextJustify::Center);
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.82f, 0.9f, 1.0f, 1.0f)));
	if (UVerticalBoxSlot* TitleSlot = MenuColumn->AddChildToVerticalBox(Title))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Fill);
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 24.0f));
	}

	ResumeButton = ArenaPauseMenuWidget::AddMenuButton(
		WidgetTree, MenuColumn, TEXT("ResumeButton"), TEXT("ResumeButtonText"),
		NSLOCTEXT("ArenaPauseMenu", "Resume", "继续游戏"));
	SettingsButton = ArenaPauseMenuWidget::AddMenuButton(
		WidgetTree, MenuColumn, TEXT("SettingsButton"), TEXT("SettingsButtonText"),
		NSLOCTEXT("ArenaPauseMenu", "Settings", "设置"));
	ReturnToMainMenuButton = ArenaPauseMenuWidget::AddMenuButton(
		WidgetTree, MenuColumn, TEXT("ReturnToMainMenuButton"), TEXT("ReturnToMainMenuButtonText"),
		NSLOCTEXT("ArenaPauseMenu", "ReturnToMainMenu", "返回主菜单"));
	QuitGameButton = ArenaPauseMenuWidget::AddMenuButton(
		WidgetTree, MenuColumn, TEXT("QuitGameButton"), TEXT("QuitGameButtonText"),
		NSLOCTEXT("ArenaPauseMenu", "QuitGame", "退出游戏"));

	SettingsPlaceholderText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("SettingsPlaceholderText"));
	SettingsPlaceholderText->SetText(NSLOCTEXT(
		"ArenaPauseMenu", "SettingsPlaceholder", "设置功能暂未开放"));
	SettingsPlaceholderText->SetJustification(ETextJustify::Center);
	SettingsPlaceholderText->SetColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.72f, 0.82f, 1.0f)));
	SettingsPlaceholderText->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* PlaceholderSlot = MenuColumn->AddChildToVerticalBox(SettingsPlaceholderText))
	{
		PlaceholderSlot->SetHorizontalAlignment(HAlign_Fill);
		PlaceholderSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
	}
}

// 继续按钮与 Escape 使用相同关闭意图。
void UArenaPauseMenuWidget::HandleResumeClicked()
{
	OnResumeRequested.Broadcast();
}

// 设置页尚未开发时只显示明确占位，不让按钮表现成失效操作。
void UArenaPauseMenuWidget::HandleSettingsClicked()
{
	if (SettingsPlaceholderText)
	{
		SettingsPlaceholderText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

// 返回主菜单只提交意图，Host/Client 的离开差异由网络子系统处理。
void UArenaPauseMenuWidget::HandleReturnToMainMenuClicked()
{
	OnReturnToMainMenuRequested.Broadcast();
}

// 退出按钮只提交本地平台退出意图。
void UArenaPauseMenuWidget::HandleQuitGameClicked()
{
	OnQuitGameRequested.Broadcast();
}
