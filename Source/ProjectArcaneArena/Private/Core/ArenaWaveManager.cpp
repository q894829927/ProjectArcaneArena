#include "Core/ArenaWaveManager.h"

#include "Character/ArenaEnemyCharacter.h"
#include "Core/ArenaGameState.h"
#include "Core/ArenaWaveDataAsset.h"
#include "Engine/TargetPoint.h"
#include "EngineUtils.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogArenaWaves, Log, All);

// WaveManager 只在服务器运行；复制数据集中写入 ArenaGameState。
AArenaWaveManager::AArenaWaveManager()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
}

// 注入数据并发现关卡刷怪点，缺少配置时保持 Waiting 并输出明确日志。
void AArenaWaveManager::Initialize(UArenaWaveDataAsset* InWaveData)
{
	if (!HasAuthority())
	{
		return;
	}

	WaveData = InWaveData;
	CollectSpawnPoints();
	if (!WaveData)
	{
		UE_LOG(LogArenaWaves, Error, TEXT("WaveManager has no WaveData asset."));
	}
	if (SpawnPoints.IsEmpty())
	{
		UE_LOG(LogArenaWaves, Error, TEXT("No TargetPoint with Actor Tag '%s' was found."), *SpawnPointActorTag.ToString());
	}
}

// 从 Waiting/Upgrade 推进一波；Combat、Victory、Defeat 阶段拒绝重复调用。
void AArenaWaveManager::StartNextWave()
{
	GetWorldTimerManager().ClearTimer(AutoStartNextWaveTimerHandle);

	AArenaGameState* ArenaGameState = GetWorld() ? GetWorld()->GetGameState<AArenaGameState>() : nullptr;
	if (!HasAuthority() || !WaveData || !ArenaGameState || SpawnPoints.IsEmpty())
	{
		UE_LOG(LogArenaWaves, Error, TEXT("StartNextWave rejected because authority, WaveData, GameState, or spawn points are missing."));
		return;
	}

	const EArenaGamePhase Phase = ArenaGameState->GetGamePhase();
	if (Phase != EArenaGamePhase::Waiting && Phase != EArenaGamePhase::Upgrade)
	{
		return;
	}

	const int32 NextWaveArrayIndex = CurrentWaveArrayIndex + 1;
	if (!WaveData->Waves.IsValidIndex(NextWaveArrayIndex))
	{
		ArenaGameState->SetGamePhase(EArenaGamePhase::Victory);
		return;
	}

	if (!BuildPendingSpawnList(NextWaveArrayIndex))
	{
		UE_LOG(LogArenaWaves, Error, TEXT("Wave %d contains no valid enemy entries; phase remains unchanged."), NextWaveArrayIndex + 1);
		return;
	}

	CurrentWaveArrayIndex = NextWaveArrayIndex;
	NextPendingSpawnIndex = 0;
	bSpawnFailureInCurrentWave = false;
	ArenaGameState->SetCurrentWaveIndex(CurrentWaveArrayIndex + 1);
	ArenaGameState->SetRemainingEnemyCount(0);
	ArenaGameState->SetGamePhase(EArenaGamePhase::Combat);
	UE_LOG(LogArenaWaves, Log, TEXT("Starting wave %d with %d pending enemies."), CurrentWaveArrayIndex + 1, PendingEnemyClasses.Num());

	SpawnNextEnemy();
	if (NextPendingSpawnIndex < PendingEnemyClasses.Num())
	{
		GetWorldTimerManager().SetTimer(
			SpawnTimerHandle,
			this,
			&AArenaWaveManager::SpawnNextEnemy,
			WaveData->Waves[CurrentWaveArrayIndex].SpawnInterval,
			true);
	}
}

// Defeat 时停止所有生成和原型升级计时器，防止终局后继续推进战斗。
void AArenaWaveManager::StopForDefeat()
{
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
	GetWorldTimerManager().ClearTimer(AutoStartNextWaveTimerHandle);
	PendingEnemyClasses.Reset();
	NextPendingSpawnIndex = 0;
}

// 标记正式升级系统是否接管阶段推进，启用后禁用三秒原型回退。
void AArenaWaveManager::SetUpgradeSystemEnabled(bool bEnabled)
{
	bUpgradeSystemEnabled = bEnabled;
	if (bUpgradeSystemEnabled)
	{
		GetWorldTimerManager().ClearTimer(AutoStartNextWaveTimerHandle);
	}
}

// Actor 销毁时清理计时器和敌人死亡委托，避免世界旅行后的悬挂回调。
void AArenaWaveManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
	GetWorldTimerManager().ClearTimer(AutoStartNextWaveTimerHandle);
	for (AArenaEnemyCharacter* Enemy : AliveEnemies)
	{
		if (Enemy)
		{
			Enemy->OnEnemyDeath.RemoveDynamic(this, &AArenaWaveManager::HandleEnemyDeath);
		}
	}
	Super::EndPlay(EndPlayReason);
}

// 收集带约定 Actor Tag 的 TargetPoint，按稳定顺序轮询使用。
void AArenaWaveManager::CollectSpawnPoints()
{
	SpawnPoints.Reset();
	for (TActorIterator<ATargetPoint> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(SpawnPointActorTag))
		{
			SpawnPoints.Add(*It);
		}
	}
}

