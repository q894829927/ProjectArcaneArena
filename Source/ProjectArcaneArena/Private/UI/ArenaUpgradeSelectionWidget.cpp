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
#include "InputCoreTypes.h"

// Slate 重建前启用根焦点；父级 Preview 路由会先于任意候选按钮拦截 Tab。
void UArenaUpgradeSelectionWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	SetIsFocusable(true);
	if (!UpgradeChoiceButton0 || !UpgradeChoiceButton1 || !UpgradeChoiceButton2)
	{
		BuildFallbackLayout();
	}
}

// Widget 加入视口后绑定按钮并默认折叠，焦点能力已在 Slate 重建前完成配置。
void UArenaUpgradeSelectionWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindChoiceButtons();
	SetVisibility(ESlateVisibility::Collapsed);
}

// Upgrade 由焦点路径优先捕获 Tab 按下，并交给 Controller 启动轻点/长按状态机。
FReply UArenaUpgradeSelectionWidget::NativeOnPreviewKeyDown(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Tab)
	{
		OnInventoryRequested.Broadcast();
		return FReply::Handled();
	}

	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

// Preview 未消费按键时在普通 KeyDown 路径兜底转发 Tab，避免蓝图焦点差异禁用只读背包。
FReply UArenaUpgradeSelectionWidget::NativeOnKeyDown(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Tab)
	{
		OnInventoryRequested.Broadcast();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

// 冒泡路径捕获 Tab 松开，供 Controller 完成轻点切换或长按临时关闭。
FReply UArenaUpgradeSelectionWidget::NativeOnKeyUp(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Tab)
	{
		OnInventoryTabReleased.Broadcast();
		return FReply::Handled();
	}

	return Super::NativeOnKeyUp(InGeometry, InKeyEvent);
}

// 仅展示 Controller 从 PlayerState 快照整理出的候选和层数，并把最终选择意图交还 Controller。
void UArenaUpgradeSelectionWidget::ShowUpgradeChoices(const TArray<FArenaUpgradeChoiceViewData>& InChoices)
{
	CurrentChoices.Reset();
	for (const FArenaUpgradeChoiceViewData& Choice : InChoices)
	{
		if (Choice.Upgrade)
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

// 优先聚焦第一个可用候选按钮以支持确认输入；父级 Preview 仍会先于子按钮接管 Tab。
UWidget* UArenaUpgradeSelectionWidget::GetInitialFocusTarget() const
{
	UButton* ChoiceButtons[] = {
		UpgradeChoiceButton0.Get(),
		UpgradeChoiceButton1.Get(),
		UpgradeChoiceButton2.Get()
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ChoiceButtons); ++Index)
	{
		UButton* ChoiceButton = ChoiceButtons[Index];
		if (CurrentChoices.IsValidIndex(Index)
			&& ChoiceButton
			&& ChoiceButton->GetIsEnabled()
			&& ChoiceButton->GetIsFocusable()
			&& ChoiceButton->GetVisibility() == ESlateVisibility::Visible)
		{
			return ChoiceButton;
		}
	}

	return const_cast<UArenaUpgradeSelectionWidget*>(this);
}

// 蓝图未提供布局时创建可直接操作且支持本地化的原生三选一界面，后续可用 WBP 子类替换外观。
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
	HeaderText->SetText(NSLOCTEXT("ArenaUpgrade", "ChooseUpgradeHeader", "Choose an Upgrade"));
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
		const FName RarityTextName,
		const FName StackTextName,
		TObjectPtr<UButton>& OutButton,
		TObjectPtr<UImage>& OutIcon,
		TObjectPtr<UTextBlock>& OutText,
		TObjectPtr<UTextBlock>& OutRarityText,
		TObjectPtr<UTextBlock>& OutStackText)
	{
		OutButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), ButtonName);
		UVerticalBox* ButtonContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		OutIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), IconName);
		OutRarityText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), RarityTextName);
		OutStackText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), StackTextName);

		// SizeBox 固定升级图标的布局尺寸，避免 Image 的期望尺寸被父布局压缩。
		USizeBox* IconSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		IconSizeBox->SetWidthOverride(FMath::Max(1.0f, UpgradeIconSize.X));
		IconSizeBox->SetHeightOverride(FMath::Max(1.0f, UpgradeIconSize.Y));
		IconSizeBox->AddChild(OutIcon);

		OutText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TextName);
		OutText->SetJustification(ETextJustify::Center);
		OutText->SetAutoWrapText(true);
		OutRarityText->SetJustification(ETextJustify::Center);
		OutStackText->SetJustification(ETextJustify::Center);
		ButtonContent->AddChildToVerticalBox(OutRarityText);
		if (UVerticalBoxSlot* IconSlot = ButtonContent->AddChildToVerticalBox(IconSizeBox))
		{
			IconSlot->SetHorizontalAlignment(HAlign_Center);
			IconSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 12.0f));
		}
		ButtonContent->AddChildToVerticalBox(OutText);
		if (UVerticalBoxSlot* StackSlot = ButtonContent->AddChildToVerticalBox(OutStackText))
		{
			StackSlot->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 8.0f));
		}
		OutButton->AddChild(ButtonContent);
		if (UHorizontalBoxSlot* ButtonSlot = ChoiceRow->AddChildToHorizontalBox(OutButton))
		{
			ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			ButtonSlot->SetPadding(FMargin(8.0f));
		}
	};

	AddChoice(
		TEXT("UpgradeChoiceButton0"), TEXT("UpgradeChoiceIcon0"), TEXT("UpgradeChoiceText0"),
		TEXT("UpgradeChoiceRarityText0"), TEXT("UpgradeChoiceStackText0"),
		UpgradeChoiceButton0, UpgradeChoiceIcon0, UpgradeChoiceText0,
		UpgradeChoiceRarityText0, UpgradeChoiceStackText0);
	AddChoice(
		TEXT("UpgradeChoiceButton1"), TEXT("UpgradeChoiceIcon1"), TEXT("UpgradeChoiceText1"),
		TEXT("UpgradeChoiceRarityText1"), TEXT("UpgradeChoiceStackText1"),
		UpgradeChoiceButton1, UpgradeChoiceIcon1, UpgradeChoiceText1,
		UpgradeChoiceRarityText1, UpgradeChoiceStackText1);
	AddChoice(
		TEXT("UpgradeChoiceButton2"), TEXT("UpgradeChoiceIcon2"), TEXT("UpgradeChoiceText2"),
		TEXT("UpgradeChoiceRarityText2"), TEXT("UpgradeChoiceStackText2"),
		UpgradeChoiceButton2, UpgradeChoiceIcon2, UpgradeChoiceText2,
		UpgradeChoiceRarityText2, UpgradeChoiceStackText2);
}

