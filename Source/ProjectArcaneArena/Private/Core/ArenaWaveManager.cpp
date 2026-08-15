#include "Core/ArenaWaveManager.h"

#include "AbilitySystemComponent.h"
#include "Character/ArenaBossCharacter.h"
#include "Character/ArenaEnemyCharacter.h"
#include "Components/ArenaEnemyAffixComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/ArenaBalanceTelemetryComponent.h"
#include "Core/ArenaGameState.h"
#include "Core/ArenaEnemyAffixDataAsset.h"
#include "Core/ArenaPlayerController.h"
#include "Core/ArenaPlayerState.h"
#include "Core/ArenaWaveDataAsset.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GAS/ArenaAbilitySystemComponent.h"
#include "GAS/ArenaGameplayTags.h"
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

// 注入波次和掉落数据，使用不同盐值隔离升级、普通掉落、词缀与精英奖励随机序列。
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
	constexpr int32 EliteAffixSeedSalt = 0x454C4954;
	constexpr int32 EliteRewardSeedSalt = 0x52455744;
	int32 EliteAffixSeed = InMatchRandomSeed ^ EliteAffixSeedSalt;
	int32 EliteRewardSeed = InMatchRandomSeed ^ EliteRewardSeedSalt;
	EliteAffixRandomStream.Initialize(EliteAffixSeed == 0 ? 1 : EliteAffixSeed);
	EliteRewardRandomStream.Initialize(EliteRewardSeed == 0 ? 1 : EliteRewardSeed);
	bStoppingForDefeat = false;
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

// 从 Waiting/Upgrade 推进一波，并在阶段切换前创建服务器真实时间统计记录。
void AArenaWaveManager::StartNextWave()
{
	GetWorldTimerManager().ClearTimer(AutoStartNextWaveTimerHandle);
	ClearBossIntroTimer();
	ClearBossOutroTimer();

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
	ArenaGameState->SetBossOutroTiming(FArenaBossOutroTiming());
	ArenaGameState->SetCurrentWaveIndex(CurrentWaveArrayIndex + 1);
	ArenaGameState->SetRemainingEnemyCount(0);
	if (UArenaBalanceTelemetryComponent* Telemetry =
		ArenaGameState->GetBalanceTelemetryComponent())
	{
		Telemetry->BeginWave(
			CurrentWaveArrayIndex + 1,
			bCurrentWaveIsBossWave,
			PendingEnemySpawns.Num());
	}
	if (!bCurrentWaveIsBossWave)
	{
		ArenaGameState->SetGamePhase(EArenaGamePhase::Combat);
	}
	UE_LOG(LogArenaWaves, Log, TEXT("Starting wave %d with %d pending enemies."), CurrentWaveArrayIndex + 1, PendingEnemySpawns.Num());

	SpawnNextEnemy();
	if (NextPendingSpawnIndex < PendingEnemySpawns.Num())
	{
		GetWorldTimerManager().SetTimer(
			SpawnTimerHandle,
			this,
			&AArenaWaveManager::SpawnNextEnemy,
			WaveData->Waves[CurrentWaveArrayIndex].SpawnInterval,
			true);
	}
}