// 将波次条目展开为待生成类列表，过滤空类和非法数量。
bool AArenaWaveManager::BuildPendingSpawnList(int32 WaveArrayIndex)
{
	PendingEnemyClasses.Reset();
	if (!WaveData || !WaveData->Waves.IsValidIndex(WaveArrayIndex))
	{
		return false;
	}

	for (const FArenaWaveEnemyEntry& Entry : WaveData->Waves[WaveArrayIndex].Enemies)
	{
		if (!Entry.EnemyClass || Entry.Count <= 0)
		{
			continue;
		}
		for (int32 CountIndex = 0; CountIndex < Entry.Count; ++CountIndex)
		{
			PendingEnemyClasses.Add(Entry.EnemyClass);
		}
	}
	return !PendingEnemyClasses.IsEmpty();
}

// 每次计时器只生成一个敌人，成功后才计入 GameState 剩余数量。
void AArenaWaveManager::SpawnNextEnemy()
{
	if (!PendingEnemyClasses.IsValidIndex(NextPendingSpawnIndex) || SpawnPoints.IsEmpty())
	{
		GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
		CheckWaveCompletion();
		return;
	}

	ATargetPoint* SpawnPoint = SpawnPoints[NextSpawnPointIndex % SpawnPoints.Num()];
	++NextSpawnPointIndex;
	const TSubclassOf<AArenaEnemyCharacter> EnemyClass = PendingEnemyClasses[NextPendingSpawnIndex++];

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AArenaEnemyCharacter* Enemy = GetWorld()->SpawnActor<AArenaEnemyCharacter>(
		EnemyClass,
		SpawnPoint->GetActorTransform(),
		SpawnParameters);
	if (Enemy)
	{
		AliveEnemies.Add(Enemy);
		Enemy->OnEnemyDeath.AddUniqueDynamic(this, &AArenaWaveManager::HandleEnemyDeath);
		UpdateReplicatedEnemyCount();
	}
	else
	{
		bSpawnFailureInCurrentWave = true;
		UE_LOG(LogArenaWaves, Error, TEXT("Failed to spawn enemy %s in wave %d."), *GetNameSafe(EnemyClass.Get()), CurrentWaveArrayIndex + 1);
	}

	if (NextPendingSpawnIndex >= PendingEnemyClasses.Num())
	{
		GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
		CheckWaveCompletion();
	}
}

// 死亡广播只处理当前 Alive 集合中的敌人，保证计数最多扣减一次。
void AArenaWaveManager::HandleEnemyDeath(AArenaEnemyCharacter* Enemy)
{
	if (!HasAuthority() || !Enemy || AliveEnemies.Remove(Enemy) == 0)
	{
		return;
	}

	Enemy->OnEnemyDeath.RemoveDynamic(this, &AArenaWaveManager::HandleEnemyDeath);
	UpdateReplicatedEnemyCount();
	CheckWaveCompletion();
}

// 全部敌人清空后进入 Victory 或广播正式 Upgrade 入口，生成失败则保留 Combat 供排错。
void AArenaWaveManager::CheckWaveCompletion()
{
	if (NextPendingSpawnIndex < PendingEnemyClasses.Num() || !AliveEnemies.IsEmpty() || bSpawnFailureInCurrentWave)
	{
		return;
	}

	AArenaGameState* ArenaGameState = GetWorld() ? GetWorld()->GetGameState<AArenaGameState>() : nullptr;
	if (!ArenaGameState || !WaveData)
	{
		return;
	}

	PendingEnemyClasses.Reset();
	if (CurrentWaveArrayIndex >= WaveData->Waves.Num() - 1)
	{
		ArenaGameState->SetGamePhase(EArenaGamePhase::Victory);
		UE_LOG(LogArenaWaves, Log, TEXT("Wave %d cleared. All configured waves are complete; entering Victory."), CurrentWaveArrayIndex + 1);
	}
	else
	{
		ArenaGameState->SetGamePhase(EArenaGamePhase::Upgrade);
		UE_LOG(LogArenaWaves, Log, TEXT("Wave %d cleared. Entering Upgrade before wave %d."), CurrentWaveArrayIndex + 1, CurrentWaveArrayIndex + 2);
		OnUpgradePhaseStarted.Broadcast();

		// 升级选择系统尚未接入时自动推进，确保原型可以完整跑通三波与 Victory。
		if (!bUpgradeSystemEnabled && bAutoStartNextWaveWithoutUpgradeSystem)
		{
			GetWorldTimerManager().SetTimer(
				AutoStartNextWaveTimerHandle,
				this,
				&AArenaWaveManager::StartNextWave,
				FMath::Max(PrototypeUpgradePhaseDuration, 0.1f),
				false);
		}
	}
}

// 把服务器 AliveEnemies 数量写入复制 GameState，HUD 不自行统计敌人。
void AArenaWaveManager::UpdateReplicatedEnemyCount()
{
	if (AArenaGameState* ArenaGameState = GetWorld() ? GetWorld()->GetGameState<AArenaGameState>() : nullptr)
	{
		ArenaGameState->SetRemainingEnemyCount(AliveEnemies.Num());
	}
}
