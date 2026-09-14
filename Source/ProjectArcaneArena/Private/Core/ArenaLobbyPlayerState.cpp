#include "Core/ArenaLobbyPlayerState.h"

#include "Net/UnrealNetwork.h"

// Lobby PlayerState 只复制菜单身份和 Ready，不携带正式战斗 GAS 或背包状态。
void AArenaLobbyPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AArenaLobbyPlayerState, bIsLobbyHost);
	DOREPLIFETIME(AArenaLobbyPlayerState, bIsLobbyReady);
}

// Host 身份只能由权威 GameMode 指定，并把 Host 自动视为已准备。
void AArenaLobbyPlayerState::SetLobbyHost(const bool bNewHost)
{
	if (!HasAuthority())
	{
		return;
	}

	const bool bChanged = bIsLobbyHost != bNewHost || (bNewHost && !bIsLobbyReady);
	bIsLobbyHost = bNewHost;
	if (bIsLobbyHost)
	{
		bIsLobbyReady = true;
	}
	if (bChanged)
	{
		OnLobbyPlayerStateChanged.Broadcast();
		ForceNetUpdate();
	}
}

// 非 Host Ready 由服务器 RPC 验证后写入；Host 不允许被切换成未准备。
void AArenaLobbyPlayerState::SetLobbyReady(const bool bNewReady)
{
	if (!HasAuthority() || bIsLobbyHost || bIsLobbyReady == bNewReady)
	{
		return;
	}

	bIsLobbyReady = bNewReady;
	OnLobbyPlayerStateChanged.Broadcast();
	ForceNetUpdate();
}

// 复制 Host 身份后通知本地 Controller 重建玩家列表和操作权限。
void AArenaLobbyPlayerState::OnRep_IsLobbyHost()
{
	OnLobbyPlayerStateChanged.Broadcast();
}

// 复制 Ready 结果后通知本地 View，不进行任何客户端规则推断。
void AArenaLobbyPlayerState::OnRep_IsLobbyReady()
{
	OnLobbyPlayerStateChanged.Broadcast();
}