// Defeat 时停止生成和升级计时器，并取消易爆延迟死亡，防止终局后爆炸、掉落或推进波次。
void AArenaWaveManager::StopForDefeat()
{
	bStoppingForDefeat = true;
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
	GetWorldTimerManager().ClearTimer(AutoStartNextWaveTimerHandle);
	ClearBossIntroTimer();
	ClearBossOutroTimer();
	PendingEnemySpawns.Reset();
	NextPendingSpawnIndex = 0;
	TArray<AArenaEnemyCharacter*> AliveEnemySnapshot;
	AliveEnemySnapshot.Reserve(AliveEnemies.Num());
	for (AArenaEnemyCharacter* Enemy : AliveEnemies)
	{
		AliveEnemySnapshot.Add(Enemy);
	}
	for (AArenaEnemyCharacter* Enemy : AliveEnemySnapshot)
	{
		if (Enemy)
		{
			Enemy->CancelDeferredDeathForDefeat();
		}
	}
	if (AArenaGameState* ArenaGameState = GetWorld() ? GetWorld()->GetGameState<AArenaGameState>() : nullptr)
	{
		ArenaGameState->SetActiveBoss(nullptr);
		ArenaGameState->SetBossOutroTiming(FArenaBossOutroTiming());
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
	ClearBossIntroTimer();
	ClearBossOutroTimer();
	for (AArenaEnemyCharacter* Enemy : AliveEnemies)
	{
		if (Enemy)
		{
			Enemy->OnEnemyDeath.RemoveDynamic(this, &AArenaWaveManager::HandleEnemyDeath);
			Enemy->OnDestroyed.RemoveDynamic(this, &AArenaWaveManager::HandleEnemyDestroyed);
		}
	}
	if (HasAuthority())
	{
		if (AArenaGameState* ArenaGameState = GetWorld() ? GetWorld()->GetGameState<AArenaGameState>() : nullptr)
		{
			ArenaGameState->SetActiveBoss(nullptr);
			ArenaGameState->SetBossOutroTiming(FArenaBossOutroTiming());
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

// 在开波前验证普通词缀池和 Boss 唯一约束，配置错误不会静默生成普通敌人。
bool AArenaWaveManager::ValidateWaveConfiguration(int32 WaveArrayIndex) const
{
	if (!WaveData || !WaveData->Waves.IsValidIndex(WaveArrayIndex))
	{
		return false;
	}

	const FArenaWaveConfig& WaveConfig = WaveData->Waves[WaveArrayIndex];
	if (!WaveConfig.bBossWave)
	{
		for (const FArenaWaveEnemyEntry& Entry : WaveConfig.Enemies)
		{
			if (!Entry.EnemyClass || Entry.Count <= 0)
			{
				continue;
			}
			if (Entry.EliteCount < 0 || Entry.EliteCount > Entry.Count)
			{
				UE_LOG(LogArenaWaves, Error, TEXT("Wave %d entry %s has EliteCount %d outside [0, Count=%d]."),
					WaveArrayIndex + 1, *GetNameSafe(Entry.EnemyClass.Get()), Entry.EliteCount, Entry.Count);
				return false;
			}
			if (Entry.EliteCount == 0)
			{
				if (!Entry.EliteAffixPool.IsEmpty())
				{
					UE_LOG(LogArenaWaves, Error, TEXT("Wave %d entry %s has an AffixPool but EliteCount is zero."),
						WaveArrayIndex + 1, *GetNameSafe(Entry.EnemyClass.Get()));
					return false;
				}
				continue;
			}
			if (!WaveData->EliteBaselineEffectClass || Entry.EliteAffixPool.IsEmpty())
			{
				UE_LOG(LogArenaWaves, Error, TEXT("Wave %d elite entry %s requires EliteBaselineEffectClass and a non-empty AffixPool."),
					WaveArrayIndex + 1, *GetNameSafe(Entry.EnemyClass.Get()));
				return false;
			}
			if (!FMath::IsFinite(WaveData->EliteBaseline.MaxHealthMultiplier)
				|| WaveData->EliteBaseline.MaxHealthMultiplier < 1.0f
				|| !FMath::IsFinite(WaveData->EliteBaseline.AttackPowerMultiplier)
				|| WaveData->EliteBaseline.AttackPowerMultiplier < 1.0f
				|| !FMath::IsFinite(WaveData->EliteBaseline.DefenseBonus)
				|| WaveData->EliteBaseline.DefenseBonus < 0.0f)
			{
				UE_LOG(LogArenaWaves, Error, TEXT("Wave %d uses an invalid global elite baseline."), WaveArrayIndex + 1);
				return false;
			}
			const bool bHasValidEliteReward = PickupDropTable && PickupDropTable->Entries.ContainsByPredicate(
				[](const FArenaPickupDropEntry& DropEntry)
				{
					return DropEntry.PickupClass && DropEntry.Weight > 0.0f;
				});
			if (!bHasValidEliteReward)
			{
				UE_LOG(LogArenaWaves, Error, TEXT("Wave %d contains elites but no valid guaranteed Pickup reward entry."), WaveArrayIndex + 1);
				return false;
			}

			TSet<FName> SeenAffixIDs;
			TSet<FGameplayTag> SeenAffixTags;
			for (const UArenaEnemyAffixDataAsset* Affix : Entry.EliteAffixPool)
			{
				FText Error;
				if (!Affix || !Affix->IsRuntimeDefinitionValid(&Error)
					|| SeenAffixIDs.Contains(Affix->AffixID) || SeenAffixTags.Contains(Affix->AffixTag))
				{
					UE_LOG(LogArenaWaves, Error, TEXT("Wave %d entry %s has invalid or duplicate affix %s: %s"),
						WaveArrayIndex + 1, *GetNameSafe(Entry.EnemyClass.Get()), *GetNameSafe(Affix), *Error.ToString());
					return false;
				}
				SeenAffixIDs.Add(Affix->AffixID);
				SeenAffixTags.Add(Affix->AffixTag);
			}
		}
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
		&& BossEntry.Count == 1
		&& BossEntry.EliteCount == 0
		&& BossEntry.EliteAffixPool.IsEmpty();
	if (!bValidBossEntry)
	{
		UE_LOG(LogArenaWaves, Error, TEXT("Boss wave %d must spawn exactly one ArenaBossCharacter subclass."), WaveArrayIndex + 1);
	}
	return bValidBossEntry;
}

// 展开敌人数和精英名额；每条 Entry 随机位置，词缀池耗尽前不重复。
bool AArenaWaveManager::BuildPendingSpawnList(int32 WaveArrayIndex)
{
	PendingEnemySpawns.Reset();
	if (!WaveData || !WaveData->Waves.IsValidIndex(WaveArrayIndex))
	{
		return false;
	}

	const FArenaWaveConfig& WaveConfig = WaveData->Waves[WaveArrayIndex];
	const int32 ParticipatingPlayerCount = GetBossScalingPlayerCount();
	const float EnemyCountMultiplier = WaveConfig.bBossWave
		? 1.0f
		: GetEnemyCountMultiplier(ParticipatingPlayerCount);
	for (const FArenaWaveEnemyEntry& Entry : WaveConfig.Enemies)
	{
		if (!Entry.EnemyClass || Entry.Count <= 0)
		{
			continue;
		}
		const int32 ExpandedCount = WaveConfig.bBossWave
			? Entry.Count
			: FMath::Max(Entry.Count, FMath::RoundToInt(static_cast<float>(Entry.Count) * EnemyCountMultiplier));
		const int32 ExpandedEliteCount = WaveConfig.bBossWave
			? 0
			: FMath::Clamp(
				FMath::RoundToInt(static_cast<float>(Entry.EliteCount) * EnemyCountMultiplier),
				0,
				ExpandedCount);

		TArray<int32> SpawnIndices;
		SpawnIndices.Reserve(ExpandedCount);
		for (int32 Index = 0; Index < ExpandedCount; ++Index)
		{
			SpawnIndices.Add(Index);
		}
		for (int32 Index = SpawnIndices.Num() - 1; Index > 0; --Index)
		{
			SpawnIndices.Swap(Index, EliteAffixRandomStream.RandRange(0, Index));
		}
		TSet<int32> EliteIndices;
		for (int32 Index = 0; Index < ExpandedEliteCount; ++Index)
		{
			EliteIndices.Add(SpawnIndices[Index]);
		}

		TArray<UArenaEnemyAffixDataAsset*> AffixDeck;
		for (UArenaEnemyAffixDataAsset* Affix : Entry.EliteAffixPool)
		{
			if (Affix)
			{
				AffixDeck.Add(Affix);
			}
		}
		int32 EliteOrdinal = 0;
		for (int32 CountIndex = 0; CountIndex < ExpandedCount; ++CountIndex)
		{
			FArenaPendingEnemySpawn& PendingSpawn = PendingEnemySpawns.AddDefaulted_GetRef();
			PendingSpawn.EnemyClass = Entry.EnemyClass;
			if (!EliteIndices.Contains(CountIndex) || AffixDeck.IsEmpty())
			{
				continue;
			}
			if (EliteOrdinal % AffixDeck.Num() == 0)
			{
				for (int32 Index = AffixDeck.Num() - 1; Index > 0; --Index)
				{
					AffixDeck.Swap(Index, EliteAffixRandomStream.RandRange(0, Index));
				}
			}
			PendingSpawn.AffixData = AffixDeck[EliteOrdinal % AffixDeck.Num()];
			++EliteOrdinal;
		}
	}
	UE_LOG(
		LogArenaWaves,
		Log,
		TEXT("Expanded wave %d for %d players with %.2fx enemy-count multiplier (%d enemies)."),
		WaveArrayIndex + 1,
		ParticipatingPlayerCount,
		EnemyCountMultiplier,
		PendingEnemySpawns.Num());
	return !PendingEnemySpawns.IsEmpty();
}

// 每次计时器只生成一个敌人；成功生成后统计一次，Boss 再完成人数缩放和 Intro。
void AArenaWaveManager::SpawnNextEnemy()
{
	if (!PendingEnemySpawns.IsValidIndex(NextPendingSpawnIndex) || SpawnPoints.IsEmpty())
	{
		GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
		CheckWaveCompletion();
		return;
	}

	ATargetPoint* SpawnPoint = SpawnPoints[NextSpawnPointIndex % SpawnPoints.Num()];
	++NextSpawnPointIndex;
	const FArenaPendingEnemySpawn PendingSpawn = PendingEnemySpawns[NextPendingSpawnIndex++];
	const TSubclassOf<AArenaEnemyCharacter> EnemyClass = PendingSpawn.EnemyClass;

	AArenaEnemyCharacter* Enemy = GetWorld()->SpawnActorDeferred<AArenaEnemyCharacter>(
		EnemyClass,
		SpawnPoint->GetActorTransform(),
		this,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (Enemy && PendingSpawn.AffixData)
	{
		if (UArenaEnemyAffixComponent* AffixComponent = Enemy->GetEnemyAffixComponent())
		{
			AffixComponent->ConfigureBeforeSpawn(
				PendingSpawn.AffixData,
				WaveData->EliteBaseline,
				WaveData->EliteBaselineEffectClass);
		}
		else
		{
			UE_LOG(LogArenaWaves, Error, TEXT("Elite class %s has no EnemyAffixComponent."), *GetNameSafe(EnemyClass.Get()));
			Enemy->Destroy();
			Enemy = nullptr;
		}
	}
	if (Enemy)
	{
		Enemy->FinishSpawning(SpawnPoint->GetActorTransform());
		if (!Enemy->DidEliteInitializationSucceed())
		{
			UE_LOG(LogArenaWaves, Error, TEXT("Elite initialization failed for %s in wave %d."), *GetNameSafe(Enemy), CurrentWaveArrayIndex + 1);
			Enemy->Destroy();
			Enemy = nullptr;
		}
	}
	if (Enemy)
	{
		bool bShouldBeginBossIntro = false;
		AliveEnemies.Add(Enemy);
		Enemy->OnEnemyDeath.AddUniqueDynamic(this, &AArenaWaveManager::HandleEnemyDeath);
		Enemy->OnDestroyed.AddUniqueDynamic(this, &AArenaWaveManager::HandleEnemyDestroyed);
		if (AArenaGameState* ArenaGameState = GetWorld()
			? GetWorld()->GetGameState<AArenaGameState>()
			: nullptr)
		{
			if (UArenaBalanceTelemetryComponent* Telemetry =
				ArenaGameState->GetBalanceTelemetryComponent())
			{
				Telemetry->RecordEnemySpawn(Cast<AArenaBossCharacter>(Enemy) != nullptr);
			}
		}
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
				bShouldBeginBossIntro = Boss != nullptr;
			}
		}
		UpdateReplicatedEnemyCount();
		if (bShouldBeginBossIntro)
		{
			BeginBossIntro();
		}
	}
	else
	{
		bSpawnFailureInCurrentWave = true;
		UE_LOG(LogArenaWaves, Error, TEXT("Failed to spawn enemy %s in wave %d."), *GetNameSafe(EnemyClass.Get()), CurrentWaveArrayIndex + 1);
	}

	if (NextPendingSpawnIndex >= PendingEnemySpawns.Num())
	{
		GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
		CheckWaveCompletion();
	}
}

// Boss 已完成权威生成与缩放后写入统一时序，并只取消玩家主动技能而保留永久被动。
void AArenaWaveManager::BeginBossIntro()
{
	AArenaGameState* ArenaGameState = GetWorld() ? GetWorld()->GetGameState<AArenaGameState>() : nullptr;
	AArenaBossCharacter* Boss = ArenaGameState ? ArenaGameState->GetActiveBoss() : nullptr;
	if (!HasAuthority() || !ArenaGameState || !Boss || !bCurrentWaveIsBossWave)
	{
		UE_LOG(LogArenaWaves, Error, TEXT("Boss Intro could not start because the authority Boss wave state is incomplete."));
		return;
	}

	FGameplayTagContainer PlayerActiveAbilityTags;
	PlayerActiveAbilityTags.AddTag(ArenaGameplayTags::Ability_Type_PlayerActive);
	for (APlayerState* PlayerState : ArenaGameState->PlayerArray)
	{
		AArenaPlayerState* ArenaPlayerState = Cast<AArenaPlayerState>(PlayerState);
		if (UArenaAbilitySystemComponent* PlayerASC = ArenaPlayerState
			? ArenaPlayerState->GetArenaAbilitySystemComponent()
			: nullptr)
		{
			PlayerASC->CancelAbilities(&PlayerActiveAbilityTags);
		}
	}

	const float IntroDuration = FMath::Max(BossIntroDuration, 0.1f);
	FArenaBossIntroTiming IntroTiming;
	IntroTiming.EndServerTimeSeconds = ArenaGameState->GetServerWorldTimeSeconds() + IntroDuration;
	IntroTiming.BlendOutDuration = FMath::Clamp(BossIntroBlendDuration, 0.0f, IntroDuration);
	ArenaGameState->SetBossIntroTiming(IntroTiming);
	ArenaGameState->SetGamePhase(EArenaGamePhase::BossIntro);

	GetWorldTimerManager().SetTimer(
		BossIntroTimerHandle,
		this,
		&AArenaWaveManager::FinishBossIntro,
		IntroDuration,
		false);
	UE_LOG(LogArenaWaves, Log, TEXT("Boss Intro started for %s and will run for %.2f seconds."),
		*GetNameSafe(Boss),
		IntroDuration);
}

// Intro 截止时重新验证 Boss 与阶段，确保死亡或终局不会迟到进入 Combat。
void AArenaWaveManager::FinishBossIntro()
{
	ClearBossIntroTimer();

	AArenaGameState* ArenaGameState = GetWorld() ? GetWorld()->GetGameState<AArenaGameState>() : nullptr;
	AArenaBossCharacter* Boss = ArenaGameState ? ArenaGameState->GetActiveBoss() : nullptr;
	const UAbilitySystemComponent* BossASC = Boss ? Boss->GetAbilitySystemComponent() : nullptr;
	if (!HasAuthority()
		|| !ArenaGameState
		|| ArenaGameState->GetGamePhase() != EArenaGamePhase::BossIntro
		|| !Boss
		|| (BossASC && BossASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead)))
	{
		return;
	}

	ArenaGameState->SetGamePhase(EArenaGamePhase::Combat);
	UE_LOG(LogArenaWaves, Log, TEXT("Boss Intro finished for %s; entering Combat."), *GetNameSafe(Boss));
}

// 任一有效参战玩家完成服务器 Hold 后都可缩短 Intro，但必须保留复制的回切窗口。
bool AArenaWaveManager::RequestBossIntroSkip(AArenaPlayerController* RequestingController)
{
	AArenaGameState* ArenaGameState = GetWorld() ? GetWorld()->GetGameState<AArenaGameState>() : nullptr;
	AArenaPlayerState* RequestingPlayerState = RequestingController
		? RequestingController->GetPlayerState<AArenaPlayerState>()
		: nullptr;
	AArenaBossCharacter* Boss = ArenaGameState ? ArenaGameState->GetActiveBoss() : nullptr;
	if (!HasAuthority()
		|| !ArenaGameState
		|| ArenaGameState->GetGamePhase() != EArenaGamePhase::BossIntro
		|| !RequestingPlayerState
		|| !RequestingPlayerState->GetArenaAbilitySystemComponent()
		|| !ArenaGameState->PlayerArray.Contains(RequestingPlayerState)
		|| !Boss
		|| (Boss->GetAbilitySystemComponent()
			&& Boss->GetAbilitySystemComponent()->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead)))
	{
		return false;
	}

	const float BlendOutDuration = FMath::Max(ArenaGameState->GetBossIntroTiming().BlendOutDuration, 0.0f);
	const float RemainingTime = ArenaGameState->GetBossIntroRemainingTime();
	if (RemainingTime <= BlendOutDuration + KINDA_SMALL_NUMBER)
	{
		return false;
	}

	FArenaBossIntroTiming ShortenedTiming = ArenaGameState->GetBossIntroTiming();
	ShortenedTiming.EndServerTimeSeconds = ArenaGameState->GetServerWorldTimeSeconds() + BlendOutDuration;
	ArenaGameState->SetBossIntroTiming(ShortenedTiming);
	ClearBossIntroTimer();
	if (BlendOutDuration <= KINDA_SMALL_NUMBER)
	{
		FinishBossIntro();
	}
	else
	{
		GetWorldTimerManager().SetTimer(
			BossIntroTimerHandle,
			this,
			&AArenaWaveManager::FinishBossIntro,
			BlendOutDuration,
			false);
	}

	UE_LOG(LogArenaWaves, Log, TEXT("%s completed the Boss Intro skip hold; Combat begins after %.2f seconds."),
		*GetNameSafe(RequestingPlayerState),
		BlendOutDuration);
	return true;
}

// 所有退出路径复用同一幂等清理入口，避免迟到的 Intro 完成回调改变终局阶段。
void AArenaWaveManager::ClearBossIntroTimer()
{
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(BossIntroTimerHandle);
	}
}

// Boss 正常死亡后先结束 Boss 波统计，再冻结战斗并发布统一 Outro 时序。
void AArenaWaveManager::BeginBossOutro(AArenaBossCharacter* DeadBoss)
{
	AArenaGameState* ArenaGameState = GetWorld() ? GetWorld()->GetGameState<AArenaGameState>() : nullptr;
	if (!HasAuthority() || !ArenaGameState || !DeadBoss || !bCurrentWaveIsBossWave)
	{
		UE_LOG(LogArenaWaves, Error, TEXT("Boss Outro could not start because the authority Boss wave state is incomplete."));
		return;
	}

	ClearBossIntroTimer();
	ClearBossOutroTimer();
	if (UArenaBalanceTelemetryComponent* Telemetry =
		ArenaGameState->GetBalanceTelemetryComponent())
	{
		Telemetry->EndWave();
	}
	DeadBoss->DestroyAllBossSummons();

	FGameplayTagContainer PlayerActiveAbilityTags;
	PlayerActiveAbilityTags.AddTag(ArenaGameplayTags::Ability_Type_PlayerActive);
	for (APlayerState* PlayerState : ArenaGameState->PlayerArray)
	{
		AArenaPlayerState* ArenaPlayerState = Cast<AArenaPlayerState>(PlayerState);
		if (UArenaAbilitySystemComponent* PlayerASC = ArenaPlayerState
			? ArenaPlayerState->GetArenaAbilitySystemComponent()
			: nullptr)
		{
			PlayerASC->CancelAbilities(&PlayerActiveAbilityTags);
		}
	}

	const float OutroDuration = FMath::Max(BossOutroDuration, 0.1f);
	FArenaBossOutroTiming OutroTiming;
	OutroTiming.EndServerTimeSeconds = ArenaGameState->GetServerWorldTimeSeconds() + OutroDuration;
	OutroTiming.BlendOutDuration = FMath::Clamp(BossOutroBlendDuration, 0.0f, OutroDuration);
	OutroTiming.BossDeathLocation = DeadBoss->GetActorLocation();
	ArenaGameState->SetBossOutroTiming(OutroTiming);
	ArenaGameState->SetGamePhase(EArenaGamePhase::BossOutro);

	GetWorldTimerManager().SetTimer(
		BossOutroTimerHandle,
		this,
		&AArenaWaveManager::FinishBossOutro,
		OutroDuration,
		false);
	UE_LOG(LogArenaWaves, Log, TEXT("Boss Outro started for %s and will run for %.2f seconds."),
		*GetNameSafe(DeadBoss),
		OutroDuration);
}

// Outro 回切完成后清空死亡 Boss 引用和时序，再由服务器进入 Victory。
void AArenaWaveManager::FinishBossOutro()
{
	ClearBossOutroTimer();

	AArenaGameState* ArenaGameState = GetWorld() ? GetWorld()->GetGameState<AArenaGameState>() : nullptr;
	if (!HasAuthority() || !ArenaGameState || ArenaGameState->GetGamePhase() != EArenaGamePhase::BossOutro)
	{
		return;
	}

	ArenaGameState->SetBossOutroTiming(FArenaBossOutroTiming());
	ArenaGameState->SetActiveBoss(nullptr);
	ArenaGameState->SetGamePhase(EArenaGamePhase::Victory);
	UE_LOG(LogArenaWaves, Log, TEXT("Boss Outro finished; entering Victory."));
}

// 任一参战玩家完成服务器 Hold 后可缩短 Outro，但必须保留镜头回切时长。
bool AArenaWaveManager::RequestBossOutroSkip(AArenaPlayerController* RequestingController)
{
	AArenaGameState* ArenaGameState = GetWorld() ? GetWorld()->GetGameState<AArenaGameState>() : nullptr;
	AArenaPlayerState* RequestingPlayerState = RequestingController
		? RequestingController->GetPlayerState<AArenaPlayerState>()
		: nullptr;
	AArenaBossCharacter* Boss = ArenaGameState ? ArenaGameState->GetActiveBoss() : nullptr;
	const UAbilitySystemComponent* BossASC = Boss ? Boss->GetAbilitySystemComponent() : nullptr;
	if (!HasAuthority()
		|| !ArenaGameState
		|| ArenaGameState->GetGamePhase() != EArenaGamePhase::BossOutro
		|| !RequestingPlayerState
		|| !RequestingPlayerState->GetArenaAbilitySystemComponent()
		|| !ArenaGameState->PlayerArray.Contains(RequestingPlayerState)
		|| !Boss
		|| !BossASC
		|| !BossASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead))
	{
		return false;
	}

	const float BlendOutDuration = FMath::Max(ArenaGameState->GetBossOutroTiming().BlendOutDuration, 0.0f);
	if (ArenaGameState->GetBossOutroRemainingTime() <= BlendOutDuration + KINDA_SMALL_NUMBER)
	{
		return false;
	}

	FArenaBossOutroTiming ShortenedTiming = ArenaGameState->GetBossOutroTiming();
	ShortenedTiming.EndServerTimeSeconds = ArenaGameState->GetServerWorldTimeSeconds() + BlendOutDuration;
	ArenaGameState->SetBossOutroTiming(ShortenedTiming);
	ClearBossOutroTimer();
	if (BlendOutDuration <= KINDA_SMALL_NUMBER)
	{
		FinishBossOutro();
	}
	else
	{
		GetWorldTimerManager().SetTimer(
			BossOutroTimerHandle,
			this,
			&AArenaWaveManager::FinishBossOutro,
			BlendOutDuration,
			false);
	}

	UE_LOG(LogArenaWaves, Log, TEXT("%s completed the Boss Outro skip hold; Victory begins after %.2f seconds."),
		*GetNameSafe(RequestingPlayerState),
		BlendOutDuration);
	return true;
}

