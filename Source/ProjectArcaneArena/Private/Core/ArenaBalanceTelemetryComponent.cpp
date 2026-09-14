#include "Core/ArenaBalanceTelemetryComponent.h"

#include "AbilitySystemComponent.h"
#include "Character/ArenaBossCharacter.h"
#include "Character/ArenaPlayerCharacter.h"
#include "Core/ArenaGameState.h"
#include "Core/ArenaLogCategories.h"
#include "Core/ArenaPlayerState.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffect.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Core/ArenaUpgradeDataAsset.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
#if UE_BUILD_SHIPPING
	constexpr int32 ArenaBalanceTelemetryDefault = 0;
#else
	constexpr int32 ArenaBalanceTelemetryDefault = 1;
#endif

	TAutoConsoleVariable<int32> CVarArenaBalanceTelemetry(
		TEXT("arena.Balance.Telemetry"),
		ArenaBalanceTelemetryDefault,
		TEXT("Enables server-only balance telemetry and CSV reports in non-Shipping builds. 0=off, 1=on."),
		ECVF_Default);

	// 控制台命令遍历 PIE 世界，但只输出 Authority GameState 的当前内存快照。
	void DumpArenaBalanceTelemetry()
	{
#if !UE_BUILD_SHIPPING
		if (!GEngine)
		{
			return;
		}

		bool bDumpedAnyServer = false;
		for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
		{
			UWorld* World = WorldContext.World();
			AArenaGameState* GameState = World ? World->GetGameState<AArenaGameState>() : nullptr;
			if (!GameState || !GameState->HasAuthority())
			{
				continue;
			}

			if (UArenaBalanceTelemetryComponent* Telemetry = GameState->GetBalanceTelemetryComponent())
			{
				Telemetry->DumpCurrentReport();
				bDumpedAnyServer = true;
			}
		}

		if (!bDumpedAnyServer)
		{
			UE_LOG(LogArenaBalance, Warning, TEXT("arena.Balance.Dump found no authority ArenaGameState."));
		}
#endif
	}

	FAutoConsoleCommand ArenaBalanceDumpCommand(
		TEXT("arena.Balance.Dump"),
		TEXT("Dumps the current authority balance telemetry snapshot to LogArenaBalance."),
		FConsoleCommandDelegate::CreateStatic(&DumpArenaBalanceTelemetry));

	// 从 ASC 的 OwnerActor 或 AvatarActor 提取 ArenaPlayerState。
	const AArenaPlayerState* ResolveArenaPlayerState(const UAbilitySystemComponent* AbilitySystemComponent)
	{
		if (!AbilitySystemComponent)
		{
			return nullptr;
		}

		if (const AArenaPlayerState* PlayerState = Cast<AArenaPlayerState>(AbilitySystemComponent->GetOwnerActor()))
		{
			return PlayerState;
		}

		const AArenaPlayerCharacter* PlayerCharacter =
			Cast<AArenaPlayerCharacter>(AbilitySystemComponent->GetAvatarActor());
		return PlayerCharacter ? PlayerCharacter->GetPlayerState<AArenaPlayerState>() : nullptr;
	}
}

// 创建无 Tick、无复制的服务器统计组件，所有接口在 Shipping 或客户端自动退化为空操作。
UArenaBalanceTelemetryComponent::UArenaBalanceTelemetryComponent()
	: CurrentObservedPhase(EArenaGamePhase::Waiting)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

// Authority 记录当前阶段，Run 仍等到第一波开始时才创建。
void UArenaBalanceTelemetryComponent::BeginPlay()
{
	Super::BeginPlay();

	const AArenaGameState* GameState = Cast<AArenaGameState>(GetOwner());
	if (GameState && GameState->HasAuthority())
	{
		CurrentObservedPhase = GameState->GetGamePhase();
		LastPhaseChangeRealSeconds = FPlatformTime::Seconds();
	}
}

// 世界退出时只为已经开始且尚未终局的 Run 写一次 Aborted，客户端和关闭统计时不写文件。
void UArenaBalanceTelemetryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsTelemetryEnabled() && bRunStarted && !bRunFinalized)
	{
		FinishRun(TEXT("Aborted"));
	}

	Super::EndPlay(EndPlayReason);
}

// 仅非 Shipping、Authority GameState 且 CVar 开启时允许记录。
bool UArenaBalanceTelemetryComponent::IsTelemetryEnabled() const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	const AArenaGameState* GameState = Cast<AArenaGameState>(GetOwner());
	return GameState
		&& GameState->HasAuthority()
		&& CVarArenaBalanceTelemetry.GetValueOnGameThread() != 0;
#endif
}

