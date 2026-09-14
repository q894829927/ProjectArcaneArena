#include "UI/ArenaInventoryWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/GridPanel.h"
#include "Components/GridSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GAS/ArenaGameplayTags.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Item/ArenaInventoryTypes.h"
#include "Item/ArenaItemDataAsset.h"
#include "UI/ArenaInventorySlotWidget.h"

namespace
{
	// 为原生文字设置明确字号，避免项目 DPI 或默认字体把紧凑工具面板撑开。
	void SetTextSize(UTextBlock* TextBlock, const int32 FontSize)
	{
		if (!TextBlock)
		{
			return;
		}

		FSlateFontInfo FontInfo = TextBlock->GetFont();
		FontInfo.Size = FontSize;
		TextBlock->SetFont(FontInfo);
	}

	// 为原生按钮创建统一文本内容，避免每个控制重复构造布局。
	UButton* AddTextButton(
		UWidgetTree* WidgetTree,
		UHorizontalBox* Parent,
		const FName ButtonName,
		const FText& Label)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), ButtonName);
		UTextBlock* ButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		ButtonText->SetText(Label);
		ButtonText->SetJustification(ETextJustify::Center);
		SetTextSize(ButtonText, 14);
		Button->AddChild(ButtonText);
		Button->SetBackgroundColor(FLinearColor(0.30f, 0.34f, 0.40f, 1.0f));
		if (UHorizontalBoxSlot* ButtonSlot = Parent->AddChildToHorizontalBox(Button))
		{
			ButtonSlot->SetPadding(FMargin(3.0f));
		}
		return Button;
	}

	// 添加一个可多选的筛选 CheckBox 和文字标签。
	UCheckBox* AddFilterCheckBox(
		UWidgetTree* WidgetTree,
		UHorizontalBox* Parent,
		const FName CheckBoxName,
		const FText& Label)
	{
		UHorizontalBox* FilterPair = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UCheckBox* CheckBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), CheckBoxName);
		UTextBlock* FilterText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FilterText->SetText(Label);
		SetTextSize(FilterText, 14);
		FilterPair->AddChildToHorizontalBox(CheckBox);
		if (UHorizontalBoxSlot* TextSlot = FilterPair->AddChildToHorizontalBox(FilterText))
		{
			TextSlot->SetPadding(FMargin(4.0f, 0.0f, 8.0f, 0.0f));
		}
		if (UHorizontalBoxSlot* PairSlot = Parent->AddChildToHorizontalBox(FilterPair))
		{
			PairSlot->SetVerticalAlignment(VAlign_Center);
		}
		return CheckBox;
	}
}

// 通过 ObjectInitializer 初始化 UUserWidget，并默认使用可被 WBP_Inventory 替换的原生槽位。
UArenaInventoryWidget::UArenaInventoryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InventorySlotWidgetClass = UArenaInventorySlotWidget::StaticClass();
}

// 初始化时先启用根键盘焦点再构建原生 View，确保输入模式切换时 Tab 不依赖子按钮焦点。
void UArenaInventoryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	BuildFallbackLayout();
}

// 加入视口时绑定控制并默认折叠，焦点能力已在 Slate 重建前完成配置。
void UArenaInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindControls();
	SetVisibility(ESlateVisibility::Collapsed);
}

// Preview 阶段优先消费 Tab/Escape，Tab 交给 Controller 区分轻点切换与长按临时查看。
FReply UArenaInventoryWidget::NativeOnPreviewKeyDown(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Tab)
	{
		OnTabPressed.Broadcast();
		return FReply::Handled();
	}
	if (Key == EKeys::Escape)
	{
		OnCloseRequested.Broadcast();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

// Tab 或 Escape 在 UI 获得键盘焦点时转发按住状态或请求关闭。
FReply UArenaInventoryWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Tab)
	{
		OnTabPressed.Broadcast();
		return FReply::Handled();
	}
	if (Key == EKeys::Escape)
	{
		OnCloseRequested.Broadcast();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

// KeyUp 冒泡路径转发 Tab 松开，兼容子按钮或自定义槽位持有焦点。
FReply UArenaInventoryWidget::NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Tab)
	{
		OnTabReleased.Broadcast();
		return FReply::Handled();
	}
	return Super::NativeOnKeyUp(InGeometry, InKeyEvent);
}

