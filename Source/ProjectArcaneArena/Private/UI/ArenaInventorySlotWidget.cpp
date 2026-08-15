#include "UI/ArenaInventorySlotWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Item/ArenaItemDataAsset.h"

// Slate 初始化时构建固定槽位树，避免背包依赖尚未创建的 WBP。
void UArenaInventorySlotWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildFallbackLayout();
}

// 左键选择槽位，短时间内第二次点击同一堆栈则请求使用。
FReply UArenaInventorySlotWidget::NativeOnMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !StackId.IsValid() || !ItemData)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	const double CurrentTimeSeconds = FPlatformTime::Seconds();
	const bool bDoubleClick = LastClickedStackId == StackId
		&& LastClickTimeSeconds >= 0.0
		&& CurrentTimeSeconds - LastClickTimeSeconds <= 0.32;
	LastClickTimeSeconds = bDoubleClick ? -1.0 : CurrentTimeSeconds;
	LastClickedStackId = bDoubleClick ? FGuid() : StackId;

	OnSlotSelected.Broadcast(StackId);
	if (bDoubleClick)
	{
		OnSlotUseRequested.Broadcast(StackId);
	}
	return FReply::Handled();
}

// 更新图标、数量和选中底色；名称只进入详情与 Tooltip，避免在紧凑槽位内遮挡图标。
void UArenaInventorySlotWidget::SetSlotData(
	FGuid InStackId,
	UArenaItemDataAsset* InItemData,
	int32 InQuantity,
	bool bSelected)
{
	StackId = InStackId;
	ItemData = InItemData;
	Quantity = FMath::Max(InQuantity, 0);
	const bool bHasItem = StackId.IsValid() && ItemData && Quantity > 0;

	SetIsEnabled(bHasItem);
	SetToolTipText(bHasItem ? ItemData->Description : FText::GetEmpty());
	if (SlotBorder)
	{
		SlotBorder->SetBrushColor(
			bSelected
				? FLinearColor(0.12f, 0.34f, 0.58f, 0.98f)
				: FLinearColor(0.10f, 0.12f, 0.16f, 0.92f));
	}
	if (ItemIcon)
	{
		ItemIcon->SetVisibility(bHasItem && !ItemData->Icon.IsNull()
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Hidden);
		if (bHasItem && !ItemData->Icon.IsNull())
		{
			ItemIcon->SetBrushFromSoftTexture(ItemData->Icon, false);
		}
	}
	if (ItemNameText)
	{
		ItemNameText->SetText(FText::GetEmpty());
	}
	if (QuantityText)
	{
		QuantityText->SetText(bHasItem && Quantity > 1 ? FText::AsNumber(Quantity) : FText::GetEmpty());
	}
	K2_OnSlotDataUpdated(StackId, ItemData, Quantity, bSelected);
}

// 构建稳定的 76×76 图标槽位，只叠加右上角数量，动态内容不会改变网格尺寸。
void UArenaInventorySlotWidget::BuildFallbackLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	USizeBox* RootSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("InventorySlotSize"));
	RootSize->SetWidthOverride(76.0f);
	RootSize->SetHeightOverride(76.0f);
	WidgetTree->RootWidget = RootSize;

	SlotBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InventorySlotBorder"));
	SlotBorder->SetPadding(FMargin(5.0f));
	SlotBorder->SetBrushColor(FLinearColor(0.10f, 0.12f, 0.16f, 0.92f));
	RootSize->AddChild(SlotBorder);

	UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("InventorySlotOverlay"));
	SlotBorder->AddChild(Overlay);

	ItemIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("InventoryItemIcon"));
	ItemIcon->SetVisibility(ESlateVisibility::Hidden);
	if (UOverlaySlot* IconSlot = Overlay->AddChildToOverlay(ItemIcon))
	{
		IconSlot->SetHorizontalAlignment(HAlign_Fill);
		IconSlot->SetVerticalAlignment(VAlign_Fill);
	}

	ItemNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("InventoryItemName"));
	ItemNameText->SetVisibility(ESlateVisibility::Collapsed);

	QuantityText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("InventoryItemQuantity"));
	QuantityText->SetJustification(ETextJustify::Right);
	QuantityText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.9f, 0.3f, 1.0f)));
	QuantityText->SetShadowColorAndOpacity(FLinearColor::Black);
	QuantityText->SetShadowOffset(FVector2D(1.0f, 1.0f));
	FSlateFontInfo QuantityFont = QuantityText->GetFont();
	QuantityFont.Size = 14;
	QuantityText->SetFont(QuantityFont);
	if (UOverlaySlot* QuantitySlot = Overlay->AddChildToOverlay(QuantityText))
	{
		QuantitySlot->SetHorizontalAlignment(HAlign_Right);
		QuantitySlot->SetVerticalAlignment(VAlign_Top);
		QuantitySlot->SetPadding(FMargin(0.0f, 2.0f, 3.0f, 0.0f));
	}
}