// 首波开始时冻结 RunId、人数和随机种子，后续死亡、复活或掉线不会改写人数快照。
void UArenaBalanceTelemetryComponent::StartRunIfNeeded()
{
	if (!IsTelemetryEnabled() || bRunStarted)
	{
		return;
	}

	const AArenaGameState* GameState = Cast<AArenaGameState>(GetOwner());
	if (!GameState)
	{
		return;
	}

	bRunStarted = true;
	bRunFinalized = false;
	RunStartRealSeconds = FPlatformTime::Seconds();
	LastPhaseChangeRealSeconds = RunStartRealSeconds;
	CurrentObservedPhase = GameState->GetGamePhase();
	RandomSeedSnapshot = GameState->GetUpgradeRandomSeed();
	PlayerCountSnapshot = 0;
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		const AArenaPlayerState* ArenaPlayerState = Cast<AArenaPlayerState>(PlayerState);
		if (ArenaPlayerState && ArenaPlayerState->GetAbilitySystemComponent())
		{
			++PlayerCountSnapshot;
			FindOrAddPlayerRecord(ArenaPlayerState);
		}
	}
	PlayerCountSnapshot = FMath::Max(PlayerCountSnapshot, 1);
	RunId = FString::Printf(
		TEXT("%s_%d_%s"),
		*FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")),
		RandomSeedSnapshot,
		*FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));

	UE_LOG(
		LogArenaBalance,
		Log,
		TEXT("Balance run %s started. Players=%d Seed=%d."),
		*RunId,
		PlayerCountSnapshot,
		RandomSeedSnapshot);
}

// 新波记录采用真实时间，避免 slomo 改变平衡时长结论。
void UArenaBalanceTelemetryComponent::BeginWave(
	int32 WaveIndex,
	bool bIsBossWave,
	int32 ConfiguredEnemyCount)
{
	if (!IsTelemetryEnabled())
	{
		return;
	}

	StartRunIfNeeded();
	if (!bRunStarted || GetActiveWaveRecord())
	{
		UE_LOG(LogArenaBalance, Warning, TEXT("BeginWave ignored because another balance wave is still active."));
		return;
	}

	FWaveRecord& Record = WaveRecords.AddDefaulted_GetRef();
	Record.WaveIndex = FMath::Max(WaveIndex, 0);
	Record.ConfiguredEnemyCount = FMath::Max(ConfiguredEnemyCount, 0);
	Record.bIsBossWave = bIsBossWave;
	Record.bActive = true;
	Record.StartRealSeconds = FPlatformTime::Seconds();
	if (CurrentObservedPhase == EArenaGamePhase::Combat)
	{
		Record.CombatStartRealSeconds = Record.StartRealSeconds;
	}
}

// 返回唯一活动波，历史波保持只读。
UArenaBalanceTelemetryComponent::FWaveRecord*
UArenaBalanceTelemetryComponent::GetActiveWaveRecord()
{
	for (int32 Index = WaveRecords.Num() - 1; Index >= 0; --Index)
	{
		if (WaveRecords[Index].bActive)
		{
			return &WaveRecords[Index];
		}
	}
	return nullptr;
}

// 成功生成后增加活动波计数，生成失败不会被记为敌人。
void UArenaBalanceTelemetryComponent::RecordEnemySpawn(bool bIsBoss)
{
	if (FWaveRecord* Wave = IsTelemetryEnabled() ? GetActiveWaveRecord() : nullptr)
	{
		++Wave->SpawnedEnemyCount;
		if (bIsBoss)
		{
			Wave->bIsBossWave = true;
		}
	}
}

// 正常死亡才增加击杀数，直接 Destroy 只由波次生命周期收口。
void UArenaBalanceTelemetryComponent::RecordEnemyKilled(bool bIsBoss)
{
	if (FWaveRecord* Wave = IsTelemetryEnabled() ? GetActiveWaveRecord() : nullptr)
	{
		++Wave->KilledEnemyCount;
		if (bIsBoss)
		{
			Wave->bIsBossWave = true;
		}
	}
}

// 固化活动波结束时间，并关闭仍在运行的 Boss 阶段计时。
void UArenaBalanceTelemetryComponent::EndWave()
{
	FWaveRecord* Wave = IsTelemetryEnabled() ? GetActiveWaveRecord() : nullptr;
	if (!Wave)
	{
		return;
	}

	Wave->EndRealSeconds = FPlatformTime::Seconds();
	if (CurrentObservedPhase == EArenaGamePhase::Combat && Wave->CombatStartRealSeconds > 0.0)
	{
		Wave->CombatRealSeconds +=
			FMath::Max(Wave->EndRealSeconds - Wave->CombatStartRealSeconds, 0.0);
		Wave->CombatStartRealSeconds = 0.0;
	}
	Wave->bActive = false;
	if (Wave->bIsBossWave)
	{
		CloseCurrentBossPhase(Wave->EndRealSeconds);
	}
}

// 玩家键使用 PlayerId 保持本局稳定，名称只用于报表阅读。
UArenaBalanceTelemetryComponent::FPlayerRecord*
UArenaBalanceTelemetryComponent::FindOrAddPlayerRecord(const AArenaPlayerState* PlayerState)
{
	if (!PlayerState)
	{
		return nullptr;
	}

	const FString PlayerKey = FString::Printf(TEXT("Player_%d"), PlayerState->GetPlayerId());
	FPlayerRecord& Record = PlayerRecords.FindOrAdd(PlayerKey);
	Record.PlayerKey = PlayerKey;
	Record.PlayerName = PlayerState->GetPlayerName();
	return &Record;
}

