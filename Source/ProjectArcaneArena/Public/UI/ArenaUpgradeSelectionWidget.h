#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ArenaUpgradeSelectionWidget.generated.h"

class UArenaUpgradeDataAsset;
class UButton;
class UImage;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FArenaUpgradeChosenSignature, FName, UpgradeID);

UCLASS(Blueprintable)
class PROJECTARCANEARENA_API UArenaUpgradeSelectionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 使用服务器下发的候选刷新三选一界面，Widget 不生成或验证升级。
	UFUNCTION(BlueprintCallable, Category = "Arena|Upgrade")
	void ShowUpgradeChoices(const TArray<UArenaUpgradeDataAsset*>& InChoices);

	UFUNCTION(BlueprintCallable, Category = "Arena|Upgrade")
	void HideUpgradeChoices();

	UPROPERTY(BlueprintAssignable, Category = "Arena|Upgrade")
	FArenaUpgradeChosenSignature OnUpgradeChosen;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

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
	void BuildFallbackLayout();
	void BindChoiceButtons();
	void RefreshChoiceVisuals();
	void BroadcastChoice(int32 ChoiceIndex);

	UFUNCTION()
	void HandleChoice0Clicked();

	UFUNCTION()
	void HandleChoice1Clicked();

	UFUNCTION()
	void HandleChoice2Clicked();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UArenaUpgradeDataAsset>> CurrentChoices;
};
