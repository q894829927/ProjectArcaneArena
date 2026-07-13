#include "UI/ArenaUpgradeSelectionWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/ArenaUpgradeDataAsset.h"

// Slate 重建前准备蓝图缺失时的原生 WidgetTree，确保 fallback 界面真正可见。
void UArenaUpgradeSelectionWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!UpgradeChoiceButton0 || !UpgradeChoiceButton1 || !UpgradeChoiceButton2)
	{
		BuildFallbackLayout();
	}
}

// Widget 加入视口后绑定按钮并以折叠状态等待服务器候选。
void UArenaUpgradeSelectionWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindChoiceButtons();
	SetVisibility(ESlateVisibility::Collapsed);
}

// 仅展示 PlayerState 已复制的候选，并把最终选择意图交还 Controller。
void UArenaUpgradeSelectionWidget::ShowUpgradeChoices(const TArray<UArenaUpgradeDataAsset*>& InChoices)
{
	CurrentChoices.Reset();
	for (UArenaUpgradeDataAsset* Choice : InChoices)
	{
		if (Choice)
		{
			CurrentChoices.Add(Choice);
		}
	}

	RefreshChoiceVisuals();
	SetVisibility(CurrentChoices.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	K2_OnUpgradeChoicesChanged();
}

// 清空本地候选并折叠界面，不修改 PlayerState 或服务器选择状态。
void UArenaUpgradeSelectionWidget::HideUpgradeChoices()
{
	CurrentChoices.Reset();
	SetVisibility(ESlateVisibility::Collapsed);
	K2_OnUpgradeChoicesChanged();
}

// 查找第一个可操作候选按钮，避免把不可聚焦的 UserWidget 容器交给 InputMode。
UWidget* UArenaUpgradeSelectionWidget::GetInitialFocusTarget() const
{
	UButton* Buttons[] = { UpgradeChoiceButton0, UpgradeChoiceButton1, UpgradeChoiceButton2 };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Buttons); ++Index)
	{
		UButton* Button = Buttons[Index];
		if (CurrentChoices.IsValidIndex(Index) && CurrentChoices[Index]
			&& Button && Button->GetIsEnabled() && Button->GetIsFocusable())
		{
			return Button;
		}
	}

	return nullptr;
}

// 蓝图未提供布局时创建可直接操作的原生三选一界面，后续可用 WBP 子类替换外观。
void UArenaUpgradeSelectionWidget::BuildFallbackLayout()
{
	if (!WidgetTree)
	{
		return;
	}

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("UpgradeRootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	UVerticalBox* ChoicePanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("UpgradeChoicePanel"));
	if (UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(ChoicePanel))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetPosition(FVector2D::ZeroVector);
		PanelSlot->SetSize(FVector2D(
			FMath::Max(1.0f, UpgradePanelSize.X),
			FMath::Max(1.0f, UpgradePanelSize.Y)));
	}

	UTextBlock* HeaderText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("UpgradeHeaderText"));
	HeaderText->SetText(FText::FromString(TEXT("Choose an Upgrade")));
	HeaderText->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* HeaderSlot = ChoicePanel->AddChildToVerticalBox(HeaderText))
	{
		HeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 24.0f));
	}

	UHorizontalBox* ChoiceRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("UpgradeChoiceRow"));
	ChoicePanel->AddChildToVerticalBox(ChoiceRow);

	auto AddChoice = [this, ChoiceRow](
		const FName ButtonName,
		const FName IconName,
		const FName TextName,
		TObjectPtr<UButton>& OutButton,
		TObjectPtr<UImage>& OutIcon,
		TObjectPtr<UTextBlock>& OutText)
	{
		OutButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), ButtonName);
		UVerticalBox* ButtonContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		OutIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), IconName);

		// SizeBox 固定升级图标的布局尺寸，避免 Image 的期望尺寸被父布局压缩。
		USizeBox* IconSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		IconSizeBox->SetWidthOverride(FMath::Max(1.0f, UpgradeIconSize.X));
		IconSizeBox->SetHeightOverride(FMath::Max(1.0f, UpgradeIconSize.Y));
		IconSizeBox->AddChild(OutIcon);

		OutText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TextName);
		OutText->SetJustification(ETextJustify::Center);
		OutText->SetAutoWrapText(true);
		if (UVerticalBoxSlot* IconSlot = ButtonContent->AddChildToVerticalBox(IconSizeBox))
		{
			IconSlot->SetHorizontalAlignment(HAlign_Center);
			IconSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 12.0f));
		}
		ButtonContent->AddChildToVerticalBox(OutText);
		OutButton->AddChild(ButtonContent);
		if (UHorizontalBoxSlot* ButtonSlot = ChoiceRow->AddChildToHorizontalBox(OutButton))
		{
			ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			ButtonSlot->SetPadding(FMargin(8.0f));
		}
	};

	AddChoice(TEXT("UpgradeChoiceButton0"), TEXT("UpgradeChoiceIcon0"), TEXT("UpgradeChoiceText0"), UpgradeChoiceButton0, UpgradeChoiceIcon0, UpgradeChoiceText0);
	AddChoice(TEXT("UpgradeChoiceButton1"), TEXT("UpgradeChoiceIcon1"), TEXT("UpgradeChoiceText1"), UpgradeChoiceButton1, UpgradeChoiceIcon1, UpgradeChoiceText1);
	AddChoice(TEXT("UpgradeChoiceButton2"), TEXT("UpgradeChoiceIcon2"), TEXT("UpgradeChoiceText2"), UpgradeChoiceButton2, UpgradeChoiceIcon2, UpgradeChoiceText2);
}

