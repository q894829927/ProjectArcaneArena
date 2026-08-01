#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ArenaMainMenuWidget.generated.h"

class UButton;
class UWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FArenaMainMenuRequestSignature);

UCLASS(Blueprintable)
class PROJECTARCANEARENA_API UArenaMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 控制两个菜单操作是否可用，关卡切换或退出开始后用于阻止重复请求。
	UFUNCTION(BlueprintCallable, Category = "Arena|Main Menu")
	void SetMenuInteractionEnabled(bool bEnabled);

	// 返回适合键盘初始焦点的控件，缺少按钮时回退到根 Widget。
	UWidget* GetInitialFocusTarget() const;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Main Menu")
	FArenaMainMenuRequestSignature OnStartGameRequested;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Main Menu")
	FArenaMainMenuRequestSignature OnQuitGameRequested;

protected:
	// 在 Blueprint 未提供完整布局时创建可直接使用的响应式原生菜单。
	virtual void NativeOnInitialized() override;

	// Widget 进入视口时绑定按钮意图，不在 View 内执行关卡切换或退出。
	virtual void NativeConstruct() override;

	// Widget 离开视口时对称解绑按钮，避免重建后重复广播。
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Main Menu")
	TObjectPtr<UButton> StartGameButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Main Menu")
	TObjectPtr<UButton> QuitGameButton;

private:
	// 构建深色竞技场风格 fallback，并生成名称与 Blueprint 约定一致的按钮。
	void BuildFallbackLayout();

	// 将按钮点击转换为开始游戏意图，由 PlayerController 负责实际旅行。
	UFUNCTION()
	void HandleStartGameClicked();

	// 将按钮点击转换为退出意图，由 PlayerController 负责平台退出行为。
	UFUNCTION()
	void HandleQuitGameClicked();
};
