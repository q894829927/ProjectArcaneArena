#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "ArenaInventoryWidget.generated.h"

class UButton;
class UCheckBox;
class UGridPanel;
class USpinBox;
class UTextBlock;
class UWidget;
class UArenaInventorySlotWidget;
class UArenaItemDataAsset;

USTRUCT(BlueprintType)
struct PROJECTARCANEARENA_API FArenaInventoryItemViewData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Inventory")
	FGuid StackId;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Inventory")
	TObjectPtr<UArenaItemDataAsset> ItemData;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Inventory")
	int32 Quantity = 0;
};

USTRUCT(BlueprintType)
struct PROJECTARCANEARENA_API FArenaInventoryPageViewData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Inventory")
	TArray<FArenaInventoryItemViewData> Items;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Inventory")
	int32 CurrentPage = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Inventory")
	int32 TotalPages = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Inventory")
	FGuid SelectedStackId;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Inventory")
	FGameplayTagContainer ActiveFilters;

	// Controller 根据复制阶段和角色状态写入，View 只据此启用 Use/Drop 意图。
	UPROPERTY(BlueprintReadOnly, Category = "Arena|Inventory")
	bool bCanPerformActions = false;

	// 在基础操作权限上额外考虑共享消耗品冷却，只控制 Use 意图。
	UPROPERTY(BlueprintReadOnly, Category = "Arena|Inventory")
	bool bCanUseItems = false;

	// 丢弃不受共享使用冷却影响，但仍遵循阶段、Dead 和 Stunned 权限。
	UPROPERTY(BlueprintReadOnly, Category = "Arena|Inventory")
	bool bCanDropItems = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FArenaInventoryStackRequestSignature, FGuid, StackId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FArenaInventoryDropRequestSignature, FGuid, StackId, int32, Quantity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FArenaInventoryFilterRequestSignature, FGameplayTag, FilterTag, bool, bEnabled);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FArenaInventorySimpleRequestSignature);

UCLASS(Blueprintable)
class PROJECTARCANEARENA_API UArenaInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 初始化背包 View 的默认槽位类型，并保留 UUserWidget 的 ObjectInitializer 初始化链。
	UArenaInventoryWidget(const FObjectInitializer& ObjectInitializer);

	// 展示 Controller 计算好的二十槽页面，不读取或修改 PlayerState Model。
	void ShowInventoryPage(const FArenaInventoryPageViewData& InViewData);

	// 折叠界面并清空本地选择表现，不影响服务器背包内容。
	void HideInventory();

	// 返回可安全聚焦的关闭按钮，供 Controller 配置 GameAndUI。
	UWidget* GetInitialFocusTarget() const;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Inventory")
	FArenaInventoryStackRequestSignature OnStackSelected;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Inventory")
	FArenaInventoryStackRequestSignature OnUseRequested;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Inventory")
	FArenaInventoryDropRequestSignature OnDropRequested;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Inventory")
	FArenaInventoryFilterRequestSignature OnFilterRequested;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Inventory")
	FArenaInventorySimpleRequestSignature OnClearFiltersRequested;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Inventory")
	FArenaInventorySimpleRequestSignature OnPreviousPageRequested;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Inventory")
	FArenaInventorySimpleRequestSignature OnNextPageRequested;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Inventory")
	FArenaInventorySimpleRequestSignature OnCloseRequested;

	// GameAndUI 下把 Tab 按下交还 Controller，按键重复由 Controller 状态机去重。
	UPROPERTY(BlueprintAssignable, Category = "Arena|Inventory")
	FArenaInventorySimpleRequestSignature OnTabPressed;

	// GameAndUI 下把 Tab 松开交还 Controller，以完成长按临时查看。
	UPROPERTY(BlueprintAssignable, Category = "Arena|Inventory")
	FArenaInventorySimpleRequestSignature OnTabReleased;

protected:
	// 初始化时创建原生二十槽 fallback。
	virtual void NativeOnInitialized() override;
	// 加入视口时绑定固定控制并默认折叠。
	virtual void NativeConstruct() override;
	// 在子按钮处理焦点导航前拦截 Tab/Escape，并把 Tab 按住状态交给 Controller。
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	// Tab 或 Escape 在 UI 获得焦点时转发状态或请求关闭背包。
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	// KeyUp 冒泡路径转发 Tab 松开，兼容子按钮或自定义槽位持有焦点。
	virtual FReply NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// 蓝图 View 可使用 Controller 已构建的只读页面数据更新自定义布局。
	UFUNCTION(BlueprintImplementableEvent, Category = "Arena|Inventory")
	void K2_OnInventoryPageUpdated(const FArenaInventoryPageViewData& InViewData);

	// 蓝图 View 在背包关闭时清理局部动画或弹窗，输入恢复仍由 Controller 负责。
	UFUNCTION(BlueprintImplementableEvent, Category = "Arena|Inventory")
	void K2_OnInventoryHidden();

	// 指定二十槽 fallback 使用的 Slot Widget 蓝图类，未配置时回退原生槽位。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Inventory", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UArenaInventorySlotWidget> InventorySlotWidgetClass;

private:
	// 创建五列四行、左右分栏详情和固定底部分页的紧凑原生 fallback。
	void BuildFallbackLayout();

	// 对称绑定所有固定控件和二十个 Slot Widget。
	void BindControls();

	// 根据页面快照刷新二十个固定槽、选中详情、按钮和筛选状态。
	void RefreshVisuals();

	// 返回当前选中堆栈的页面 ViewData。
	const FArenaInventoryItemViewData* FindSelectedItem() const;

	UFUNCTION()
	void HandleSlotSelected(FGuid StackId);

	UFUNCTION()
	void HandleSlotUseRequested(FGuid StackId);

	UFUNCTION()
	void HandleUseClicked();

	// 首次点击 Drop 时只展开数量确认面板。
	UFUNCTION()
	void HandleOpenDropClicked();

	// 确认后提交当前 StackId 和整数数量。
	UFUNCTION()
	void HandleConfirmDropClicked();

	// 取消时仅关闭本地数量面板。
	UFUNCTION()
	void HandleCancelDropClicked();

	UFUNCTION()
	void HandlePreviousPageClicked();

	UFUNCTION()
	void HandleNextPageClicked();

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleClearFiltersClicked();

	UFUNCTION()
	void HandleConsumableFilterChanged(bool bIsChecked);

	UFUNCTION()
	void HandleHealthFilterChanged(bool bIsChecked);

	UFUNCTION()
	void HandleEnergyFilterChanged(bool bIsChecked);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UArenaInventorySlotWidget>> SlotWidgets;

	UPROPERTY(Transient)
	TObjectPtr<UGridPanel> ItemGrid;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PageText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailsText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PreviousPageButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> NextPageButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> UseButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> DropButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ConfirmDropButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CancelDropButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ClearFiltersButton;

	UPROPERTY(Transient)
	TObjectPtr<UCheckBox> ConsumableFilterCheckBox;

	UPROPERTY(Transient)
	TObjectPtr<UCheckBox> HealthFilterCheckBox;

	UPROPERTY(Transient)
	TObjectPtr<UCheckBox> EnergyFilterCheckBox;

	UPROPERTY(Transient)
	TObjectPtr<USpinBox> DropQuantitySpinBox;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> DropConfirmPanel;

	UPROPERTY(Transient)
	FArenaInventoryPageViewData CurrentViewData;

	bool bUpdatingFilterControls = false;
};