// 所有 Outro 退出路径复用同一 Timer 清理入口，防止迟到回调覆盖终局阶段。
void AArenaWaveManager::ClearBossOutroTimer()
{
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(BossOutroTimerHandle);
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

// 普通波只为三、四人增加最小数量密度，一至两人继续使用原始 DataAsset 数量。
float AArenaWaveManager::GetEnemyCountMultiplier(const int32 ParticipatingPlayerCount) const
{
	if (ParticipatingPlayerCount >= 4)
	{
		return FMath::Max(FourPlayerEnemyCountMultiplier, 1.0f);
	}
	if (ParticipatingPlayerCount == 3)
	{
		return FMath::Max(ThreePlayerEnemyCountMultiplier, 1.0f);
	}
	return 1.0f;
}

// 死亡广播只处理当前 Alive 集合并记录一次击杀；最终 Boss 正常死亡再转入 Outro。
void AArenaWaveManager::HandleEnemyDeath(AArenaEnemyCharacter* Enemy)
{
	if (!HasAuthority() || !Enemy || AliveEnemies.Remove(Enemy) == 0)
	{
		return;
	}

	Enemy->OnEnemyDeath.RemoveDynamic(this, &AArenaWaveManager::HandleEnemyDeath);
	Enemy->OnDestroyed.RemoveDynamic(this, &AArenaWaveManager::HandleEnemyDestroyed);
	if (AArenaGameState* ArenaGameState = GetWorld()
		? GetWorld()->GetGameState<AArenaGameState>()
		: nullptr)
	{
		if (UArenaBalanceTelemetryComponent* Telemetry =
			ArenaGameState->GetBalanceTelemetryComponent())
		{
			Telemetry->RecordEnemyKilled(Cast<AArenaBossCharacter>(Enemy) != nullptr);
		}
	}
	if (bStoppingForDefeat)
	{
		// 易爆取消仍记录一次真实击杀并清理 Alive 集合，但 Defeat 不生成奖励、Outro 或推进波次。
		UpdateReplicatedEnemyCount();
		return;
	}
	if (AArenaBossCharacter* DeadBoss = Cast<AArenaBossCharacter>(Enemy))
	{
		ClearBossIntroTimer();
		UpdateReplicatedEnemyCount();
		const bool bIsFinalWave = WaveData && CurrentWaveArrayIndex >= WaveData->Waves.Num() - 1;
		if (bCurrentWaveIsBossWave && bIsFinalWave && AliveEnemies.IsEmpty())
		{
			PendingEnemySpawns.Reset();
			BeginBossOutro(DeadBoss);
			return;
		}
	}
	else
	{
		// Boss Foundation 明确跳过普通恢复掉落，普通敌人保持既有全局掉落表。
		const bool bGuaranteedEliteDrop = Enemy->GetEnemyAffixComponent()
			&& Enemy->GetEnemyAffixComponent()->IsElite();
		TrySpawnPickupDrop(Enemy, bGuaranteedEliteDrop);
	}
	UpdateReplicatedEnemyCount();
	CheckWaveCompletion();
}

// 直接销毁执行无掉落清理；最终 Boss 未走正常死亡时带警告安全跳过 Outro。
void AArenaWaveManager::HandleEnemyDestroyed(AActor* DestroyedActor)
{
	AArenaEnemyCharacter* Enemy = Cast<AArenaEnemyCharacter>(DestroyedActor);
	if (!HasAuthority() || !Enemy || AliveEnemies.Remove(Enemy) == 0)
	{
		return;
	}

	Enemy->OnEnemyDeath.RemoveDynamic(this, &AArenaWaveManager::HandleEnemyDeath);
	Enemy->OnDestroyed.RemoveDynamic(this, &AArenaWaveManager::HandleEnemyDestroyed);
	if (Cast<AArenaBossCharacter>(Enemy))
	{
		ClearBossIntroTimer();
		ClearBossOutroTimer();
		if (AArenaGameState* ArenaGameState = GetWorld() ? GetWorld()->GetGameState<AArenaGameState>() : nullptr)
		{
			UE_LOG(LogArenaWaves, Warning, TEXT("Boss %s was destroyed without the normal death flow; skipping Boss Outro."), *GetNameSafe(Enemy));
			ArenaGameState->SetBossOutroTiming(FArenaBossOutroTiming());
			ArenaGameState->SetActiveBoss(nullptr);
			UpdateReplicatedEnemyCount();
			const bool bIsFinalWave = WaveData && CurrentWaveArrayIndex >= WaveData->Waves.Num() - 1;
			if (bCurrentWaveIsBossWave && bIsFinalWave && AliveEnemies.IsEmpty())
			{
				PendingEnemySpawns.Reset();
				if (UArenaBalanceTelemetryComponent* Telemetry =
					ArenaGameState->GetBalanceTelemetryComponent())
				{
					Telemetry->EndWave();
				}
				ArenaGameState->SetGamePhase(EArenaGamePhase::Victory);
				return;
			}
		}
	}
	UpdateReplicatedEnemyCount();
	CheckWaveCompletion();
}

// 每名受管理敌人只经过本入口一次；成功完成生成后统计一次世界 Pickup。
void AArenaWaveManager::TrySpawnPickupDrop(const AArenaEnemyCharacter* Enemy, bool bGuaranteedEliteDrop)
{
	if (!HasAuthority() || !Enemy || !PickupDropTable || !GetWorld())
	{
		return;
	}

	const float DropChance = FMath::Clamp(PickupDropTable->DropChance, 0.0f, 1.0f);
	if (!bGuaranteedEliteDrop && (DropChance <= 0.0f || PickupRandomStream.FRand() >= DropChance))
	{
		return;
	}

	FRandomStream& RewardRandomStream = bGuaranteedEliteDrop ? EliteRewardRandomStream : PickupRandomStream;
	const TSubclassOf<AArenaPickupActor> PickupClass = DrawWeightedPickupClass(RewardRandomStream);
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

	const FTransform SpawnTransform(FRotator::ZeroRotator, SpawnLocation);
	AArenaPickupActor* Pickup = GetWorld()->SpawnActorDeferred<AArenaPickupActor>(
		PickupClass,
		SpawnTransform,
		this,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Pickup)
	{
		UE_LOG(LogArenaWaves, Warning, TEXT("Failed to allocate deferred pickup %s for enemy %s."),
			*GetNameSafe(PickupClass.Get()),
			*GetNameSafe(Enemy));
		return;
	}

	// FinishSpawning 期间允许附近玩家立即拾取并销毁 Actor，该路径仍属于成功掉落。
	Pickup->FinishSpawning(SpawnTransform);
	if (AArenaGameState* ArenaGameState = GetWorld()->GetGameState<AArenaGameState>())
	{
		if (UArenaBalanceTelemetryComponent* Telemetry =
			ArenaGameState->GetBalanceTelemetryComponent())
		{
			Telemetry->RecordPickupTransaction(
				nullptr,
				EArenaBalancePickupTransaction::Spawned,
				FGameplayTag(),
				1);
		}
	}
}

// 只统计有效 Class 和正权重，确保错误条目不会影响其他可用掉落。
TSubclassOf<AArenaPickupActor> AArenaWaveManager::DrawWeightedPickupClass(FRandomStream& RandomStream) const
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

	const double Draw = static_cast<double>(RandomStream.FRand()) * TotalWeight;
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

// 全部敌人清空后先固化波次统计，再进入 Victory 或 Upgrade；Boss Outro 走独立路径。
void AArenaWaveManager::CheckWaveCompletion()
{
	if (bStoppingForDefeat || NextPendingSpawnIndex < PendingEnemySpawns.Num()
		|| !AliveEnemies.IsEmpty() || bSpawnFailureInCurrentWave)
	{
		return;
	}

	AArenaGameState* ArenaGameState = GetWorld() ? GetWorld()->GetGameState<AArenaGameState>() : nullptr;
	if (!ArenaGameState || !WaveData)
	{
		return;
	}

	if (ArenaGameState->GetGamePhase() == EArenaGamePhase::BossOutro)
	{
		return;
	}
	if (ArenaGameState->GetGamePhase() != EArenaGamePhase::Combat)
	{
		// Defeat 和其他非战斗阶段优先，禁止延迟死亡回调覆盖终局状态。
		return;
	}

	if (UArenaBalanceTelemetryComponent* Telemetry =
		ArenaGameState->GetBalanceTelemetryComponent())
	{
		Telemetry->EndWave();
	}
	PendingEnemySpawns.Reset();
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
