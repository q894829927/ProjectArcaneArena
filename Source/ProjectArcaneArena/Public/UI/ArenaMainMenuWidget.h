#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/ArenaLobbyTypes.h"
#include "ArenaMainMenuWidget.generated.h"

class UButton;
class UComboBoxString;
class UEditableTextBox;
class UTextBlock;
class UWidget;
class UWidgetSwitcher;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FArenaMainMenuRequestSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FArenaMainMenuHostRequestSignature, int32, MaxPlayers);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FArenaMainMenuJoinRequestSignature, const FString&, Address);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FArenaMainMenuReadyRequestSignature, bool, bReady);

UCLASS(Blueprintable)
class PROJECTARCANEARENA_API UArenaMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 控制当前页面所有操作是否可用，连接或旅行开始后用于阻止重复请求。
	UFUNCTION(BlueprintCallable, Category = "Arena|Main Menu")
	void SetMenuInteractionEnabled(bool bEnabled);

	// 显示单人、多人和退出入口，并清理上一次 Lobby 的临时状态文本。
	UFUNCTION(BlueprintCallable, Category = "Arena|Main Menu")
	void ShowFrontPage();

	// 显示 Host 容量与 Direct IP 地址输入页面。
	UFUNCTION(BlueprintCallable, Category = "Arena|Main Menu")
	void ShowMultiplayerSetupPage();

	// 显示连接或 Seamless Travel 状态，并禁用重复网络操作。
	UFUNCTION(BlueprintCallable, Category = "Arena|Main Menu")
	void ShowConnectingPage(const FText& StatusText);

	// 使用 Controller 构建的只读 ViewData 刷新成员列表与本地操作权限。
	UFUNCTION(BlueprintCallable, Category = "Arena|Main Menu")
	void ShowLobbyPage(const FArenaLobbyViewData& ViewData);

	// 在当前菜单页显示一次简短错误，不由 View 自行解释网络失败类型。
	UFUNCTION(BlueprintCallable, Category = "Arena|Main Menu")
	void SetErrorMessage(const FText& ErrorMessage);

	// 返回当前页面适合键盘初始焦点的控件，缺少按钮时回退到根 Widget。
	UWidget* GetInitialFocusTarget() const;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Main Menu")
	FArenaMainMenuRequestSignature OnSinglePlayerRequested;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Main Menu")
	FArenaMainMenuRequestSignature OnMultiplayerRequested;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Main Menu")
	FArenaMainMenuHostRequestSignature OnHostLobbyRequested;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Main Menu")
	FArenaMainMenuJoinRequestSignature OnJoinLobbyRequested;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Main Menu")
	FArenaMainMenuRequestSignature OnBackRequested;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Main Menu")
	FArenaMainMenuReadyRequestSignature OnLobbyReadyRequested;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Main Menu")
	FArenaMainMenuRequestSignature OnLobbyStartRequested;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Main Menu")
	FArenaMainMenuRequestSignature OnLobbyLeaveRequested;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Main Menu")
	FArenaMainMenuRequestSignature OnQuitGameRequested;

protected:
	// 在 Blueprint 未提供完整多人控件契约时创建可直接使用的响应式原生菜单。
	virtual void NativeOnInitialized() override;

	// Widget 进入视口时幂等绑定所有页面按钮，只广播操作意图。
	virtual void NativeConstruct() override;

	// Widget 离开视口时对称解绑按钮，避免旅行失败重建后重复广播。
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Main Menu")
	TObjectPtr<UWidgetSwitcher> MenuPageSwitcher;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Main Menu")
	TObjectPtr<UButton> SinglePlayerButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Main Menu")
	TObjectPtr<UButton> MultiplayerButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Main Menu")
	TObjectPtr<UButton> QuitGameButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Main Menu")
	TObjectPtr<UComboBoxString> HostMaxPlayersComboBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Main Menu")
	TObjectPtr<UButton> HostLobbyButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Main Menu")
	TObjectPtr<UEditableTextBox> JoinAddressTextBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Main Menu")
	TObjectPtr<UButton> JoinLobbyButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Main Menu")
	TObjectPtr<UButton> MultiplayerBackButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Main Menu")
	TObjectPtr<UTextBlock> LobbyPlayersText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Main Menu")
	TObjectPtr<UTextBlock> LobbyStatusText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Main Menu")
	TObjectPtr<UButton> LobbyReadyButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Main Menu")
	TObjectPtr<UTextBlock> LobbyReadyButtonText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Main Menu")
	TObjectPtr<UButton> LobbyStartButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Main Menu")
	TObjectPtr<UButton> LobbyLeaveButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Main Menu")
	TObjectPtr<UTextBlock> ConnectingStatusText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|Main Menu")
	TObjectPtr<UTextBlock> MenuErrorText;

private:
	// 构建四页 fallback，并生成名称与 Blueprint 可选控件契约一致的控件。
	void BuildFallbackLayout();

	// 把首页单人按钮转换为本地无 Listen 旅行意图。
	UFUNCTION()
	void HandleSinglePlayerClicked();

	// 请求 Controller 打开多人设置页，View 不创建或加入网络会话。
	UFUNCTION()
	void HandleMultiplayerClicked();

	// 读取二至四人下拉选项并广播 Host 请求。
	UFUNCTION()
	void HandleHostLobbyClicked();

	// 读取地址输入框并广播 Join 请求，服务器地址规则由 Subsystem 验证。
	UFUNCTION()
	void HandleJoinLobbyClicked();

	// 请求 Controller 返回首页并清理本地连接错误状态。
	UFUNCTION()
	void HandleBackClicked();

	// 按当前复制 Ready 状态广播切换意图，最终结果由服务器验证。
	UFUNCTION()
	void HandleLobbyReadyClicked();

	// 广播 Host 开始意图，GameMode 会重新验证所有成员状态。
	UFUNCTION()
	void HandleLobbyStartClicked();

	// 广播离开房间意图，由 Subsystem 区分 Host 和 Client 断开路径。
	UFUNCTION()
	void HandleLobbyLeaveClicked();

	// 广播退出意图，由 PlayerController 负责平台退出行为。
	UFUNCTION()
	void HandleQuitGameClicked();

	FArenaLobbyViewData CurrentLobbyViewData;
	int32 CurrentPageIndex = 0;
	bool bMenuInteractionEnabled = true;
};