// ASC 只有归属 ArenaPlayerState 时才进入玩家汇总。
UArenaBalanceTelemetryComponent::FPlayerRecord*
UArenaBalanceTelemetryComponent::FindOrAddPlayerRecord(
	const UAbilitySystemComponent* AbilitySystemComponent)
{
	return FindOrAddPlayerRecord(ResolveArenaPlayerState(AbilitySystemComponent));
}

// 玩家使用稳定 PlayerId，同类敌人按 Class 聚合，便于比较近战、远程和 Boss 的整体贡献。
FString UArenaBalanceTelemetryComponent::ResolveSourceKey(
	const UAbilitySystemComponent* AbilitySystemComponent)
{
	if (const AArenaPlayerState* PlayerState = ResolveArenaPlayerState(AbilitySystemComponent))
	{
		return FString::Printf(TEXT("Player_%d"), PlayerState->GetPlayerId());
	}

	const AActor* Avatar = AbilitySystemComponent ? AbilitySystemComponent->GetAvatarActor() : nullptr;
	return Avatar ? Avatar->GetClass()->GetName() : TEXT("UnknownSource");
}

// 主动技能标签按字典序选择第一个 Ability 叶标签，排除分类和被动层级。
FString UArenaBalanceTelemetryComponent::ResolveAbilityCommitLabel(
	const FGameplayTagContainer& AbilityTags)
{
	TArray<FGameplayTag> Tags;
	AbilityTags.GetGameplayTagArray(Tags);
	Tags.Sort([](const FGameplayTag& Left, const FGameplayTag& Right)
	{
		return Left.ToString() < Right.ToString();
	});

	for (const FGameplayTag& Tag : Tags)
	{
		const FString TagString = Tag.ToString();
		if (TagString.StartsWith(TEXT("Ability."))
			&& !TagString.StartsWith(TEXT("Ability.Type."))
			&& !TagString.StartsWith(TEXT("Ability.Passive.")))
		{
			return TagString;
		}
	}
	return TEXT("Ability.Unknown");
}

// 与伤害日志保持同一优先级：升级 TargetAbilityTag、Burning、Ability/Status AssetTag、来源类。
FString UArenaBalanceTelemetryComponent::ResolveDamageSourceLabel(
	const FGameplayEffectSpec& DamageSpec)
{
	if (const UArenaUpgradeDataAsset* UpgradeData =
		Cast<UArenaUpgradeDataAsset>(DamageSpec.GetEffectContext().GetSourceObject()))
	{
		if (UpgradeData->TargetAbilityTag.IsValid())
		{
			return UpgradeData->TargetAbilityTag.ToString();
		}
	}

	const FString EffectClassName = GetNameSafe(DamageSpec.Def);
	if (EffectClassName.Contains(TEXT("Burning")))
	{
		return ArenaGameplayTags::Status_Burning.GetTag().ToString();
	}

	FGameplayTagContainer AssetTags;
	DamageSpec.GetAllAssetTags(AssetTags);
	TArray<FGameplayTag> Tags;
	AssetTags.GetGameplayTagArray(Tags);
	Tags.Sort([](const FGameplayTag& Left, const FGameplayTag& Right)
	{
		return Left.ToString() < Right.ToString();
	});
	for (const FGameplayTag& Tag : Tags)
	{
		const FString TagString = Tag.ToString();
		if (TagString.StartsWith(TEXT("Ability.")) || TagString.StartsWith(TEXT("Status.")))
		{
			return TagString;
		}
	}

	const UObject* SourceObject = DamageSpec.GetEffectContext().GetSourceObject();
	const FString SourceClassName = GetNameSafe(SourceObject ? SourceObject->GetClass() : nullptr);
	if (SourceClassName.Contains(TEXT("BasicAttack")))
	{
		return ArenaGameplayTags::Ability_BasicAttack.GetTag().ToString();
	}
	if (SourceClassName.Contains(TEXT("FireballProjectile")))
	{
		return ArenaGameplayTags::Ability_Fireball.GetTag().ToString();
	}
	if (SourceClassName.Contains(TEXT("LightningStormArea")))
	{
		return ArenaGameplayTags::Ability_LightningStorm.GetTag().ToString();
	}
	if (SourceClassName.Contains(TEXT("EnemyMeleeAttack")))
	{
		return ArenaGameplayTags::Ability_Enemy_MeleeAttack.GetTag().ToString();
	}
	if (SourceClassName.Contains(TEXT("EnemyProjectile")))
	{
		return ArenaGameplayTags::Ability_Enemy_RangedAttack.GetTag().ToString();
	}
	if (SourceClassName.Contains(TEXT("Overload")))
	{
		return ArenaGameplayTags::Ability_Passive_Overload.GetTag().ToString();
	}
	if (SourceObject)
	{
		return SourceObject->GetClass()->GetName();
	}
	if (DamageSpec.Def)
	{
		return DamageSpec.Def->GetClass()->GetName();
	}
	return TEXT("Damage.Unknown");
}

