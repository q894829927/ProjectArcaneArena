#include "Core/ArenaWaveManager.h"

#include "Character/ArenaBossCharacter.h"
#include "Character/ArenaEnemyCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Core/ArenaGameState.h"
#include "Core/ArenaPlayerState.h"
#include "Core/ArenaWaveDataAsset.h"
#include "Engine/TargetPoint.h"
#include "EngineUtils.h"
#include "Item/ArenaPickupActor.h"
#include "Item/ArenaPickupDropTableDataAsset.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogArenaWaves, Log, All);

// WaveManager 只在服务器运行；复制数据集中写入 ArenaGameState。
AArenaWaveManager::AArenaWaveManager()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
}

// 注入波次和掉落数据，使用派生种子隔离升级与掉落随机序列。
void AArenaWaveManager::Initialize(
	UArenaWaveDataAsset* InWaveData,
	UArenaPickupDropTableDataAsset* InPickupDropTable,
	int32 InMatchRandomSeed)
{
	if (!HasAuthority())
	{
		return;
	}

	WaveData = InWaveData;
	PickupDropTable = InPickupDropTable;
	constexpr int32 PickupSeedSalt = 0x4C4F4F54;
	int32 PickupSeed = InMatchRandomSeed ^ PickupSeedSalt;
	if (PickupSeed == 0)
	{
		PickupSeed = 1;
	}
	PickupRandomStream.Initialize(PickupSeed);
	CollectSpawnPoints();
	if (!WaveData)
	{
		UE_LOG(LogArenaWaves, Error, TEXT("WaveManager has no WaveData asset."));
	}
	if (SpawnPoints.IsEmpty())
	{
		UE_LOG(LogArenaWaves, Error, TEXT("No TargetPoint with Actor Tag '%s' was found."), *SpawnPointActorTag.ToString());
	}
	if (!PickupDropTable)
	{
		UE_LOG(LogArenaWaves, Warning, TEXT("WaveManager has no PickupDropTable; enemy drops are disabled."));
	}
	else
	{
		// 启动时即报告空表或全部无效权重，避免错误配置只在偶然抽中掉落时才暴露。
		const bool bHasValidPickupEntry = PickupDropTable->Entries.ContainsByPredicate(
			[](const FArenaPickupDropEntry& Entry)
			{
				return Entry.PickupClass && Entry.Weight > 0.0f;
			});
		if (!bHasValidPickupEntry)
		{
			UE_LOG(LogArenaWaves, Warning, TEXT("PickupDropTable has no valid positive-weight entry; enemy drops are disabled."));
		}
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

	if (!ValidateWaveConfiguration(NextWaveArrayIndex) || !BuildPendingSpawnList(NextWaveArrayIndex))
	{
		UE_LOG(LogArenaWaves, Error, TEXT("Wave %d has invalid enemy or Boss configuration; phase remains unchanged."), NextWaveArrayIndex + 1);
		return;
	}

	CurrentWaveArrayIndex = NextWaveArrayIndex;
	NextPendingSpawnIndex = 0;
	bSpawnFailureInCurrentWave = false;
	bCurrentWaveIsBossWave = WaveData->Waves[CurrentWaveArrayIndex].bBossWave;
	ArenaGameState->SetActiveBoss(nullptr);
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
	if (AArenaGameState* ArenaGameState = GetWorld() ? GetWorld()->GetGameState<AArenaGameState>() : nullptr)
	{
		ArenaGameState->SetActiveBoss(nullptr);
	}
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
	if (HasAuthority())
	{
		if (AArenaGameState* ArenaGameState = GetWorld() ? GetWorld()->GetGameState<AArenaGameState>() : nullptr)
		{
			ArenaGameState->SetActiveBoss(nullptr);
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

// Boss 波要求唯一条目、唯一数量且 Class 继承 ArenaBossCharacter，避免错误资产进入 Boss 生命周期。
bool AArenaWaveManager::ValidateWaveConfiguration(int32 WaveArrayIndex) const
{
	if (!WaveData || !WaveData->Waves.IsValidIndex(WaveArrayIndex))
	{
		return false;
	}

	const FArenaWaveConfig& WaveConfig = WaveData->Waves[WaveArrayIndex];
	if (!WaveConfig.bBossWave)
	{
		return true;
	}

	if (WaveConfig.Enemies.Num() != 1)
	{
		UE_LOG(LogArenaWaves, Error, TEXT("Boss wave %d must contain exactly one enemy entry."), WaveArrayIndex + 1);
		return false;
	}

	const FArenaWaveEnemyEntry& BossEntry = WaveConfig.Enemies[0];
	const bool bValidBossEntry = BossEntry.EnemyClass
		&& BossEntry.EnemyClass->IsChildOf(AArenaBossCharacter::StaticClass())
		&& BossEntry.Count == 1;
	if (!bValidBossEntry)
	{
		UE_LOG(LogArenaWaves, Error, TEXT("Boss wave %d must spawn exactly one ArenaBossCharacter subclass."), WaveArrayIndex + 1);
	}
	return bValidBossEntry;
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

// 每次计时器只生成一个敌人；Boss 会先完成人数缩放再写入 ActiveBoss 和剩余数量。
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
		if (bCurrentWaveIsBossWave)
		{
			AArenaBossCharacter* Boss = Cast<AArenaBossCharacter>(Enemy);
			if (Boss && !Boss->InitializePlayerCountScaling(GetBossScalingPlayerCount()))
			{
				UE_LOG(
					LogArenaWaves,
					Error,
					TEXT("Boss %s player-count scaling did not complete; inspect LogArenaBoss for rollback status."),
					*GetNameSafe(Boss));
			}
			if (AArenaGameState* ArenaGameState = GetWorld() ? GetWorld()->GetGameState<AArenaGameState>() : nullptr)
			{
				ArenaGameState->SetActiveBoss(Boss);
			}
		}
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

// 使用 GameState.PlayerArray 的稳定服务器快照统计参与者，不要求 Pawn 存活以避免死亡降低 Boss 初始难度。
int32 AArenaWaveManager::GetBossScalingPlayerCount() const
{
	const AArenaGameState* ArenaGameState = GetWorld() ? GetWorld()->GetGameState<AArenaGameState>() : nullptr;
	if (!ArenaGameState)
	{
		UE_LOG(LogArenaWaves, Warning, TEXT("Boss scaling could not find ArenaGameState; single-player fallback will be used."));
		return 0;
	}

	int32 ParticipatingPlayerCount = 0;
	for (APlayerState* CandidatePlayerState : ArenaGameState->PlayerArray)
	{
		const AArenaPlayerState* ArenaPlayerState = Cast<AArenaPlayerState>(CandidatePlayerState);
		if (ArenaPlayerState && ArenaPlayerState->GetArenaAbilitySystemComponent())
		{
			++ParticipatingPlayerCount;
		}
	}
	if (ParticipatingPlayerCount == 0)
	{
		UE_LOG(
			LogArenaWaves,
			Warning,
			TEXT("Boss scaling found 0 valid ArenaPlayerState out of %d PlayerArray entries."),
			ArenaGameState->PlayerArray.Num());
	}
	return ParticipatingPlayerCount;
}

// 死亡广播只处理当前 Alive 集合中的敌人，保证计数扣减和掉落抽取最多各执行一次。
void AArenaWaveManager::HandleEnemyDeath(AArenaEnemyCharacter* Enemy)
{
	if (!HasAuthority() || !Enemy || AliveEnemies.Remove(Enemy) == 0)
	{
		return;
	}

	Enemy->OnEnemyDeath.RemoveDynamic(this, &AArenaWaveManager::HandleEnemyDeath);
	if (Cast<AArenaBossCharacter>(Enemy))
	{
		if (AArenaGameState* ArenaGameState = GetWorld() ? GetWorld()->GetGameState<AArenaGameState>() : nullptr)
		{
			ArenaGameState->SetActiveBoss(nullptr);
		}
	}
	else
	{
		// Boss Foundation 明确跳过普通恢复掉落，普通敌人保持既有全局掉落表。
		TrySpawnPickupDrop(Enemy);
	}
	UpdateReplicatedEnemyCount();
	CheckWaveCompletion();
}

// 每名受管理敌人只经过本入口一次，因此单次死亡最多生成一个共享拾取物。
void AArenaWaveManager::TrySpawnPickupDrop(const AArenaEnemyCharacter* Enemy)
{
	if (!HasAuthority() || !Enemy || !PickupDropTable || !GetWorld())
	{
		return;
	}

	const float DropChance = FMath::Clamp(PickupDropTable->DropChance, 0.0f, 1.0f);
	if (DropChance <= 0.0f || PickupRandomStream.FRand() >= DropChance)
	{
		return;
	}

	const TSubclassOf<AArenaPickupActor> PickupClass = DrawWeightedPickupClass();
	if (!PickupClass)
	{
		UE_LOG(LogArenaWaves, Warning, TEXT("Pickup drop roll succeeded, but the drop table has no valid positive-weight entry."));
		return;
	}

	float CapsuleHalfHeight = 0.0f;
	if (const UCapsuleComponent* Capsule = Enemy->GetCapsuleComponent())
	{
		CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	}
	const FVector SpawnLocation = Enemy->GetActorLocation()
		- FVector(0.0f, 0.0f, CapsuleHalfHeight)
		+ FVector(0.0f, 0.0f, 35.0f);

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AArenaPickupActor* Pickup = GetWorld()->SpawnActor<AArenaPickupActor>(
		PickupClass,
		SpawnLocation,
		FRotator::ZeroRotator,
		SpawnParameters);
	if (!Pickup)
	{
		UE_LOG(LogArenaWaves, Warning, TEXT("Failed to spawn pickup %s for enemy %s."),
			*GetNameSafe(PickupClass.Get()),
			*GetNameSafe(Enemy));
	}
}

// 只统计有效 Class 和正权重，确保错误条目不会影响其他可用掉落。
TSubclassOf<AArenaPickupActor> AArenaWaveManager::DrawWeightedPickupClass()
{
	if (!PickupDropTable)
	{
		return nullptr;
	}

	double TotalWeight = 0.0;
	for (const FArenaPickupDropEntry& Entry : PickupDropTable->Entries)
	{
		if (Entry.PickupClass && Entry.Weight > 0.0f)
		{
			TotalWeight += static_cast<double>(Entry.Weight);
		}
	}
	if (TotalWeight <= 0.0)
	{
		return nullptr;
	}

	const double Draw = static_cast<double>(PickupRandomStream.FRand()) * TotalWeight;
	double CumulativeWeight = 0.0;
	TSubclassOf<AArenaPickupActor> LastValidClass;
	for (const FArenaPickupDropEntry& Entry : PickupDropTable->Entries)
	{
		if (!Entry.PickupClass || Entry.Weight <= 0.0f)
		{
			continue;
		}

		LastValidClass = Entry.PickupClass;
		CumulativeWeight += static_cast<double>(Entry.Weight);
		if (Draw < CumulativeWeight)
		{
			return Entry.PickupClass;
		}
	}

	return LastValidClass;
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
