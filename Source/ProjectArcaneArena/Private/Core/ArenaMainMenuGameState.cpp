#include "Core/ArenaMainMenuGameState.h"

#include "Net/UnrealNetwork.h"

// 菜单 GameState 仅复制 Lobby 展示与旅行锁，不承载正式游戏阶段数据。
void AArenaMainMenuGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AArenaMainMenuGameState, LobbyMaxPlayers);
	DOREPLIFETIME(AArenaMainMenuGameState, bLobbyActive);
	DOREPLIFETIME(AArenaMainMenuGameState, bTravelStarting);
}

// 保留引擎 PlayerArray 维护后广播，客户端可据此重建稳定排序的 Lobby 成员列表。
void AArenaMainMenuGameState::AddPlayerState(APlayerState* PlayerState)
{
	Super::AddPlayerState(PlayerState);
	OnLobbyStateChanged.Broadcast();
}

// 成员离开后立即广播，Host Start 条件不等待额外轮询。
void AArenaMainMenuGameState::RemovePlayerState(APlayerState* PlayerState)
{
	Super::RemovePlayerState(PlayerState);
	OnLobbyStateChanged.Broadcast();
}

// Lobby 参数由服务器 URL 初始化并限制为设计支持的二至四人。
void AArenaMainMenuGameState::InitializeLobby(const int32 InMaxPlayers, const bool bInLobbyActive)
{
	if (!HasAuthority())
	{
		return;
	}

	LobbyMaxPlayers = FMath::Clamp(InMaxPlayers, 2, 4);
	bLobbyActive = bInLobbyActive;
	bTravelStarting = false;
	OnLobbyStateChanged.Broadcast();
	ForceNetUpdate();
}

// 服务器锁定后客户端只能观察连接状态，不能再改变 Ready 或重复开始比赛。
void AArenaMainMenuGameState::SetTravelStarting(const bool bInTravelStarting)
{
	if (!HasAuthority() || bTravelStarting == bInTravelStarting)
	{
		return;
	}

	bTravelStarting = bInTravelStarting;
	OnLobbyStateChanged.Broadcast();
	ForceNetUpdate();
}

// 容量变化只刷新 View，不在客户端决定能否加入或开始。
void AArenaMainMenuGameState::OnRep_LobbyMaxPlayers()
{
	OnLobbyStateChanged.Broadcast();
}

// Lobby 激活复制到达时驱动页面切换和连接状态确认。
void AArenaMainMenuGameState::OnRep_LobbyActive()
{
	OnLobbyStateChanged.Broadcast();
}

// 旅行锁复制到达时驱动 Connecting 页面并禁用所有 Lobby 操作。
void AArenaMainMenuGameState::OnRep_TravelStarting()
{
	OnLobbyStateChanged.Broadcast();
}