// 伤害类型只有恰好一个物理、火焰或闪电标签时才作为主类型记录。
FString UArenaBalanceTelemetryComponent::ResolveDamageTypeLabel(
	const FGameplayEffectSpec& DamageSpec)
{
	FGameplayTagContainer AssetTags;
	DamageSpec.GetAllAssetTags(AssetTags);
	TArray<FGameplayTag> DamageTypes;
	const FGameplayTag CandidateDamageTypes[] = {
		ArenaGameplayTags::Damage_Physical.GetTag(),
		ArenaGameplayTags::Damage_Fire.GetTag(),
		ArenaGameplayTags::Damage_Lightning.GetTag()};
	for (const FGameplayTag& Candidate : CandidateDamageTypes)
	{
		if (AssetTags.HasTagExact(Candidate))
		{
			DamageTypes.Add(Candidate);
		}
	}
	return DamageTypes.Num() == 1 ? DamageTypes[0].ToString() : TEXT("Damage.Unknown");
}

// 同一来源和技能共享一条累计记录，伤害类型不拆行以保持报表紧凑。
UArenaBalanceTelemetryComponent::FAbilityRecord&
UArenaBalanceTelemetryComponent::FindOrAddAbilityRecord(
	const UAbilitySystemComponent* SourceAbilitySystemComponent,
	const FString& AbilityOrStatus)
{
	const FString SourceKey = ResolveSourceKey(SourceAbilitySystemComponent);
	const FString MapKey = SourceKey + TEXT("|") + AbilityOrStatus;
	FAbilityRecord& Record = AbilityRecords.FindOrAdd(MapKey);
	Record.SourceKey = SourceKey;
	const AActor* SourceAvatar =
		SourceAbilitySystemComponent ? SourceAbilitySystemComponent->GetAvatarActor() : nullptr;
	Record.SourceName = SourceAvatar
		? SourceAvatar->GetClass()->GetName()
		: TEXT("UnknownSource");
	Record.AbilityOrStatus = AbilityOrStatus;
	return Record;
}

// 服务器确认 Commit 后同时累计来源技能和玩家总 Commit；被动标签不会进入本入口。
void UArenaBalanceTelemetryComponent::RecordAbilityCommit(
	const UAbilitySystemComponent* SourceAbilitySystemComponent,
	const FGameplayTagContainer& AbilityTags)
{
	if (!IsTelemetryEnabled() || !bRunStarted || !SourceAbilitySystemComponent)
	{
		return;
	}

	const FString AbilityLabel = ResolveAbilityCommitLabel(AbilityTags);
	if (AbilityLabel == TEXT("Ability.Unknown"))
	{
		return;
	}

	++FindOrAddAbilityRecord(SourceAbilitySystemComponent, AbilityLabel).Commits;
	if (FPlayerRecord* Player = FindOrAddPlayerRecord(SourceAbilitySystemComponent))
	{
		++Player->AbilityCommits;
	}
}