// 把三个候选按钮绑定到固定候选索引，重复 Construct 时避免重复委托。
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

// 根据候选数量刷新图标、稀有度、说明和选择后等级，旧蓝图缺少新控件时用 FText 合并并保留本地化历史。
void UArenaUpgradeSelectionWidget::RefreshChoiceVisuals()
{
	UButton* Buttons[] = { UpgradeChoiceButton0, UpgradeChoiceButton1, UpgradeChoiceButton2 };
	UImage* Icons[] = { UpgradeChoiceIcon0, UpgradeChoiceIcon1, UpgradeChoiceIcon2 };
	UTextBlock* TextBlocks[] = { UpgradeChoiceText0, UpgradeChoiceText1, UpgradeChoiceText2 };
	UTextBlock* RarityTextBlocks[] = { UpgradeChoiceRarityText0, UpgradeChoiceRarityText1, UpgradeChoiceRarityText2 };
	UTextBlock* StackTextBlocks[] = { UpgradeChoiceStackText0, UpgradeChoiceStackText1, UpgradeChoiceStackText2 };

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Buttons); ++Index)
	{
		const bool bHasChoice = CurrentChoices.IsValidIndex(Index) && CurrentChoices[Index].Upgrade;
		const FArenaUpgradeChoiceViewData* ChoiceView = bHasChoice ? &CurrentChoices[Index] : nullptr;
		const UArenaUpgradeDataAsset* Choice = ChoiceView ? ChoiceView->Upgrade.Get() : nullptr;
		if (Buttons[Index])
		{
			Buttons[Index]->SetVisibility(bHasChoice ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
		if (Icons[Index])
		{
			const bool bHasIcon = Choice && !Choice->Icon.IsNull();
			Icons[Index]->SetVisibility(bHasIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			if (bHasIcon)
			{
				Icons[Index]->SetBrushFromSoftTexture(Choice->Icon, false);
			}
		}

		const FText RarityText = Choice ? GetRarityDisplayText(Choice->Rarity) : FText::GetEmpty();
		const FText StackText = ChoiceView ? GetStackDisplayText(*ChoiceView) : FText::GetEmpty();
		if (RarityTextBlocks[Index])
		{
			RarityTextBlocks[Index]->SetVisibility(bHasChoice ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			RarityTextBlocks[Index]->SetText(RarityText);
			if (Choice)
			{
				RarityTextBlocks[Index]->SetColorAndOpacity(FSlateColor(GetRarityDisplayColor(Choice->Rarity)));
			}
		}
		if (StackTextBlocks[Index])
		{
			StackTextBlocks[Index]->SetVisibility(bHasChoice ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			StackTextBlocks[Index]->SetText(StackText);
		}

		if (Choice && TextBlocks[Index])
		{
			FText DisplayText = FText::Format(
				NSLOCTEXT("ArenaUpgrade", "ChoiceNameDescriptionFormat", "{0}\n\n{1}"),
				Choice->UpgradeName,
				Choice->Description);
			if (!RarityTextBlocks[Index])
			{
				DisplayText = FText::Format(
					NSLOCTEXT("ArenaUpgrade", "ChoiceWithRarityFormat", "[{0}]\n{1}"),
					RarityText,
					DisplayText);
			}
			if (!StackTextBlocks[Index])
			{
				DisplayText = FText::Format(
					NSLOCTEXT("ArenaUpgrade", "ChoiceWithStackFormat", "{0}\n\n{1}"),
					DisplayText,
					StackText);
			}
			TextBlocks[Index]->SetText(DisplayText);
		}
		else if (TextBlocks[Index])
		{
			TextBlocks[Index]->SetText(FText::GetEmpty());
		}
	}
}

// 将升级稀有度转换为可本地化的英文短标签，避免 UI 依赖枚举内部名称。
FText UArenaUpgradeSelectionWidget::GetRarityDisplayText(EArenaUpgradeRarity Rarity) const
{
	switch (Rarity)
	{
	case EArenaUpgradeRarity::Rare:
		return NSLOCTEXT("ArenaUpgrade", "RarityRare", "Rare");
	case EArenaUpgradeRarity::Epic:
		return NSLOCTEXT("ArenaUpgrade", "RarityEpic", "Epic");
	case EArenaUpgradeRarity::Legendary:
		return NSLOCTEXT("ArenaUpgrade", "RarityLegendary", "Legendary");
	case EArenaUpgradeRarity::Common:
	default:
		return NSLOCTEXT("ArenaUpgrade", "RarityCommon", "Common");
	}
}

// 为 Common、Rare、Epic 和 Legendary 提供灰白、蓝、紫、金的默认视觉区分。
FLinearColor UArenaUpgradeSelectionWidget::GetRarityDisplayColor(EArenaUpgradeRarity Rarity) const
{
	switch (Rarity)
	{
	case EArenaUpgradeRarity::Rare:
		return FLinearColor(0.302f, 0.639f, 1.0f, 1.0f);
	case EArenaUpgradeRarity::Epic:
		return FLinearColor(0.710f, 0.424f, 1.0f, 1.0f);
	case EArenaUpgradeRarity::Legendary:
		return FLinearColor(1.0f, 0.710f, 0.180f, 1.0f);
	case EArenaUpgradeRarity::Common:
	default:
		return FLinearColor(0.845f, 0.845f, 0.845f, 1.0f);
	}
}

// 使用 Controller 计算的选择后层数显示 Lv. N/Max，并再次夹取数值防止异常配置溢出。
FText UArenaUpgradeSelectionWidget::GetStackDisplayText(const FArenaUpgradeChoiceViewData& Choice) const
{
	const int32 SafeMaxStacks = FMath::Max(Choice.MaxStacks, 1);
	const int32 SafeResultingStacks = FMath::Clamp(Choice.ResultingStacks, 1, SafeMaxStacks);
	return FText::Format(
		NSLOCTEXT("ArenaUpgrade", "ResultingStackLevel", "Lv. {0}/{1}"),
		FText::AsNumber(SafeResultingStacks),
		FText::AsNumber(SafeMaxStacks));
}

// 将有效索引转换为 UpgradeID 广播，Controller 负责发送服务器 RPC。
void UArenaUpgradeSelectionWidget::BroadcastChoice(int32 ChoiceIndex)
{
	if (CurrentChoices.IsValidIndex(ChoiceIndex) && CurrentChoices[ChoiceIndex].Upgrade)
	{
		OnUpgradeChosen.Broadcast(CurrentChoices[ChoiceIndex].Upgrade->UpgradeID);
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
