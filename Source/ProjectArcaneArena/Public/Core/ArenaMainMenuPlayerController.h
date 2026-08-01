#pragma once

#include "CoreMinimal.h"
#include "Core/ArenaDirectConnectSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "ArenaMainMenuPlayerController.generated.h"

class AArenaLobbyPlayerState;
class AArenaMainMenuGameState;
class UArenaDirectConnectSubsystem;
class UArenaMainMenuWidget;

UCLASS()
class PROJECTARCANEARENA_API AArenaMainMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AArenaMainMenuPlayerController();

	// Client 请求服务器切换自己的 Lobby Ready；Host 请求会被 GameMode 拒绝。
	UFUNCTION(Server, Reliable)
	void ServerSetLobbyReady(bool bReady);

	// Host 请求服务器开始比赛，GameMode 会重新验证人数和全部 Client Ready。
	UFUNCTION(Server, Reliable)
	void ServerStartLobbyMatch();

protected:
	// 仅在本地 Controller 创建菜单、显示鼠标并建立 UIOnly 焦点。
	virtual void BeginPlay() override;

	// PlayerState 首次复制后绑定个人 Ready 状态并刷新 Lobby View。
	virtual void OnRep_PlayerState() override;

	// Host 主动关闭 Lobby 时保存原因并让 Client 返回自己的主菜单。
	virtual void ClientReturnToMainMenuWithTextReason_Implementation(const FText& ReturnReason) override;

	// 关卡旅行或退出时解绑 View、GameState、PlayerState 与 Subsystem 委托。
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// 创建并显示配置的菜单 Widget；配置缺失时回退到原生 View 类。
	void CreateMainMenu();

	// 重新绑定当前复制 Lobby 对象并构建只读 ViewData，网络规则仍由服务器决定。
	UFUNCTION()
	void RefreshLobbyView();

	// 页面切换后把 UIOnly 输入和键盘焦点交给当前有效操作按钮。
	void ApplyMenuInputFocus();

	// 接收 View 的单人意图，防重后打开无 Listen 的正式战斗关卡。
	UFUNCTION()
	void HandleSinglePlayerRequested();

	// 接收 View 的多人入口意图，仅切换本地设置页面。
	UFUNCTION()
	void HandleMultiplayerRequested();

	// 接收容量后调用跨关卡 Subsystem 创建 Listen Lobby。
	UFUNCTION()
	void HandleHostLobbyRequested(int32 MaxPlayers);

	// 接收地址后调用跨关卡 Subsystem 校验并加入 Lobby。
	UFUNCTION()
	void HandleJoinLobbyRequested(const FString& Address);

	// 返回首页并消费本地旧错误，不发送网络请求。
	UFUNCTION()
	void HandleBackRequested();

	// 把 Client Ready 意图发送服务器，等待 PlayerState 复制确认。
	UFUNCTION()
	void HandleLobbyReadyRequested(bool bReady);

	// 把 Host Start 意图发送服务器，等待 GameState 旅行锁复制确认。
	UFUNCTION()
	void HandleLobbyStartRequested();

	// 通过 DirectConnectSubsystem 区分 Host 关闭与 Client 主动断开。
	UFUNCTION()
	void HandleLobbyLeaveRequested();

	// 接收 View 的退出意图，通过引擎统一接口退出 Standalone 或结束 PIE。
	UFUNCTION()
	void HandleQuitGameRequested();

	// 连接状态变化时即时显示 Connecting；失败文本由返回后的菜单统一消费。
	UFUNCTION()
	void HandleConnectionStateChanged(
		EArenaDirectConnectState OldState,
		EArenaDirectConnectState NewState);

	// 获取当前 GameInstance 上唯一 Direct Connect 状态机。
	UArenaDirectConnectSubsystem* GetDirectConnectSubsystem() const;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Main Menu")
	TSubclassOf<UArenaMainMenuWidget> MainMenuWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Main Menu")
	FName GameplayMapName = TEXT("/Game/TopDown/Lvl_TopDown");

	UPROPERTY(Transient)
	TObjectPtr<UArenaMainMenuWidget> MainMenuWidget;

	TWeakObjectPtr<AArenaMainMenuGameState> BoundLobbyGameState;
	TArray<TWeakObjectPtr<AArenaLobbyPlayerState>> BoundLobbyPlayerStates;
	bool bLocalTerminalRequestStarted = false;
};