// 把三个按钮各自绑定到固定候选索引，重复 Construct 时避免重复委托。
void UArenaUpgradeSelectionWidget::BindChoiceButtons()
{
	if (UpgradeChoiceButton0)
	{
		UpgradeChoiceButton0->OnClicked.AddUniqueDynamic(this, &UArenaUpgradeSelectionWidget::HandleChoice0Clicked);
	}
	if (UpgradeChoiceButton1)
	{
		UpgradeChoiceButton1->OnClicked.AddUniqueDynamic(this, &UArenaUpgradeSelectionWidget::HandleChoice1Clicked);
	}
	if (UpgradeChoiceButton2)
	{
		UpgradeChoiceButton2->OnClicked.AddUniqueDynamic(this, &UArenaUpgradeSelectionWidget::HandleChoice2Clicked);
	}
}

// 根据候选数量刷新按钮、DataAsset 图标、名称和描述文本。
void UArenaUpgradeSelectionWidget::RefreshChoiceVisuals()
{
	UButton* Buttons[] = { UpgradeChoiceButton0, UpgradeChoiceButton1, UpgradeChoiceButton2 };
	UImage* Icons[] = { UpgradeChoiceIcon0, UpgradeChoiceIcon1, UpgradeChoiceIcon2 };
	UTextBlock* TextBlocks[] = { UpgradeChoiceText0, UpgradeChoiceText1, UpgradeChoiceText2 };

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Buttons); ++Index)
	{
		const bool bHasChoice = CurrentChoices.IsValidIndex(Index) && CurrentChoices[Index];
		if (Buttons[Index])
		{
			Buttons[Index]->SetVisibility(bHasChoice ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
		if (Icons[Index])
		{
			const bool bHasIcon = bHasChoice && !CurrentChoices[Index]->Icon.IsNull();
			Icons[Index]->SetVisibility(bHasIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			if (bHasIcon)
			{
				Icons[Index]->SetBrushFromSoftTexture(CurrentChoices[Index]->Icon, false);
			}
		}
		if (bHasChoice && TextBlocks[Index])
		{
			const UArenaUpgradeDataAsset* Choice = CurrentChoices[Index];
			TextBlocks[Index]->SetText(FText::FromString(FString::Printf(
				TEXT("%s\n\n%s"),
				*Choice->UpgradeName.ToString(),
				*Choice->Description.ToString())));
		}
	}
}

// 将有效索引转换为 UpgradeID 广播，Controller 负责发送服务器 RPC。
void UArenaUpgradeSelectionWidget::BroadcastChoice(int32 ChoiceIndex)
{
	if (CurrentChoices.IsValidIndex(ChoiceIndex) && CurrentChoices[ChoiceIndex])
	{
		OnUpgradeChosen.Broadcast(CurrentChoices[ChoiceIndex]->UpgradeID);
	}
}

// 选择第一个服务器候选。
void UArenaUpgradeSelectionWidget::HandleChoice0Clicked()
{
	BroadcastChoice(0);
}

// 选择第二个服务器候选。
void UArenaUpgradeSelectionWidget::HandleChoice1Clicked()
{
	BroadcastChoice(1);
}

// 选择第三个服务器候选。
void UArenaUpgradeSelectionWidget::HandleChoice2Clicked()
{
	BroadcastChoice(2);
}
