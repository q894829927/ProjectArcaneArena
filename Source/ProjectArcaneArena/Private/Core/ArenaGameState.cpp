#include "Core/ArenaGameState.h"

#include "Net/UnrealNetwork.h"

// 构造复制游戏状态，波次规则只在服务器写入这些字段。
AArenaGameState::AArenaGameState()
{
	bReplicates = true;
}

// 复制波次、Boss、演出时序、背包阶段规则与 Victory Ready 快照，客户端仅观察权威状态。
void AArenaGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AArenaGameState, GamePhase);
	DOREPLIFETIME(AArenaGameState, CurrentWaveIndex);
	DOREPLIFETIME(AArenaGameState, RemainingEnemyCount);
	DOREPLIFETIME(AArenaGameState, UpgradeRandomSeed);
	DOREPLIFETIME(AArenaGameState, ActiveBoss);
	DOREPLIFETIME(AArenaGameState, BossIntroTiming);
	DOREPLIFETIME(AArenaGameState, BossOutroTiming);
	DOREPLIFETIME(AArenaGameState, VictoryRestartReadyCount);
	DOREPLIFETIME(AArenaGameState, VictoryRestartRequiredCount);
	DOREPLIFETIME(AArenaGameState, bAllowInventoryOperationsWhileWaiting);
}

// Boss 演出阶段独占镜头和跳过输入，其余阶段允许打开背包查看。
bool AArenaGameState::IsInventoryViewAllowedForPhase(EArenaGamePhase Phase)
{
	return Phase != EArenaGamePhase::BossIntro
		&& Phase != EArenaGamePhase::BossOutro;
}

// Combat 与 Victory 允许完整操作；Waiting 只在测试 GameMode 显式授权时开放。
bool AArenaGameState::AreInventoryOperationsAllowedForPhase(
	EArenaGamePhase Phase,
	bool bAllowWaitingOperations)
{
	return Phase == EArenaGamePhase::Combat
		|| Phase == EArenaGamePhase::Victory
		|| (Phase == EArenaGamePhase::Waiting && bAllowWaitingOperations);
}

// 使用当前复制阶段判断本地背包是否可见。
bool AArenaGameState::CanViewInventory() const
{
	return IsInventoryViewAllowedForPhase(GamePhase);
}

// 使用服务器复制的 Waiting 测试开关统一判断拾取、使用和丢弃权限。
bool AArenaGameState::CanPerformInventoryOperations() const
{
	return AreInventoryOperationsAllowedForPhase(
		GamePhase,
		bAllowInventoryOperationsWhileWaiting);
}

// 使用 GameState 已同步的服务器时间计算 Intro 剩余秒数，避免客户端本地时钟漂移。
float AArenaGameState::GetBossIntroRemainingTime() const
{
	return FMath::Max(BossIntroTiming.EndServerTimeSeconds - GetServerWorldTimeSeconds(), 0.0f);
}

