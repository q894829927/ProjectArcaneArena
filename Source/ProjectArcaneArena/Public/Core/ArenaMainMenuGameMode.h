#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ArenaMainMenuGameMode.generated.h"

UCLASS()
class PROJECTARCANEARENA_API AArenaMainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AArenaMainMenuGameMode();

	// 服务器验证 Lobby、成员和旅行锁后更新指定 Client 的 Ready 状态。
	void SetLobbyPlayerReady(APlayerController* RequestingController, bool bReady);

	// 服务器验证 Host 与全部 Client Ready 后锁定 Lobby 并开始 Seamless Travel。
	void TryStartLobbyMatch(APlayerController* RequestingController);

	// 返回当前成员是否满足至少两人且所有非 Host 已 Ready 的开始条件。
	bool CanStartLobbyMatch() const;

protected:
	// 从 URL 读取 ArenaLobby 与 MaxPlayers，保留到 GameState 创建后初始化。
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	// 初始化复制 Lobby 状态；普通单人菜单保持非 Lobby 首页。
	virtual void BeginPlay() override;

	// Lobby 锁定旅行后拒绝竞态中的新登录，容量校验仍先由 GameSession 执行。
	virtual void PreLogin(
		const FString& Options,
		const FString& Address,
		const FUniqueNetIdRepl& UniqueId,
		FString& ErrorMessage) override;

	// 登录成员由服务器分配 Host/Client 身份，Host 自动 Ready。
	virtual void PostLogin(APlayerController* NewPlayer) override;

	// 成员退出后刷新剩余房间列表和开始条件。
	virtual void Logout(AController* Exiting) override;

	// 菜单无需出生点，直接保留 Controller 的默认变换并让登录流程成功完成。
	virtual bool UpdatePlayerStartSpot(AController* Player, const FString& Portal, FString& OutErrorMessage) override;

	// 菜单使用 UMG View，不创建 AHUD Actor。
	virtual void InitializeHUDForPlayer_Implementation(APlayerController* NewPlayer) override;

	// 菜单关卡保留本地 PlayerController，但明确禁止任何重生路径生成 Pawn。
	virtual void RestartPlayer(AController* NewPlayer) override;

private:
	// 在服务器 PlayerArray 中查找唯一 Lobby Host，零个或多个都视为配置不完整。
	class AArenaLobbyPlayerState* FindLobbyHostPlayerState() const;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Lobby")
	FName GameplayMapName = TEXT("/Game/TopDown/Lvl_TopDown");

	int32 ConfiguredLobbyMaxPlayers = 2;
	bool bConfiguredAsLobby = false;
};