// 接收 Controller 已筛选分页的 ViewData，并展示二十个固定槽位。
void UArenaInventoryWidget::ShowInventoryPage(const FArenaInventoryPageViewData& InViewData)
{
	CurrentViewData = InViewData;
	CurrentViewData.CurrentPage = FMath::Max(CurrentViewData.CurrentPage, 1);
	CurrentViewData.TotalPages = FMath::Max(CurrentViewData.TotalPages, 1);
	RefreshVisuals();
	SetVisibility(ESlateVisibility::Visible);
	K2_OnInventoryPageUpdated(CurrentViewData);
}

// 折叠 View 并清除选择快照，服务器 Model 保持不变。
void UArenaInventoryWidget::HideInventory()
{
	CurrentViewData = FArenaInventoryPageViewData();
	if (DropConfirmPanel)
	{
		DropConfirmPanel->SetVisibility(ESlateVisibility::Hidden);
	}
	SetVisibility(ESlateVisibility::Collapsed);
	K2_OnInventoryHidden();
}

// 返回可聚焦的关闭按钮，缺失时回退到 Widget 本身。
UWidget* UArenaInventoryWidget::GetInitialFocusTarget() const
{
	return CloseButton ? static_cast<UWidget*>(CloseButton.Get()) : const_cast<UArenaInventoryWidget*>(this);
}

