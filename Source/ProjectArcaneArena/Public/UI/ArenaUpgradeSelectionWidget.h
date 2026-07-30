#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ArenaUpgradeSelectionWidget.generated.h"

class UArenaUpgradeDataAsset;
class UButton;
class UImage;
class UTextBlock;
class UWidget;
enum class EArenaUpgradeRarity : uint8;

USTRUCT(BlueprintType)
struct PROJECTARCANEARENA_API FArenaUpgradeChoiceViewData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Upgrade")
	TObjectPtr<UArenaUpgradeDataAsset> Upgrade;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Upgrade")
	int32 CurrentStacks = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Upgrade")
	int32 ResultingStacks = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Upgrade")
	int32 MaxStacks = 1;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FArenaUpgradeChosenSignature, FName, UpgradeID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FArenaUpgradeInventoryRequestSignature);

UCLASS(Blueprintable)
class PROJECTARCANEARENA_API UArenaUpgradeSelectionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 使用 Controller 整理的候选与层数快照刷新三选一界面，Widget 不生成或验证升级。
	UFUNCTION(BlueprintCallable, Category = "Arena|Upgrade")
	void ShowUpgradeChoices(const TArray<FArenaUpgradeChoiceViewData>& InChoices);

	// 清空本地候选展示并折叠界面，不修改 PlayerState 或服务器选择状态。
	UFUNCTION(BlueprintCallable, Category = "Arena|Upgrade")
	void HideUpgradeChoices();

	// 优先返回第一个有效候选按钮供键盘/手柄确认，缺失时回退可聚焦根 Widget。
	UWidget* GetInitialFocusTarget() const;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Upgrade")
	FArenaUpgradeChosenSignature OnUpgradeChosen;

	// Upgrade 阶段通过该意图把 Tab 交还 Controller，View 不自行决定背包权限。
	UPROPERTY(BlueprintAssignable, Category = "Arena|Upgrade")
	FArenaUpgradeInventoryRequestSignature OnInventoryRequested;

	// Upgrade 阶段把 Tab 松开交还 Controller，以完成长按临时查看。
	UPROPERTY(BlueprintAssignable, Category = "Arena|Upgrade")
	FArenaUpgradeInventoryRequestSignature OnInventoryTabReleased;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	// 在候选按钮处理按键前拦截 Tab 按下，使只读背包可以从 Upgrade 界面打开。
	virtual FReply NativeOnPreviewKeyDown(
		const FGeometry& InGeometry,
		const FKeyEvent& InKeyEvent) override;
	// 焦点路由未执行 Preview 时再次捕获 Tab，避免自定义蓝图子控件吞掉背包切换。
	virtual FReply NativeOnKeyDown(
		const FGeometry& InGeometry,
		const FKeyEvent& InKeyEvent) override;
	// 在冒泡路径捕获 Tab 松开，供 Controller 判定轻点或长按。
	virtual FReply NativeOnKeyUp(
		const FGeometry& InGeometry,
		const FKeyEvent& InKeyEvent) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Upgrade")
	TObjectPtr<UButton> UpgradeChoiceButton0;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Upgrade")
	TObjectPtr<UButton> UpgradeChoiceButton1;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Upgrade")
	TObjectPtr<UButton> UpgradeChoiceButton2;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Upgrade")
	TObjectPtr<UTextBlock> UpgradeChoiceText0;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Upgrade")
	TObjectPtr<UTextBlock> UpgradeChoiceText1;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Upgrade")
	TObjectPtr<UTextBlock> UpgradeChoiceText2;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Upgrade")
	TObjectPtr<UTextBlock> UpgradeChoiceRarityText0;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Upgrade")
	TObjectPtr<UTextBlock> UpgradeChoiceRarityText1;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Upgrade")
	TObjectPtr<UTextBlock> UpgradeChoiceRarityText2;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Upgrade")
	TObjectPtr<UTextBlock> UpgradeChoiceStackText0;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Upgrade")
	TObjectPtr<UTextBlock> UpgradeChoiceStackText1;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Upgrade")
	TObjectPtr<UTextBlock> UpgradeChoiceStackText2;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Upgrade")
	TObjectPtr<UImage> UpgradeChoiceIcon0;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Upgrade")
	TObjectPtr<UImage> UpgradeChoiceIcon1;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Upgrade")
	TObjectPtr<UImage> UpgradeChoiceIcon2;

	// 原生 fallback 使用 SizeBox 固定图标尺寸；蓝图子类可覆盖默认值。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Upgrade|Layout", meta = (ClampMin = "1.0"))
	FVector2D UpgradeIconSize = FVector2D(220.0f, 220.0f);

	// 原生 fallback 根面板尺寸，为放大的图标和说明文本保留稳定空间。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Upgrade|Layout", meta = (ClampMin = "1.0"))
	FVector2D UpgradePanelSize = FVector2D(1200.0f, 460.0f);

	UFUNCTION(BlueprintImplementableEvent, Category = "Arena|Upgrade", meta = (DisplayName = "Upgrade Choices Changed"))
	void K2_OnUpgradeChoicesChanged();

private:
	// 蓝图未提供布局时创建包含图标、稀有度、说明和等级的原生三选一界面。
	void BuildFallbackLayout();
	// 把三个固定按钮绑定到对应候选索引，重复 Construct 时不重复添加委托。
	void BindChoiceButtons();
	// 根据候选快照刷新卡片内容，并在缺少专用文本控件时回退到主文本显示。
	void RefreshChoiceVisuals();
	// 将稀有度枚举转换为可本地化的玩家可见名称。
	FText GetRarityDisplayText(EArenaUpgradeRarity Rarity) const;
	// 返回四种稀有度的默认显示颜色，供原生和蓝图可选文本统一使用。
	FLinearColor GetRarityDisplayColor(EArenaUpgradeRarity Rarity) const;
	// 将选择后的升级层数格式化为 Lv. N/Max。
	FText GetStackDisplayText(const FArenaUpgradeChoiceViewData& Choice) const;
	// 将有效候选索引转换为 UpgradeID 并广播给 Controller。
	void BroadcastChoice(int32 ChoiceIndex);

	UFUNCTION()
	void HandleChoice0Clicked();

	UFUNCTION()
	void HandleChoice1Clicked();

	UFUNCTION()
	void HandleChoice2Clicked();

	UPROPERTY(Transient)
	TArray<FArenaUpgradeChoiceViewData> CurrentChoices;
};
