#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ArenaBalanceTelemetryComponent.generated.h"

class AArenaBossCharacter;
class AArenaPlayerState;
class UAbilitySystemComponent;
struct FGameplayEffectSpec;

enum class EArenaGamePhase : uint8;

// 区分已成功提交的 Pickup 事务，统计层只观察结果而不参与背包或掉落规则。
enum class EArenaBalancePickupTransaction : uint8
{
	Spawned,
	Collected,
	Used,
	Dropped
};

UCLASS(ClassGroup = (Arena), meta = (BlueprintSpawnableComponent))
class PROJECTARCANEARENA_API UArenaBalanceTelemetryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UArenaBalanceTelemetryComponent();

	// 开始一波并在首波创建新的本局统计快照；重复开始同一活动波会被拒绝。
	void BeginWave(int32 WaveIndex, bool bIsBossWave, int32 ConfiguredEnemyCount);

	// 记录服务器成功生成的受 WaveManager 管理敌人。
	void RecordEnemySpawn(bool bIsBoss);

	// 记录服务器确认的受 WaveManager 管理敌人死亡。
	void RecordEnemyKilled(bool bIsBoss);

	// 结束当前活动波并固化真实持续时间；重复调用保持幂等。
	void EndWave();

	// 记录服务器确认的主动技能 Commit，客户端预测与被动技能不会进入统计。
	void RecordAbilityCommit(
		const UAbilitySystemComponent* SourceAbilitySystemComponent,
		const FGameplayTagContainer& AbilityTags);

	// 记录伤害管线最终实际消耗的 Shield 与 Health，并沿用现有伤害来源解析规则。
	void RecordAuthoritativeDamage(
		const FGameplayEffectSpec& DamageSpec,
		const UAbilitySystemComponent* SourceAbilitySystemComponent,
		const UAbilitySystemComponent* TargetAbilitySystemComponent,
		float AppliedShieldDamage,
		float AppliedHealthDamage,
		bool bCriticalHit,
		bool bKilledTarget);

	// 记录服务器已确认升级及其结果层数。
	void RecordUpgradeSelected(
		const AArenaPlayerState* PlayerState,
		FName UpgradeID,
		int32 ResultingStackCount);

	// 记录成功生成、拾取、使用或丢弃后的 Pickup 事务。
	void RecordPickupTransaction(
		const AArenaPlayerState* PlayerState,
		EArenaBalancePickupTransaction Transaction,
		const FGameplayTag& ItemTag,
		int32 Quantity,
		float ActualRestoreAmount = 0.0f);

	// 记录一名玩家首次进入死亡状态，重复通知不会增加次数。
	void RecordPlayerDeath(const AArenaPlayerState* PlayerState);

	// 玩家复活后清除本次死亡门闩，使后续再次死亡可以重新计数。
	void RecordPlayerRevived(const AArenaPlayerState* PlayerState);

	// 记录 Boss 阶段单向推进并固化上一阶段真实持续时间。
	void RecordBossPhaseChanged(const AArenaBossCharacter* Boss, int32 NewPhaseNumber);

	// 记录一次实际注册成功的 Boss 召唤物。
	void RecordBossSummons(int32 SuccessfulSummonCount);

	// 观察 GameState 权威阶段变化，累计 Combat 时间并在终局只结算一次。
	void HandleGamePhaseChanged(EArenaGamePhase OldPhase, EArenaGamePhase NewPhase);

	// 以 Victory、Defeat 或 Aborted 结算本局并向四张 CSV 各追加一次。
	void FinishRun(const FString& Outcome);

	// 输出当前内存快照到日志，不结束本局也不写入 CSV。
	void DumpCurrentReport() const;

	// 返回 CVar 与构建配置共同决定的运行时启用状态。
	bool IsTelemetryEnabled() const;