// 构建紧凑的左右分栏工具面板，固定网格与操作区高度，避免低分辨率下挤压 HUD。
void UArenaInventoryWidget::BuildFallbackLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("InventoryRootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InventoryBackdrop"));
	Backdrop->SetBrushColor(FLinearColor(0.005f, 0.008f, 0.015f, 0.58f));
	Backdrop->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* BackdropSlot = RootCanvas->AddChildToCanvas(Backdrop))
	{
		BackdropSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		BackdropSlot->SetOffsets(FMargin(0.0f));
		BackdropSlot->SetZOrder(0);
	}

	UBorder* PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InventoryPanelBorder"));
	PanelBorder->SetPadding(FMargin(16.0f));
	PanelBorder->SetBrushColor(FLinearColor(0.035f, 0.045f, 0.065f, 0.98f));
	if (UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(PanelBorder))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetPosition(FVector2D::ZeroVector);
		PanelSlot->SetSize(FVector2D(800.0f, 570.0f));
		PanelSlot->SetZOrder(1);
	}

	UVerticalBox* Panel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("InventoryPanel"));
	PanelBorder->AddChild(Panel);

	UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("InventoryHeaderRow"));
	UTextBlock* HeaderText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("InventoryHeaderText"));
	HeaderText->SetText(NSLOCTEXT("ArenaInventory", "InventoryHeader", "Inventory"));
	SetTextSize(HeaderText, 22);
	if (UHorizontalBoxSlot* HeaderTextSlot = HeaderRow->AddChildToHorizontalBox(HeaderText))
	{
		HeaderTextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		HeaderTextSlot->SetVerticalAlignment(VAlign_Center);
	}
	CloseButton = AddTextButton(
		WidgetTree,
		HeaderRow,
		TEXT("InventoryCloseButton"),
		NSLOCTEXT("ArenaInventory", "CloseInventory", "Close"));
	Panel->AddChildToVerticalBox(HeaderRow);

	UHorizontalBox* FilterRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("InventoryFilterRow"));
	ClearFiltersButton = AddTextButton(
		WidgetTree,
		FilterRow,
		TEXT("InventoryAllFilterButton"),
		NSLOCTEXT("ArenaInventory", "FilterAll", "All"));
	ConsumableFilterCheckBox = AddFilterCheckBox(
		WidgetTree,
		FilterRow,
		TEXT("InventoryConsumableFilter"),
		NSLOCTEXT("ArenaInventory", "FilterConsumable", "Consumable"));
	HealthFilterCheckBox = AddFilterCheckBox(
		WidgetTree,
		FilterRow,
		TEXT("InventoryHealthFilter"),
		NSLOCTEXT("ArenaInventory", "FilterHealth", "Health"));
	EnergyFilterCheckBox = AddFilterCheckBox(
		WidgetTree,
		FilterRow,
		TEXT("InventoryEnergyFilter"),
		NSLOCTEXT("ArenaInventory", "FilterEnergy", "Energy"));
	if (UVerticalBoxSlot* FilterSlot = Panel->AddChildToVerticalBox(FilterRow))
	{
		FilterSlot->SetPadding(FMargin(0.0f, 8.0f));
	}

	UHorizontalBox* ContentRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("InventoryContentRow"));
	if (UVerticalBoxSlot* ContentSlot = Panel->AddChildToVerticalBox(ContentRow))
	{
		ContentSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	USizeBox* GridSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("InventoryGridSize"));
	GridSizeBox->SetWidthOverride(420.0f);
	GridSizeBox->SetHeightOverride(350.0f);
	if (UHorizontalBoxSlot* GridContainerSlot = ContentRow->AddChildToHorizontalBox(GridSizeBox))
	{
		GridContainerSlot->SetVerticalAlignment(VAlign_Center);
		GridContainerSlot->SetPadding(FMargin(0.0f, 0.0f, 14.0f, 0.0f));
	}

	ItemGrid = WidgetTree->ConstructWidget<UGridPanel>(UGridPanel::StaticClass(), TEXT("InventoryItemGrid"));
	GridSizeBox->AddChild(ItemGrid);
	TSubclassOf<UArenaInventorySlotWidget> ResolvedSlotWidgetClass = InventorySlotWidgetClass;
	if (!ResolvedSlotWidgetClass)
	{
		ResolvedSlotWidgetClass = UArenaInventorySlotWidget::StaticClass();
	}
	for (int32 Index = 0; Index < 20; ++Index)
	{
		UArenaInventorySlotWidget* SlotWidget = WidgetTree->ConstructWidget<UArenaInventorySlotWidget>(
			ResolvedSlotWidgetClass,
			*FString::Printf(TEXT("InventorySlot_%02d"), Index));
		SlotWidgets.Add(SlotWidget);
		if (UGridSlot* GridSlot = ItemGrid->AddChildToGrid(SlotWidget, Index / 5, Index % 5))
		{
			GridSlot->SetPadding(FMargin(3.0f));
			GridSlot->SetHorizontalAlignment(HAlign_Center);
			GridSlot->SetVerticalAlignment(VAlign_Center);
		}
	}

	UBorder* DetailsBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InventoryDetailsBorder"));
	DetailsBorder->SetPadding(FMargin(14.0f));
	DetailsBorder->SetBrushColor(FLinearColor(0.055f, 0.070f, 0.095f, 1.0f));
	if (UHorizontalBoxSlot* DetailsBorderSlot = ContentRow->AddChildToHorizontalBox(DetailsBorder))
	{
		DetailsBorderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		DetailsBorderSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UVerticalBox* DetailsPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("InventoryDetailsPanel"));
	DetailsBorder->AddChild(DetailsPanel);

	DetailsText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("InventoryDetailsText"));
	DetailsText->SetAutoWrapText(true);
	SetTextSize(DetailsText, 15);
	if (UVerticalBoxSlot* DetailsSlot = DetailsPanel->AddChildToVerticalBox(DetailsText))
	{
		DetailsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		DetailsSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
	}

	UHorizontalBox* ActionRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("InventoryActionRow"));
	UseButton = AddTextButton(
		WidgetTree,
		ActionRow,
		TEXT("InventoryUseButton"),
		NSLOCTEXT("ArenaInventory", "UseItem", "Use"));
	DropButton = AddTextButton(
		WidgetTree,
		ActionRow,
		TEXT("InventoryDropButton"),
		NSLOCTEXT("ArenaInventory", "DropItem", "Drop"));
	DetailsPanel->AddChildToVerticalBox(ActionRow);

	UHorizontalBox* DropConfirmRow =
		WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("InventoryDropConfirmRow"));
	DropConfirmPanel = DropConfirmRow;
	UTextBlock* QuantityLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("InventoryDropQuantityLabel"));
	QuantityLabel->SetText(NSLOCTEXT("ArenaInventory", "DropQuantityLabel", "Qty"));
	SetTextSize(QuantityLabel, 14);
	if (UHorizontalBoxSlot* QuantityLabelSlot = DropConfirmRow->AddChildToHorizontalBox(QuantityLabel))
	{
		QuantityLabelSlot->SetVerticalAlignment(VAlign_Center);
		QuantityLabelSlot->SetPadding(FMargin(3.0f));
	}
	DropQuantitySpinBox = WidgetTree->ConstructWidget<USpinBox>(USpinBox::StaticClass(), TEXT("InventoryDropQuantity"));
	DropQuantitySpinBox->SetMinValue(1.0f);
	DropQuantitySpinBox->SetMaxValue(1.0f);
	DropQuantitySpinBox->SetDelta(1.0f);
	DropQuantitySpinBox->SetMinFractionalDigits(0);
	DropQuantitySpinBox->SetMaxFractionalDigits(0);
	DropQuantitySpinBox->SetMinDesiredWidth(54.0f);
	if (UHorizontalBoxSlot* QuantitySlot = DropConfirmRow->AddChildToHorizontalBox(DropQuantitySpinBox))
	{
		QuantitySlot->SetPadding(FMargin(3.0f));
	}
	ConfirmDropButton = AddTextButton(
		WidgetTree,
		DropConfirmRow,
		TEXT("InventoryConfirmDropButton"),
		NSLOCTEXT("ArenaInventory", "ConfirmDropItem", "Confirm"));
	CancelDropButton = AddTextButton(
		WidgetTree,
		DropConfirmRow,
		TEXT("InventoryCancelDropButton"),
		NSLOCTEXT("ArenaInventory", "CancelDropItem", "Cancel"));
	DropConfirmRow->SetVisibility(ESlateVisibility::Hidden);
	if (UVerticalBoxSlot* DropConfirmSlot = DetailsPanel->AddChildToVerticalBox(DropConfirmRow))
	{
		DropConfirmSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
	}

	UHorizontalBox* PageRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("InventoryPageRow"));
	PreviousPageButton = AddTextButton(
		WidgetTree,
		PageRow,
		TEXT("InventoryPreviousPageButton"),
		NSLOCTEXT("ArenaInventory", "PreviousPage", "<"));
	PageText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("InventoryPageText"));
	PageText->SetJustification(ETextJustify::Center);
	SetTextSize(PageText, 14);
	if (UHorizontalBoxSlot* PageLabelSlot = PageRow->AddChildToHorizontalBox(PageText))
	{
		PageLabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		PageLabelSlot->SetVerticalAlignment(VAlign_Center);
	}
	NextPageButton = AddTextButton(
		WidgetTree,
		PageRow,
		TEXT("InventoryNextPageButton"),
		NSLOCTEXT("ArenaInventory", "NextPage", ">"));
	if (UVerticalBoxSlot* PageSlot = Panel->AddChildToVerticalBox(PageRow))
	{
		PageSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
	}
}