// 使用 GameState 已同步的服务器时间计算 Outro 剩余秒数，避免客户端本地时钟漂移。
float AArenaGameState::GetBossOutroRemainingTime() const
{
	return FMath::Max(BossOutroTiming.EndServerTimeSeconds - GetServerWorldTimeSeconds(), 0.0f);
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

// 保存服务器为本局生成的升级随机种子，并立即安排复制给当前及后加入的客户端。
void AArenaGameState::SetUpgradeRandomSeed(int32 NewUpgradeRandomSeed)
{
	if (!HasAuthority() || UpgradeRandomSeed == NewUpgradeRandomSeed)
	{
		return;
	}

	const int32 OldUpgradeRandomSeed = UpgradeRandomSeed;
	UpgradeRandomSeed = NewUpgradeRandomSeed;
	OnUpgradeRandomSeedChanged.Broadcast(OldUpgradeRandomSeed, UpgradeRandomSeed);
	ForceNetUpdate();
}

// 服务器切换当前 Boss 引用，并让 Listen Server 本地 HUD 与客户端 OnRep 使用同一通知路径。
void AArenaGameState::SetActiveBoss(AArenaBossCharacter* NewActiveBoss)
{
	if (!HasAuthority() || ActiveBoss == NewActiveBoss)
	{
		return;
	}

	AArenaBossCharacter* OldActiveBoss = ActiveBoss;
	ActiveBoss = NewActiveBoss;
	OnActiveBossChanged.Broadcast(OldActiveBoss, ActiveBoss);
	ForceNetUpdate();
}

// 服务器更新 Intro 截止时间，并让 Listen Server 与远端客户端走同一委托刷新路径。
void AArenaGameState::SetBossIntroTiming(const FArenaBossIntroTiming& NewTiming)
{
	if (!HasAuthority() || BossIntroTiming == NewTiming)
	{
		return;
	}

	const FArenaBossIntroTiming OldTiming = BossIntroTiming;
	BossIntroTiming = NewTiming;
	OnBossIntroTimingChanged.Broadcast(OldTiming, BossIntroTiming);
	ForceNetUpdate();
}

// 服务器更新 Outro 截止时间和死亡位置，并统一通知 Listen Server 与远端客户端。
void AArenaGameState::SetBossOutroTiming(const FArenaBossOutroTiming& NewTiming)
{
	if (!HasAuthority() || BossOutroTiming == NewTiming)
	{
		return;
	}

	const FArenaBossOutroTiming OldTiming = BossOutroTiming;
	BossOutroTiming = NewTiming;
	OnBossOutroTimingChanged.Broadcast(OldTiming, BossOutroTiming);
	ForceNetUpdate();
}

// 服务器汇总 Victory 重开确认人数，并分别广播 Ready 与 Required 的变化。
void AArenaGameState::SetVictoryRestartCounts(int32 NewReadyCount, int32 NewRequiredCount)
{
	if (!HasAuthority())
	{
		return;
	}

	NewRequiredCount = FMath::Max(NewRequiredCount, 0);
	NewReadyCount = FMath::Clamp(NewReadyCount, 0, NewRequiredCount);

	if (VictoryRestartReadyCount != NewReadyCount)
	{
		const int32 OldReadyCount = VictoryRestartReadyCount;
		VictoryRestartReadyCount = NewReadyCount;
		OnVictoryRestartReadyCountChanged.Broadcast(OldReadyCount, VictoryRestartReadyCount);
	}

	if (VictoryRestartRequiredCount != NewRequiredCount)
	{
		const int32 OldRequiredCount = VictoryRestartRequiredCount;
		VictoryRestartRequiredCount = NewRequiredCount;
		OnVictoryRestartRequiredCountChanged.Broadcast(OldRequiredCount, VictoryRestartRequiredCount);
	}

	ForceNetUpdate();
}

// 服务器在测试 GameMode 初始化时写入 Waiting 完整权限，并推动初始状态及时复制。
void AArenaGameState::SetAllowInventoryOperationsWhileWaiting(bool bAllow)
{
	if (!HasAuthority() || bAllowInventoryOperationsWhileWaiting == bAllow)
	{
		return;
	}

	bAllowInventoryOperationsWhileWaiting = bAllow;
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

// 客户端收到本局随机种子后通知 HUD，显示层不自行生成或修改种子。
void AArenaGameState::OnRep_UpgradeRandomSeed(int32 OldUpgradeRandomSeed)
{
	OnUpgradeRandomSeedChanged.Broadcast(OldUpgradeRandomSeed, UpgradeRandomSeed);
}

// 客户端在 Boss Actor 引用解析后刷新本地 HUD 绑定。
void AArenaGameState::OnRep_ActiveBoss(AArenaBossCharacter* OldActiveBoss)
{
	OnActiveBossChanged.Broadcast(OldActiveBoss, ActiveBoss);
}

// Intro 时序复制变化后刷新客户端镜头、倒计时和跳过表现。
void AArenaGameState::OnRep_BossIntroTiming(FArenaBossIntroTiming OldTiming)
{
	OnBossIntroTimingChanged.Broadcast(OldTiming, BossIntroTiming);
}

// Outro 时序复制变化后刷新客户端镜头、倒计时和跳过表现。
void AArenaGameState::OnRep_BossOutroTiming(FArenaBossOutroTiming OldTiming)
{
	OnBossOutroTimingChanged.Broadcast(OldTiming, BossOutroTiming);
}

// Ready 人数复制变化后刷新 Victory 确认状态。
void AArenaGameState::OnRep_VictoryRestartReadyCount(int32 OldReadyCount)
{
	OnVictoryRestartReadyCountChanged.Broadcast(OldReadyCount, VictoryRestartReadyCount);
}

// Required 人数复制变化后刷新 Victory 确认状态。
void AArenaGameState::OnRep_VictoryRestartRequiredCount(int32 OldRequiredCount)
{
	OnVictoryRestartRequiredCountChanged.Broadcast(OldRequiredCount, VictoryRestartRequiredCount);
}
