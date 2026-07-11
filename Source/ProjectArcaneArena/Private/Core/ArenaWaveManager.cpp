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

void AArenaWaveManager::StopForDefeat()
{
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
	PendingEnemyClasses.Reset();
	NextPendingSpawnIndex = 0;
}

void AArenaWaveManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
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

// 只有全部成功生成且存活数为零时才进入 Upgrade/Victory，生成失败会保留 Combat 供排错。
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
	}
	else
	{
		ArenaGameState->SetGamePhase(EArenaGamePhase::Upgrade);
	}
}

void AArenaWaveManager::UpdateReplicatedEnemyCount()
{
	if (AArenaGameState* ArenaGameState = GetWorld() ? GetWorld()->GetGameState<AArenaGameState>() : nullptr)
	{
		ArenaGameState->SetRemainingEnemyCount(AliveEnemies.Num());
	}
}