// 最终伤害只在权威路由中记录一次，并按玩家来源/目标更新波次与个人汇总。
void UArenaBalanceTelemetryComponent::RecordAuthoritativeDamage(
	const FGameplayEffectSpec& DamageSpec,
	const UAbilitySystemComponent* SourceAbilitySystemComponent,
	const UAbilitySystemComponent* TargetAbilitySystemComponent,
	float AppliedShieldDamage,
	float AppliedHealthDamage,
	bool bCriticalHit,
	bool bKilledTarget)
{
	if (!IsTelemetryEnabled()
		|| !bRunStarted
		|| !SourceAbilitySystemComponent
		|| !TargetAbilitySystemComponent)
	{
		return;
	}

	AppliedShieldDamage = FMath::Max(AppliedShieldDamage, 0.0f);
	AppliedHealthDamage = FMath::Max(AppliedHealthDamage, 0.0f);
	if (AppliedShieldDamage <= KINDA_SMALL_NUMBER && AppliedHealthDamage <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FString DamageSource = ResolveDamageSourceLabel(DamageSpec);
	FAbilityRecord& Ability = FindOrAddAbilityRecord(SourceAbilitySystemComponent, DamageSource);
	const FString DamageType = ResolveDamageTypeLabel(DamageSpec);
	if (Ability.DamageType.IsEmpty())
	{
		Ability.DamageType = DamageType;
	}
	else if (Ability.DamageType != DamageType)
	{
		Ability.DamageType = TEXT("Damage.Mixed");
	}
	++Ability.Hits;
	Ability.ShieldDamage += AppliedShieldDamage;
	Ability.HealthDamage += AppliedHealthDamage;
	if (bCriticalHit)
	{
		++Ability.CriticalHits;
	}
	if (bKilledTarget)
	{
		++Ability.Kills;
	}

	const bool bSourceIsPlayer = ResolveArenaPlayerState(SourceAbilitySystemComponent) != nullptr;
	const bool bTargetIsPlayer = ResolveArenaPlayerState(TargetAbilitySystemComponent) != nullptr;
	if (FPlayerRecord* SourcePlayer = FindOrAddPlayerRecord(SourceAbilitySystemComponent))
	{
		SourcePlayer->DamageDealtToShield += AppliedShieldDamage;
		SourcePlayer->DamageDealtToHealth += AppliedHealthDamage;
		SourcePlayer->CriticalHits += bCriticalHit ? 1 : 0;
		SourcePlayer->Kills += bKilledTarget ? 1 : 0;
	}
	if (FPlayerRecord* TargetPlayer = FindOrAddPlayerRecord(TargetAbilitySystemComponent))
	{
		TargetPlayer->ShieldDamageTaken += AppliedShieldDamage;
		TargetPlayer->HealthDamageTaken += AppliedHealthDamage;
		if (bKilledTarget)
		{
			RecordPlayerDeath(ResolveArenaPlayerState(TargetAbilitySystemComponent));
		}
	}

	if (FWaveRecord* Wave = GetActiveWaveRecord())
	{
		if (bSourceIsPlayer)
		{
			Wave->PlayerShieldDamage += AppliedShieldDamage;
			Wave->PlayerHealthDamage += AppliedHealthDamage;
		}
		if (bTargetIsPlayer)
		{
			Wave->IncomingShieldDamage += AppliedShieldDamage;
			Wave->IncomingHealthDamage += AppliedHealthDamage;
		}
	}

}

// 升级只在 PlayerState 完成服务器写入后记录，层数采用写入后的结果。
void UArenaBalanceTelemetryComponent::RecordUpgradeSelected(
	const AArenaPlayerState* PlayerState,
	FName UpgradeID,
	int32 ResultingStackCount)
{
	if (!IsTelemetryEnabled() || !bRunStarted || UpgradeID.IsNone())
	{
		return;
	}

	if (FPlayerRecord* Player = FindOrAddPlayerRecord(PlayerState))
	{
		Player->UpgradeSelections.Add(FString::Printf(
			TEXT("%s:%d"),
			*UpgradeID.ToString(),
			FMath::Max(ResultingStackCount, 0)));
	}
}

// 事务成功后按类型更新活动波和所属玩家，恢复量使用属性实际增量。
void UArenaBalanceTelemetryComponent::RecordPickupTransaction(
	const AArenaPlayerState* PlayerState,
	EArenaBalancePickupTransaction Transaction,
	const FGameplayTag& ItemTag,
	int32 Quantity,
	float ActualRestoreAmount)
{
	if (!IsTelemetryEnabled() || !bRunStarted || Quantity <= 0)
	{
		return;
	}

	FWaveRecord* Wave = GetActiveWaveRecord();
	FPlayerRecord* Player = FindOrAddPlayerRecord(PlayerState);
	switch (Transaction)
	{
	case EArenaBalancePickupTransaction::Spawned:
		if (Wave)
		{
			Wave->PickupSpawnedCount += Quantity;
		}
		break;
	case EArenaBalancePickupTransaction::Collected:
		if (Wave)
		{
			Wave->PickupCollectedCount += Quantity;
		}
		if (Player)
		{
			Player->PickupsCollected += Quantity;
			if (ItemTag.MatchesTag(ArenaGameplayTags::Item_Effect_Restore_Health))
			{
				Player->HealthRestored += FMath::Max(ActualRestoreAmount, 0.0f);
			}
			else if (ItemTag.MatchesTag(ArenaGameplayTags::Item_Effect_Restore_Energy))
			{
				Player->EnergyRestored += FMath::Max(ActualRestoreAmount, 0.0f);
			}
		}
		break;
	case EArenaBalancePickupTransaction::Used:
		if (Wave)
		{
			Wave->PotionUsedCount += Quantity;
		}
		if (Player)
		{
			if (ItemTag.MatchesTag(ArenaGameplayTags::Item_Effect_Restore_Health))
			{
				Player->HealthPotionsUsed += Quantity;
				Player->HealthRestored += FMath::Max(ActualRestoreAmount, 0.0f);
			}
			else if (ItemTag.MatchesTag(ArenaGameplayTags::Item_Effect_Restore_Energy))
			{
				Player->EnergyPotionsUsed += Quantity;
				Player->EnergyRestored += FMath::Max(ActualRestoreAmount, 0.0f);
			}
		}
		break;
	case EArenaBalancePickupTransaction::Dropped:
		if (Player)
		{
			Player->ItemsDropped += Quantity;
		}
		break;
	default:
		break;
	}
}

// 玩家死亡按 PlayerId 去重，复活后再次死亡仍属于同一 Run 的新死亡事件。
void UArenaBalanceTelemetryComponent::RecordPlayerDeath(const AArenaPlayerState* PlayerState)
{
	if (!IsTelemetryEnabled() || !bRunStarted || !PlayerState)
	{
		return;
	}

	const FString PlayerKey = FString::Printf(TEXT("Player_%d"), PlayerState->GetPlayerId());
	if (DeadPlayerKeys.Contains(PlayerKey))
	{
		return;
	}
	DeadPlayerKeys.Add(PlayerKey);
	if (FPlayerRecord* Player = FindOrAddPlayerRecord(PlayerState))
	{
		++Player->Deaths;
	}
	if (FWaveRecord* Wave = GetActiveWaveRecord())
	{
		++Wave->PlayerDeathCount;
	}
}

// 复活只重置统计门闩，不减少已经记录的死亡次数。
void UArenaBalanceTelemetryComponent::RecordPlayerRevived(const AArenaPlayerState* PlayerState)
{
	if (!IsTelemetryEnabled() || !bRunStarted || !PlayerState)
	{
		return;
	}

	DeadPlayerKeys.Remove(FString::Printf(TEXT("Player_%d"), PlayerState->GetPlayerId()));
}

// 单向阶段变化先固化上一阶段，再启动新阶段；首次 Phase 1 不播放 Cue 也仍进入统计。
void UArenaBalanceTelemetryComponent::RecordBossPhaseChanged(
	const AArenaBossCharacter* Boss,
	int32 NewPhaseNumber)
{
	if (!IsTelemetryEnabled()
		|| !bRunStarted
		|| !Boss
		|| NewPhaseNumber < 1
		|| NewPhaseNumber > 3)
	{
		return;
	}

	const double Now = FPlatformTime::Seconds();
	CloseCurrentBossPhase(Now);
	CurrentBossPhaseNumber = NewPhaseNumber;
	BossPhaseStartRealSeconds = Now;
}

// 只有 RegisterBossSummon 成功后才累计，容量拒绝和生成失败不计数。
void UArenaBalanceTelemetryComponent::RecordBossSummons(int32 SuccessfulSummonCount)
{
	if (IsTelemetryEnabled() && bRunStarted && SuccessfulSummonCount > 0)
	{
		BossSummonCount += SuccessfulSummonCount;
	}
}

// 当前 Boss 阶段时长使用真实时间累计，重复关闭保持无操作。
void UArenaBalanceTelemetryComponent::CloseCurrentBossPhase(double NowRealSeconds)
{
	if (CurrentBossPhaseNumber < 1 || CurrentBossPhaseNumber > 3 || BossPhaseStartRealSeconds <= 0.0)
	{
		return;
	}

	BossPhaseDurations[CurrentBossPhaseNumber - 1] +=
		FMath::Max(NowRealSeconds - BossPhaseStartRealSeconds, 0.0);
	CurrentBossPhaseNumber = 0;
	BossPhaseStartRealSeconds = 0.0;
}

// 阶段切换前累计旧阶段 Combat 秒数，其他阶段不计入战斗时长。
void UArenaBalanceTelemetryComponent::AccumulateCurrentPhaseTime(double NowRealSeconds)
{
	if (bRunStarted && CurrentObservedPhase == EArenaGamePhase::Combat)
	{
		TotalCombatRealSeconds += FMath::Max(NowRealSeconds - LastPhaseChangeRealSeconds, 0.0);
	}
	LastPhaseChangeRealSeconds = NowRealSeconds;
}

// GameState 每次权威切换阶段都经过这里，Victory/Defeat 直接触发唯一结算。
void UArenaBalanceTelemetryComponent::HandleGamePhaseChanged(
	EArenaGamePhase OldPhase,
	EArenaGamePhase NewPhase)
{
	if (!IsTelemetryEnabled())
	{
		return;
	}

	const double Now = FPlatformTime::Seconds();
	if (FWaveRecord* Wave = GetActiveWaveRecord())
	{
		if (OldPhase == EArenaGamePhase::Combat && Wave->CombatStartRealSeconds > 0.0)
		{
			Wave->CombatRealSeconds +=
				FMath::Max(Now - Wave->CombatStartRealSeconds, 0.0);
			Wave->CombatStartRealSeconds = 0.0;
		}
		if (NewPhase == EArenaGamePhase::Combat && Wave->CombatStartRealSeconds <= 0.0)
		{
			Wave->CombatStartRealSeconds = Now;
		}
	}
	CurrentObservedPhase = OldPhase;
	AccumulateCurrentPhaseTime(Now);
	CurrentObservedPhase = NewPhase;
	if (NewPhase == EArenaGamePhase::Victory)
	{
		FinishRun(TEXT("Victory"));
	}
	else if (NewPhase == EArenaGamePhase::Defeat)
	{
		FinishRun(TEXT("Defeat"));
	}
}

// 终局先关闭活动波和阶段计时，再写出四份报表；防重标记在写文件前设置。
void UArenaBalanceTelemetryComponent::FinishRun(const FString& Outcome)
{
	if (!IsTelemetryEnabled() || !bRunStarted || bRunFinalized)
	{
		return;
	}

	bRunFinalized = true;
	RunEndRealSeconds = FPlatformTime::Seconds();
	AccumulateCurrentPhaseTime(RunEndRealSeconds);
	CloseCurrentBossPhase(RunEndRealSeconds);
	if (FWaveRecord* Wave = GetActiveWaveRecord())
	{
		Wave->EndRealSeconds = RunEndRealSeconds;
		if (CurrentObservedPhase == EArenaGamePhase::Combat && Wave->CombatStartRealSeconds > 0.0)
		{
			Wave->CombatRealSeconds +=
				FMath::Max(RunEndRealSeconds - Wave->CombatStartRealSeconds, 0.0);
			Wave->CombatStartRealSeconds = 0.0;
		}
		Wave->bActive = false;
	}
	FinalOutcome = Outcome.IsEmpty() ? TEXT("Unknown") : Outcome;

	const bool bWroteReports = WriteCsvReports(FinalOutcome);
	if (bWroteReports)
	{
		UE_LOG(
			LogArenaBalance,
			Log,
			TEXT("Balance run %s finished as %s. Real=%.2fs Combat=%.2fs Waves=%d CSV=Written."),
			*RunId,
			*FinalOutcome,
			RunEndRealSeconds - RunStartRealSeconds,
			TotalCombatRealSeconds,
			WaveRecords.Num());
	}
	else
	{
		UE_LOG(
			LogArenaBalance,
			Error,
			TEXT("Balance run %s finished as %s, but one or more CSV reports could not be written."),
			*RunId,
			*FinalOutcome);
	}
}

// Dump 只输出当前快照，便于 PIE 中途确认统计，没有任何文件或玩法副作用。
void UArenaBalanceTelemetryComponent::DumpCurrentReport() const
{
	if (!IsTelemetryEnabled())
	{
		UE_LOG(LogArenaBalance, Log, TEXT("Balance telemetry is disabled."));
		return;
	}

	const double Now = FPlatformTime::Seconds();
	const double RunDuration = bRunStarted ? FMath::Max(Now - RunStartRealSeconds, 0.0) : 0.0;
	UE_LOG(
		LogArenaBalance,
		Log,
		TEXT("Run=%s Started=%s Finalized=%s Outcome=%s Real=%.2fs Combat=%.2fs Players=%d Seed=%d Waves=%d Abilities=%d Summons=%d"),
		*RunId,
		bRunStarted ? TEXT("true") : TEXT("false"),
		bRunFinalized ? TEXT("true") : TEXT("false"),
		*FinalOutcome,
		RunDuration,
		TotalCombatRealSeconds,
		PlayerCountSnapshot,
		RandomSeedSnapshot,
		WaveRecords.Num(),
		AbilityRecords.Num(),
		BossSummonCount);
}

// CSV 转义采用标准双引号规则，避免玩家名和升级文本破坏行结构。
FString UArenaBalanceTelemetryComponent::EscapeCsv(const FString& Value)
{
	FString Escaped = Value.Replace(TEXT("\""), TEXT("\"\""));
	if (Escaped.Contains(TEXT(","))
		|| Escaped.Contains(TEXT("\""))
		|| Escaped.Contains(TEXT("\n"))
		|| Escaped.Contains(TEXT("\r")))
	{
		Escaped = TEXT("\"") + Escaped + TEXT("\"");
	}
	return Escaped;
}

// 多值列以分号连接，输入顺序由权威事件发生顺序决定。
FString UArenaBalanceTelemetryComponent::JoinValues(const TArray<FString>& Values)
{
	return FString::Join(Values, TEXT(";"));
}

// 文件首次创建时写表头，之后只追加 UTF-8 数据；目录创建失败会安全返回。
bool UArenaBalanceTelemetryComponent::AppendCsvRows(
	const FString& FileName,
	const FString& Header,
	const TArray<FString>& Rows)
{
	if (Rows.IsEmpty())
	{
		return true;
	}

	const FString ReportDirectory = FPaths::ProjectSavedDir() / TEXT("BalanceReports");
	if (!IFileManager::Get().MakeDirectory(*ReportDirectory, true))
	{
		return false;
	}

	const FString FullPath = ReportDirectory / FileName;
	FString Payload;
	if (IFileManager::Get().FileSize(*FullPath) <= 0)
	{
		Payload += Header;
		Payload += LINE_TERMINATOR;
	}
	for (const FString& Row : Rows)
	{
		Payload += Row;
		Payload += LINE_TERMINATOR;
	}

	return FFileHelper::SaveStringToFile(
		Payload,
		*FullPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
		&IFileManager::Get(),
		FILEWRITE_Append);
}

// 生成 Run、Wave、Player、Ability 四张稳定列结构的报表并逐文件追加。
bool UArenaBalanceTelemetryComponent::WriteCsvReports(const FString& Outcome)
{
	const double TotalDuration = FMath::Max(RunEndRealSeconds - RunStartRealSeconds, 0.0);
	double BossCombatSeconds = 0.0;
	for (const FWaveRecord& Wave : WaveRecords)
	{
		if (Wave.bIsBossWave)
		{
			BossCombatSeconds += Wave.CombatRealSeconds;
		}
	}
	TArray<FString> RunRows;
	RunRows.Add(FString::Printf(
		TEXT("%s,%s,%d,%d,%.3f,%.3f,%d,%.3f,%.3f,%.3f,%.3f,%d"),
		*EscapeCsv(RunId),
		*EscapeCsv(Outcome),
		PlayerCountSnapshot,
		RandomSeedSnapshot,
		TotalDuration,
		TotalCombatRealSeconds,
		WaveRecords.Num(),
		BossCombatSeconds,
		BossPhaseDurations[0],
		BossPhaseDurations[1],
		BossPhaseDurations[2],
		BossSummonCount));

	TArray<FString> WaveRows;
	for (const FWaveRecord& Wave : WaveRecords)
	{
		WaveRows.Add(FString::Printf(
			TEXT("%s,%d,%s,%d,%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%d,%d,%d"),
			*EscapeCsv(RunId),
			Wave.WaveIndex,
			Wave.bIsBossWave ? TEXT("Boss") : TEXT("Normal"),
			Wave.ConfiguredEnemyCount,
			Wave.SpawnedEnemyCount,
			Wave.KilledEnemyCount,
			FMath::Max(Wave.EndRealSeconds - Wave.StartRealSeconds, 0.0),
			Wave.CombatRealSeconds,
			Wave.PlayerShieldDamage,
			Wave.PlayerHealthDamage,
			Wave.IncomingShieldDamage,
			Wave.IncomingHealthDamage,
			Wave.PlayerDeathCount,
			Wave.PickupSpawnedCount,
			Wave.PickupCollectedCount,
			Wave.PotionUsedCount));
	}

	TArray<FString> PlayerKeys;
	PlayerRecords.GetKeys(PlayerKeys);
	PlayerKeys.Sort();
	TArray<FString> PlayerRows;
	for (const FString& Key : PlayerKeys)
	{
		const FPlayerRecord& Player = PlayerRecords[Key];
		PlayerRows.Add(FString::Printf(
			TEXT("%s,%s,%s,%d,%d,%d,%d,%.3f,%.3f,%.3f,%.3f,%d,%d,%d,%d,%.3f,%.3f,%s"),
			*EscapeCsv(RunId),
			*EscapeCsv(Player.PlayerKey),
			*EscapeCsv(Player.PlayerName),
			Player.AbilityCommits,
			Player.CriticalHits,
			Player.Kills,
			Player.Deaths,
			Player.DamageDealtToShield,
			Player.DamageDealtToHealth,
			Player.ShieldDamageTaken,
			Player.HealthDamageTaken,
			Player.PickupsCollected,
			Player.ItemsDropped,
			Player.HealthPotionsUsed,
			Player.EnergyPotionsUsed,
			Player.HealthRestored,
			Player.EnergyRestored,
			*EscapeCsv(JoinValues(Player.UpgradeSelections))));
	}

	TArray<FString> AbilityKeys;
	AbilityRecords.GetKeys(AbilityKeys);
	AbilityKeys.Sort();
	TArray<FString> AbilityRows;
	for (const FString& Key : AbilityKeys)
	{
		const FAbilityRecord& Ability = AbilityRecords[Key];
		AbilityRows.Add(FString::Printf(
			TEXT("%s,%s,%s,%s,%s,%d,%d,%d,%d,%.3f,%.3f"),
			*EscapeCsv(RunId),
			*EscapeCsv(Ability.SourceKey),
			*EscapeCsv(Ability.SourceName),
			*EscapeCsv(Ability.AbilityOrStatus),
			*EscapeCsv(Ability.DamageType),
			Ability.Commits,
			Ability.Hits,
			Ability.CriticalHits,
			Ability.Kills,
			Ability.ShieldDamage,
			Ability.HealthDamage));
	}

	const bool bRunWritten = AppendCsvRows(
		TEXT("ArenaRunSummary.csv"),
		TEXT("RunId,Outcome,PlayerCount,RandomSeed,TotalRealSeconds,CombatRealSeconds,WaveCount,BossCombatSeconds,BossPhase1Seconds,BossPhase2Seconds,BossPhase3Seconds,BossSummonCount"),
		RunRows);
	const bool bWaveWritten = AppendCsvRows(
		TEXT("ArenaWaveSummary.csv"),
		TEXT("RunId,WaveIndex,WaveType,ConfiguredEnemies,SpawnedEnemies,KilledEnemies,RealSeconds,CombatSeconds,PlayerShieldDamage,PlayerHealthDamage,IncomingShieldDamage,IncomingHealthDamage,PlayerDeaths,PickupSpawned,PickupCollected,PotionUsed"),
		WaveRows);
	const bool bPlayerWritten = AppendCsvRows(
		TEXT("ArenaPlayerSummary.csv"),
		TEXT("RunId,PlayerKey,PlayerName,AbilityCommits,CriticalHits,Kills,Deaths,DamageToShield,DamageToHealth,ShieldDamageTaken,HealthDamageTaken,PickupsCollected,ItemsDropped,HealthPotionsUsed,EnergyPotionsUsed,HealthRestored,EnergyRestored,Upgrades"),
		PlayerRows);
	const bool bAbilityWritten = AppendCsvRows(
		TEXT("ArenaAbilitySummary.csv"),
		TEXT("RunId,SourceKey,SourceName,AbilityOrStatus,DamageType,Commits,Hits,CriticalHits,Kills,ShieldDamage,HealthDamage"),
		AbilityRows);
	return bRunWritten && bWaveWritten && bPlayerWritten && bAbilityWritten;
}