// 绑定原生控制，重复 Construct 时使用 AddUniqueDynamic 防止重复提交 RPC。
void UArenaInventoryWidget::BindControls()
{
	for (UArenaInventorySlotWidget* SlotWidget : SlotWidgets)
	{
		if (!SlotWidget)
		{
			continue;
		}
		SlotWidget->OnSlotSelected.AddUniqueDynamic(this, &UArenaInventoryWidget::HandleSlotSelected);
		SlotWidget->OnSlotUseRequested.AddUniqueDynamic(this, &UArenaInventoryWidget::HandleSlotUseRequested);
	}
	if (UseButton)
	{
		UseButton->OnClicked.AddUniqueDynamic(this, &UArenaInventoryWidget::HandleUseClicked);
	}
	if (DropButton)
	{
		DropButton->OnClicked.AddUniqueDynamic(this, &UArenaInventoryWidget::HandleOpenDropClicked);
	}
	if (ConfirmDropButton)
	{
		ConfirmDropButton->OnClicked.AddUniqueDynamic(
			this,
			&UArenaInventoryWidget::HandleConfirmDropClicked);
	}
	if (CancelDropButton)
	{
		CancelDropButton->OnClicked.AddUniqueDynamic(
			this,
			&UArenaInventoryWidget::HandleCancelDropClicked);
	}
	if (PreviousPageButton)
	{
		PreviousPageButton->OnClicked.AddUniqueDynamic(this, &UArenaInventoryWidget::HandlePreviousPageClicked);
	}
	if (NextPageButton)
	{
		NextPageButton->OnClicked.AddUniqueDynamic(this, &UArenaInventoryWidget::HandleNextPageClicked);
	}
	if (CloseButton)
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &UArenaInventoryWidget::HandleCloseClicked);
	}
	if (ClearFiltersButton)
	{
		ClearFiltersButton->OnClicked.AddUniqueDynamic(this, &UArenaInventoryWidget::HandleClearFiltersClicked);
	}
	if (ConsumableFilterCheckBox)
	{
		ConsumableFilterCheckBox->OnCheckStateChanged.AddUniqueDynamic(
			this,
			&UArenaInventoryWidget::HandleConsumableFilterChanged);
	}
	if (HealthFilterCheckBox)
	{
		HealthFilterCheckBox->OnCheckStateChanged.AddUniqueDynamic(
			this,
			&UArenaInventoryWidget::HandleHealthFilterChanged);
	}
	if (EnergyFilterCheckBox)
	{
		EnergyFilterCheckBox->OnCheckStateChanged.AddUniqueDynamic(
			this,
			&UArenaInventoryWidget::HandleEnergyFilterChanged);
	}
}

