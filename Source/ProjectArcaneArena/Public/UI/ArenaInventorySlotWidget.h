#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ArenaInventorySlotWidget.generated.h"

class UBorder;
class UImage;
class UTextBlock;
class UArenaItemDataAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FArenaInventorySlotActionSignature, FGuid, StackId);

UCLASS(Blueprintable)
class PROJECTARCANEARENA_API UArenaInventorySlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 使用 Controller 提供的只读堆栈 ViewData 刷新单个槽位。
	void SetSlotData(FGuid InStackId, UArenaItemDataAsset* InItemData, int32 InQuantity, bool bSelected);

	UPROPERTY(BlueprintAssignable, Category = "Arena|Inventory")
	FArenaInventorySlotActionSignature OnSlotSelected;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Inventory")
	FArenaInventorySlotActionSignature OnSlotUseRequested;

protected:
	// Slate 初始化时构建固定槽位 fallback。
	virtual void NativeOnInitialized() override;
	// 左键选择槽位，并识别同一 StackId 的短间隔双击。
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// 蓝图 Slot 可在只读数据变化时更新自定义布局，不直接访问背包 Model。
	UFUNCTION(BlueprintImplementableEvent, Category = "Arena|Inventory")
	void K2_OnSlotDataUpdated(FGuid InStackId, UArenaItemDataAsset* InItemData, int32 InQuantity, bool bSelected);

private:
	// 创建固定尺寸的纯图标与数量原生槽位，名称改由详情区和 Tooltip 展示。
	void BuildFallbackLayout();

	UPROPERTY(Transient)
	TObjectPtr<UBorder> SlotBorder;

	UPROPERTY(Transient)
	TObjectPtr<UImage> ItemIcon;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ItemNameText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> QuantityText;

	FGuid StackId;

	UPROPERTY(Transient)
	TObjectPtr<UArenaItemDataAsset> ItemData;

	int32 Quantity = 0;
	FGuid LastClickedStackId;
	double LastClickTimeSeconds = -1.0;
};
