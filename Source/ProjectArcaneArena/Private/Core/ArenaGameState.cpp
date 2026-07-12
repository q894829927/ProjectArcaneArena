#include "Core/ArenaGameState.h"

#include "Net/UnrealNetwork.h"

// 构造复制游戏状态，波次规则只在服务器写入这些字段。
AArenaGameState::AArenaGameState()
{
	bReplicates = true;
}

void AArenaGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AArenaGameState, GamePhase);
	DOREPLIFETIME(AArenaGameState, CurrentWaveIndex);
	DOREPLIFETIME(AArenaGameState, RemainingEnemyCount);
}

// 服务器更新游戏阶段，并让监听服务器本地 UI 与远端 OnRep 获得一致通知。
void AArenaGameState::SetGamePhase(EArenaGamePhase NewPhase)
{
	if (!HasAuthority() || GamePhase == NewPhase)
	{
		return;
	}

	const EArenaGamePhase OldPhase = GamePhase;
	GamePhase = NewPhase;
	OnGamePhaseChanged.Broadcast(OldPhase, GamePhase);
	ForceNetUpdate();
}

// 服务器更新当前波次索引，并立即通知监听服务器表现层。
void AArenaGameState::SetCurrentWaveIndex(int32 NewWaveIndex)
{
	NewWaveIndex = FMath::Max(NewWaveIndex, 0);
	if (!HasAuthority() || CurrentWaveIndex == NewWaveIndex)
	{
		return;
	}

	const int32 OldWaveIndex = CurrentWaveIndex;
	CurrentWaveIndex = NewWaveIndex;
	OnCurrentWaveIndexChanged.Broadcast(OldWaveIndex, CurrentWaveIndex);
	ForceNetUpdate();
}

// 服务器更新剩余敌人数，禁止负数进入复制状态。
void AArenaGameState::SetRemainingEnemyCount(int32 NewRemainingEnemyCount)
{
	NewRemainingEnemyCount = FMath::Max(NewRemainingEnemyCount, 0);
	if (!HasAuthority() || RemainingEnemyCount == NewRemainingEnemyCount)
	{
		return;
	}

	const int32 OldRemainingEnemyCount = RemainingEnemyCount;
	RemainingEnemyCount = NewRemainingEnemyCount;
	OnRemainingEnemyCountChanged.Broadcast(OldRemainingEnemyCount, RemainingEnemyCount);
	ForceNetUpdate();
}

void AArenaGameState::OnRep_GamePhase(EArenaGamePhase OldPhase)
{
	OnGamePhaseChanged.Broadcast(OldPhase, GamePhase);
}

void AArenaGameState::OnRep_CurrentWaveIndex(int32 OldWaveIndex)
{
	OnCurrentWaveIndexChanged.Broadcast(OldWaveIndex, CurrentWaveIndex);
}

void AArenaGameState::OnRep_RemainingEnemyCount(int32 OldRemainingEnemyCount)
{
	OnRemainingEnemyCountChanged.Broadcast(OldRemainingEnemyCount, RemainingEnemyCount);
}