// 把页面 ViewData 映射到固定二十槽，并按只读权限刷新详情、操作与翻页状态。
void UArenaInventoryWidget::RefreshVisuals()
{
	for (int32 Index = 0; Index < SlotWidgets.Num(); ++Index)
	{
		UArenaInventorySlotWidget* SlotWidget = SlotWidgets[Index];
		const FArenaInventoryItemViewData* Item = CurrentViewData.Items.IsValidIndex(Index)
			? &CurrentViewData.Items[Index]
			: nullptr;
		if (SlotWidget)
		{
			SlotWidget->SetSlotData(
				Item ? Item->StackId : FGuid(),
				Item ? Item->ItemData.Get() : nullptr,
				Item ? Item->Quantity : 0,
				Item && Item->StackId == CurrentViewData.SelectedStackId);
		}
	}

	if (PageText)
	{
		PageText->SetText(FText::Format(
			NSLOCTEXT("ArenaInventory", "PageFormat", "Page {0} / {1}"),
			FText::AsNumber(CurrentViewData.CurrentPage),
			FText::AsNumber(CurrentViewData.TotalPages)));
	}
	if (PreviousPageButton)
	{
		PreviousPageButton->SetIsEnabled(CurrentViewData.CurrentPage > 1);
	}
	if (NextPageButton)
	{
		NextPageButton->SetIsEnabled(CurrentViewData.CurrentPage < CurrentViewData.TotalPages);
	}

	const FArenaInventoryItemViewData* SelectedItem = FindSelectedItem();
	const bool bHasSelection = SelectedItem && SelectedItem->ItemData && SelectedItem->Quantity > 0;
	if (DetailsText)
	{
		DetailsText->SetText(bHasSelection
			? FText::Format(
				NSLOCTEXT("ArenaInventory", "SelectedItemDetails", "{0} x{1}\n{2}"),
				SelectedItem->ItemData->DisplayName,
				FText::AsNumber(SelectedItem->Quantity),
				SelectedItem->ItemData->Description)
			: NSLOCTEXT("ArenaInventory", "NoItemSelected", "Select an item."));
	}
	if (UseButton)
	{
		UseButton->SetIsEnabled(bHasSelection && CurrentViewData.bCanUseItems);
	}
	if (DropButton)
	{
		DropButton->SetIsEnabled(bHasSelection && CurrentViewData.bCanDropItems);
	}
	if ((!bHasSelection || !CurrentViewData.bCanDropItems) && DropConfirmPanel)
	{
		DropConfirmPanel->SetVisibility(ESlateVisibility::Hidden);
	}
	if (DropQuantitySpinBox)
	{
		const float MaxQuantity = bHasSelection ? static_cast<float>(SelectedItem->Quantity) : 1.0f;
		DropQuantitySpinBox->SetMinValue(1.0f);
		DropQuantitySpinBox->SetMaxValue(FMath::Max(MaxQuantity, 1.0f));
		DropQuantitySpinBox->SetValue(FMath::Clamp(DropQuantitySpinBox->GetValue(), 1.0f, MaxQuantity));
		DropQuantitySpinBox->SetIsEnabled(bHasSelection && CurrentViewData.bCanDropItems);
	}

	bUpdatingFilterControls = true;
	if (ConsumableFilterCheckBox)
	{
		ConsumableFilterCheckBox->SetIsChecked(
			CurrentViewData.ActiveFilters.HasTagExact(ArenaGameplayTags::Item_Type_Consumable));
	}
	if (HealthFilterCheckBox)
	{
		HealthFilterCheckBox->SetIsChecked(
			CurrentViewData.ActiveFilters.HasTagExact(ArenaGameplayTags::Item_Effect_Restore_Health));
	}
	if (EnergyFilterCheckBox)
	{
		EnergyFilterCheckBox->SetIsChecked(
			CurrentViewData.ActiveFilters.HasTagExact(ArenaGameplayTags::Item_Effect_Restore_Energy));
	}
	bUpdatingFilterControls = false;
}

// 在当前页面中查找选中 StackId，跨页或被移除时返回空。
const FArenaInventoryItemViewData* UArenaInventoryWidget::FindSelectedItem() const
{
	return CurrentViewData.Items.FindByPredicate([this](const FArenaInventoryItemViewData& Item)
	{
		return Item.StackId == CurrentViewData.SelectedStackId;
	});
}