protected:
	// Authority 组件记录初始阶段，客户端实例保持完全无操作。
	virtual void BeginPlay() override;

	// 有效未结算 Run 在世界退出时按 Aborted 写出一次，终局路径不会重复输出。
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FWaveRecord
	{
		int32 WaveIndex = 0;
		int32 ConfiguredEnemyCount = 0;
		int32 SpawnedEnemyCount = 0;
		int32 KilledEnemyCount = 0;
		int32 PlayerDeathCount = 0;
		int32 PickupSpawnedCount = 0;
		int32 PickupCollectedCount = 0;
		int32 PotionUsedCount = 0;
		bool bIsBossWave = false;
		bool bActive = false;
		double StartRealSeconds = 0.0;
		double EndRealSeconds = 0.0;
		double CombatRealSeconds = 0.0;
		double CombatStartRealSeconds = 0.0;
		double PlayerShieldDamage = 0.0;
		double PlayerHealthDamage = 0.0;
		double IncomingShieldDamage = 0.0;
		double IncomingHealthDamage = 0.0;
	};

	struct FPlayerRecord
	{
		FString PlayerKey;
		FString PlayerName;
		int32 AbilityCommits = 0;
		int32 CriticalHits = 0;
		int32 Kills = 0;
		int32 Deaths = 0;
		int32 PickupsCollected = 0;
		int32 ItemsDropped = 0;
		int32 HealthPotionsUsed = 0;
		int32 EnergyPotionsUsed = 0;
		double DamageDealtToShield = 0.0;
		double DamageDealtToHealth = 0.0;
		double ShieldDamageTaken = 0.0;
		double HealthDamageTaken = 0.0;
		double HealthRestored = 0.0;
		double EnergyRestored = 0.0;
		TArray<FString> UpgradeSelections;
	};

	struct FAbilityRecord
	{
		FString SourceKey;
		FString SourceName;
		FString AbilityOrStatus;
		FString DamageType;
		int32 Commits = 0;
		int32 Hits = 0;
		int32 CriticalHits = 0;
		int32 Kills = 0;
		double ShieldDamage = 0.0;
		double HealthDamage = 0.0;
	};

	// 创建首波对应的新 RunId、人数、种子和真实时间快照。
	void StartRunIfNeeded();

	// 返回当前活动波；没有活动波时返回空。
	FWaveRecord* GetActiveWaveRecord();

	// 从 PlayerState 构建单局内稳定键并按需创建玩家记录。
	FPlayerRecord* FindOrAddPlayerRecord(const AArenaPlayerState* PlayerState);

	// 从 ASC 的 Owner/Avatar 解析玩家记录，非玩家来源返回空。
	FPlayerRecord* FindOrAddPlayerRecord(const UAbilitySystemComponent* AbilitySystemComponent);

	// 从 ASC 构建来源稳定键，玩家按 PlayerId、敌人按 Class 聚合同类实例。
	static FString ResolveSourceKey(const UAbilitySystemComponent* AbilitySystemComponent);

	// 从 Ability AssetTags 选择确定性的主动技能标签。
	static FString ResolveAbilityCommitLabel(const FGameplayTagContainer& AbilityTags);

	// 沿用伤害日志规则，从升级 DataAsset、AssetTag 与来源类解析可读技能或状态。
	static FString ResolveDamageSourceLabel(const FGameplayEffectSpec& DamageSpec);

	// 从伤害 AssetTags 选择唯一元素类型，没有唯一结果时返回 Damage.Unknown。
	static FString ResolveDamageTypeLabel(const FGameplayEffectSpec& DamageSpec);

	// 获取指定来源与技能的汇总记录。
	FAbilityRecord& FindOrAddAbilityRecord(
		const UAbilitySystemComponent* SourceAbilitySystemComponent,
		const FString& AbilityOrStatus);

	// 把当前阶段截至现在的真实 Combat 时间累计进 Run。
	void AccumulateCurrentPhaseTime(double NowRealSeconds);

	// 固化当前 Boss 阶段真实时长，供终局和 Dump 共用。
	void CloseCurrentBossPhase(double NowRealSeconds);

	// 把四类汇总追加到 Saved/BalanceReports，写入失败仅记录日志。
	bool WriteCsvReports(const FString& Outcome);

	// 追加一组 CSV 行并在文件为空时先写表头。
	static bool AppendCsvRows(
		const FString& FileName,
		const FString& Header,
		const TArray<FString>& Rows);

	// 转义逗号、引号和换行，避免名称或升级列表破坏 CSV 列结构。
	static FString EscapeCsv(const FString& Value);

	// 把升级记录或阶段时长数组拼成稳定的单列文本。
	static FString JoinValues(const TArray<FString>& Values);

	FString RunId;
	FString FinalOutcome;
	TArray<FWaveRecord> WaveRecords;
	TMap<FString, FPlayerRecord> PlayerRecords;
	TMap<FString, FAbilityRecord> AbilityRecords;
	TSet<FString> DeadPlayerKeys;
	double RunStartRealSeconds = 0.0;
	double RunEndRealSeconds = 0.0;
	double LastPhaseChangeRealSeconds = 0.0;
	double TotalCombatRealSeconds = 0.0;
	double BossPhaseStartRealSeconds = 0.0;
	double BossPhaseDurations[3] = {0.0, 0.0, 0.0};
	EArenaGamePhase CurrentObservedPhase;
	int32 PlayerCountSnapshot = 0;
	int32 RandomSeedSnapshot = 0;
	int32 CurrentBossPhaseNumber = 0;
	int32 BossSummonCount = 0;
	bool bRunStarted = false;
	bool bRunFinalized = false;
};
