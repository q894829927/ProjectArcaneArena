#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ArenaPauseMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FArenaPauseMenuRequestSignature);

UCLASS(Blueprintable)
class PROJECTARCANEARENA_API UArenaPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 显示 ESC 菜单并清理上一次设置占位提示，玩法状态仍由 Controller 管理。
	UFUNCTION(BlueprintCallable, Category = "Arena|Pause Menu")
	void ShowPauseMenu();

	// 隐藏 ESC 菜单并清理临时提示，不直接恢复角色输入。
	UFUNCTION(BlueprintCallable, Category = "Arena|Pause Menu")
	void HidePauseMenu();

	// 返回适合作为 UIOnly 初始焦点的继续按钮，缺失时回退到根 Widget。
	UWidget* GetInitialFocusTarget() const;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Pause Menu")
	FArenaPauseMenuRequestSignature OnResumeRequested;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Pause Menu")
	FArenaPauseMenuRequestSignature OnReturnToMainMenuRequested;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Pause Menu")
	FArenaPauseMenuRequestSignature OnQuitGameRequested;

protected:
	// Blueprint 未提供完整控件契约时创建可直接使用的原生 ESC 菜单。
	virtual void NativeOnInitialized() override;

	// Widget 进入视口时幂等绑定按钮，只向 Controller 广播操作意图。
	virtual void NativeConstruct() override;

	// Widget 离开视口时对称解绑按钮，避免旅行或重建后重复响应。
	virtual void NativeDestruct() override;

	// UIOnly 模式下由 Widget 消费 Escape，保证无需鼠标也能关闭菜单。
	virtual FReply NativeOnPreviewKeyDown(
		const FGeometry& InGeometry,
		const FKeyEvent& InKeyEvent) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Pause Menu")
	TObjectPtr<UButton> ResumeButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Pause Menu")
	TObjectPtr<UButton> SettingsButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Pause Menu")
	TObjectPtr<UButton> ReturnToMainMenuButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Pause Menu")
	TObjectPtr<UButton> QuitGameButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Pause Menu")
	TObjectPtr<UTextBlock> SettingsPlaceholderText;

private:
	// 构建半透明全屏遮罩和紧凑纵向按钮组，适配没有 WBP 的打包版本。
	void BuildFallbackLayout();

	// 继续按钮只广播关闭请求，输入恢复由 Controller 的统一状态机处理。
	UFUNCTION()
	void HandleResumeClicked();

	// 设置按钮当前仅显示占位提示，不创建或修改任何配置数据。
	UFUNCTION()
	void HandleSettingsClicked();

	// 返回主菜单按钮把网络离开意图交给 Controller 和 DirectConnectSubsystem。
	UFUNCTION()
	void HandleReturnToMainMenuClicked();

	// 退出按钮只广播平台退出意图，由 Controller 执行最终 QuitGame。
	UFUNCTION()
	void HandleQuitGameClicked();
};
