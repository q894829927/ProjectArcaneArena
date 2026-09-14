#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "ArenaMainMenuGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FArenaLobbyStateChangedSignature);

UCLASS()
class PROJECTARCANEARENA_API AArenaMainMenuGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	// 注册 Lobby 容量、激活状态和旅行锁的复制字段。
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// PlayerState 加入数组时广播一次，使所有客户端及时刷新成员列表。
	virtual void AddPlayerState(APlayerState* PlayerState) override;

	// PlayerState 离开数组时广播一次，使 Ready 条件和列表立即收敛。
	virtual void RemovePlayerState(APlayerState* PlayerState) override;

	// 仅服务器初始化本次 Lobby 容量与激活状态。
	void InitializeLobby(int32 InMaxPlayers, bool bInLobbyActive);

	// 仅服务器锁定或解除旅行状态，并通知所有菜单 View。
	void SetTravelStarting(bool bInTravelStarting);

	// 返回当前菜单世界是否是网络 Lobby，而不是普通首页。
	UFUNCTION(BlueprintPure, Category = "Arena|Lobby")
	bool IsLobbyActive() const { return bLobbyActive; }

	// 返回 Host 创建时选择的二至四人容量。
	UFUNCTION(BlueprintPure, Category = "Arena|Lobby")
	int32 GetLobbyMaxPlayers() const { return LobbyMaxPlayers; }

	// 返回服务器是否已锁定 Lobby 并开始 Seamless Travel。
	UFUNCTION(BlueprintPure, Category = "Arena|Lobby")
	bool IsTravelStarting() const { return bTravelStarting; }

	UPROPERTY(BlueprintAssignable, Category = "Arena|Lobby")
	FArenaLobbyStateChangedSignature OnLobbyStateChanged;

private:
	// 容量复制完成后刷新本地设置与玩家计数。
	UFUNCTION()
	void OnRep_LobbyMaxPlayers();

	// Lobby 激活状态复制完成后切换首页或房间页面。
	UFUNCTION()
	void OnRep_LobbyActive();

	// 旅行锁复制完成后禁用重复操作并显示进入战斗状态。
	UFUNCTION()
	void OnRep_TravelStarting();

	UPROPERTY(ReplicatedUsing = OnRep_LobbyMaxPlayers)
	int32 LobbyMaxPlayers = 2;

	UPROPERTY(ReplicatedUsing = OnRep_LobbyActive)
	bool bLobbyActive = false;

	UPROPERTY(ReplicatedUsing = OnRep_TravelStarting)
	bool bTravelStarting = false;
};