// 槽位点击只更新 Controller 的本地选择状态。
void UArenaInventoryWidget::HandleSlotSelected(FGuid StackId)
{
	if (DropConfirmPanel)
	{
		DropConfirmPanel->SetVisibility(ESlateVisibility::Hidden);
	}
	OnStackSelected.Broadcast(StackId);
}

// 双击槽位仅在阶段、状态和共享冷却均允许时发送使用请求，服务器仍会重新验证。
void UArenaInventoryWidget::HandleSlotUseRequested(FGuid StackId)
{
	if (CurrentViewData.bCanUseItems)
	{
		OnUseRequested.Broadcast(StackId);
	}
}

// Use 按钮仅在非只读且共享冷却结束时提交当前选中堆栈。
void UArenaInventoryWidget::HandleUseClicked()
{
	if (CurrentViewData.bCanUseItems && CurrentViewData.SelectedStackId.IsValid())
	{
		OnUseRequested.Broadcast(CurrentViewData.SelectedStackId);
	}
}

// Drop 首次点击仅在非只读阶段展开数量确认面板，避免单击误丢物品。
void UArenaInventoryWidget::HandleOpenDropClicked()
{
	if (CurrentViewData.bCanDropItems
		&& CurrentViewData.SelectedStackId.IsValid()
		&& DropConfirmPanel)
	{
		DropConfirmPanel->SetVisibility(ESlateVisibility::Visible);
	}
}

// Confirm 仅提交合法正整数，不把非法输入静默改成一；服务器仍按最新权威堆栈重新验证。
void UArenaInventoryWidget::HandleConfirmDropClicked()
{
	if (CurrentViewData.bCanDropItems
		&& CurrentViewData.SelectedStackId.IsValid()
		&& DropQuantitySpinBox)
	{
		const int32 RequestedQuantity = FMath::RoundToInt(DropQuantitySpinBox->GetValue());
		const FArenaInventoryItemViewData* SelectedItem = FindSelectedItem();
		if (SelectedItem
			&& ArenaInventory::IsValidDropQuantity(RequestedQuantity, SelectedItem->Quantity))
		{
			OnDropRequested.Broadcast(CurrentViewData.SelectedStackId, RequestedQuantity);
		}
	}
	if (DropConfirmPanel)
	{
		DropConfirmPanel->SetVisibility(ESlateVisibility::Hidden);
	}
}

// Cancel 只折叠本地数量面板，不向服务器发送丢弃请求。
void UArenaInventoryWidget::HandleCancelDropClicked()
{
	if (DropConfirmPanel)
	{
		DropConfirmPanel->SetVisibility(ESlateVisibility::Hidden);
	}
}

// 请求 Controller 切换上一页。
void UArenaInventoryWidget::HandlePreviousPageClicked()
{
	OnPreviousPageRequested.Broadcast();
}

// 请求 Controller 切换下一页。
void UArenaInventoryWidget::HandleNextPageClicked()
{
	OnNextPageRequested.Broadcast();
}

// 关闭按钮只请求 Controller 恢复本地输入。
void UArenaInventoryWidget::HandleCloseClicked()
{
	OnCloseRequested.Broadcast();
}

// All 清空所有本地筛选 Tag。
void UArenaInventoryWidget::HandleClearFiltersClicked()
{
	OnClearFiltersRequested.Broadcast();
}

// 多选 Consumable 筛选，使用 OR 语义由 Controller 生成页面。
void UArenaInventoryWidget::HandleConsumableFilterChanged(bool bIsChecked)
{
	if (!bUpdatingFilterControls)
	{
		OnFilterRequested.Broadcast(ArenaGameplayTags::Item_Type_Consumable, bIsChecked);
	}
}

// 多选 Health 恢复筛选。
void UArenaInventoryWidget::HandleHealthFilterChanged(bool bIsChecked)
{
	if (!bUpdatingFilterControls)
	{
		OnFilterRequested.Broadcast(ArenaGameplayTags::Item_Effect_Restore_Health, bIsChecked);
	}
}

// 多选 Energy 恢复筛选。
void UArenaInventoryWidget::HandleEnergyFilterChanged(bool bIsChecked)
{
	if (!bUpdatingFilterControls)
	{
		OnFilterRequested.Broadcast(ArenaGameplayTags::Item_Effect_Restore_Energy, bIsChecked);
	}
}
