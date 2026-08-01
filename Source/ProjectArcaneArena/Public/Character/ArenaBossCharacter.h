#pragma once

#include "CoreMinimal.h"
#include "Character/ArenaEnemyCharacter.h"
#include "Core/ArenaGameState.h"
#include "GameplayEffectTypes.h"
#include "ArenaBossCharacter.generated.h"

class UGameplayEffect;
class UAnimMontage;

UCLASS()
class PROJECTARCANEARENA_API AArenaBossCharacter : public AArenaEnemyCharacter
{
	GENERATED_BODY()

public:
	// 设置 Boss 专属 Controller、路径朝向和显示默认值，战斗能力与死亡流程继续复用敌人基类。
	AArenaBossCharacter();

	// 返回 Boss HUD 使用的本地化名称，UI 不自行保存玩法身份。
	UFUNCTION(BlueprintPure, Category = "Arena|Boss")
	FText GetBossDisplayName() const { return BossDisplayName; }

	// 从 Boss ASC 当前标签解析阶段，避免复制第二份阶段枚举或让 UI 持有玩法状态。
	UFUNCTION(BlueprintPure, Category = "Arena|Boss")
	FGameplayTag GetCurrentBossPhaseTag() const;

	// 根据 Boss 生成时的有效 PlayerState 数量一次性应用 MaxHealth 缩放，重复调用不会叠加。
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Arena|Boss|Scaling")
	bool InitializePlayerCountScaling(int32 ParticipatingPlayerCount);

	// 返回服务器初始化时冻结的玩家数量；该调试快照不额外复制到客户端。
	UFUNCTION(BlueprintPure, Category = "Arena|Boss|Scaling")
	int32 GetScalingPlayerCountSnapshot() const { return ScalingPlayerCountSnapshot; }

	// 返回服务器已采用的初始 Health 倍率；客户端以复制的 MaxHealth 作为权威显示来源。
	UFUNCTION(BlueprintPure, Category = "Arena|Boss|Scaling")
	float GetAppliedHealthMultiplier() const { return AppliedHealthMultiplier; }

	// 在服务器登记一个由 Boss Ability 生成的敌人，并写入召唤身份及事件资格标签。
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Arena|Boss|Summons")
	bool RegisterBossSummon(AArenaEnemyCharacter* SummonedEnemy);

	// 从 Boss 活动集合移除召唤物并对称解除死亡、销毁委托和召唤标签。
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Arena|Boss|Summons")
	void UnregisterBossSummon(AArenaEnemyCharacter* SummonedEnemy);

	// 销毁并解绑当前 Boss 的全部召唤物，供死亡、终局和 Actor 销毁路径复用。
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Arena|Boss|Summons")
	void DestroyAllBossSummons();

	// 返回服务器当前仍有效且尚未死亡的 Boss 召唤物数量。
	UFUNCTION(BlueprintPure, Category = "Arena|Boss|Summons")
	int32 GetActiveSummonCount() const;

	// 返回场上召唤数量上限扣除活动数量后的可用容量。
	UFUNCTION(BlueprintPure, Category = "Arena|Boss|Summons")
	int32 GetRemainingSummonCapacity() const;

	// 检查当前是否至少能接纳请求中的一个召唤物，允许容量不足时进行部分生成。
	UFUNCTION(BlueprintPure, Category = "Arena|Boss|Summons")
	bool CanAcceptBossSummons(int32 RequestedCount) const;

protected:
	// 在敌人 GAS 初始化完成后绑定服务器阶段委托，并按当前 Combat 状态初始化 Phase 1。
	virtual void BeginPlay() override;

	// 销毁前清理全部召唤物，再对称解除阶段委托并移除 Enrage GE、Cue 与阶段标签。
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss")
	FText BossDisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Death")
	TObjectPtr<UAnimMontage> DeathMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Death", meta = (ClampMin = "0.01"))
	float DeathMontagePlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Phase", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PhaseTwoHealthRatio = 0.70f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Phase", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PhaseThreeHealthRatio = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Phase")
	TSubclassOf<UGameplayEffect> EnrageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Scaling", meta = (ClampMin = "0.01"))
	float SinglePlayerHealthMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Scaling", meta = (ClampMin = "0.01"))
	float TwoPlayerHealthMultiplier = 1.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Scaling", meta = (ClampMin = "0.01"))
	float ThreePlayerHealthMultiplier = 2.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Scaling", meta = (ClampMin = "0.01"))
	float FourPlayerHealthMultiplier = 2.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Scaling")
	TSubclassOf<UGameplayEffect> PlayerCountScalingEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Summons", meta = (ClampMin = "1"))
	int32 MaxActiveSummons = 4;

private:
	// 使用指定的 SetByCaller Instant GE 写入 MaxHealth 增量，原生 Effect Class 也可用于失败回滚。
	bool ApplyPlayerCountMaxHealthDelta(
		float MaxHealthDelta,
		TSubclassOf<UGameplayEffect> ScalingEffectClass);

	// 通过 Healing Meta Attribute 把 Boss 当前 Health 恢复到缩放后的 MaxHealth。
	bool RestoreHealthAfterPlayerCountScaling();

	// 注册 Health、死亡和 GameState 阶段委托，所有阶段写入仅在服务器执行。
	void BindBossPhaseDelegates();

	// 对称解除阶段相关委托，支持关卡切换和 Actor 销毁。
	void UnbindBossPhaseDelegates();

	// 为当前 Combat 生命周期添加初始 Phase 1，不播放转换 Cue。
	void InitializeBossPhaseState();

	// 根据最新 Health 比例单向推进到目标阶段，跨阈值时只提交最终阶段。
	void EvaluateBossPhase(float NewHealth);

	// 先添加新阶段标签再移除旧标签，并按需应用 Enrage 与转换 Cue。
	void AdvanceToBossPhase(const FGameplayTag& NewPhaseTag, int32 NewPhaseNumber);

	// 应用一次配置的 Infinite Enrage GE，并保存 Handle 用于确定性清理。
	void ApplyEnrageEffect();

	// 移除阶段标签和 Enrage GE，供死亡、终局和销毁路径复用。
	void CleanupBossPhaseState();

	// 在服务器执行一次阶段转换 Cue，RawMagnitude 携带目标阶段编号。
	void ExecutePhaseTransitionCue(int32 NewPhaseNumber);

	// 把阶段叶标签映射为单调递增序号，用于阻止治疗导致阶段回退。
	static int32 GetBossPhaseNumber(const FGameplayTag& PhaseTag);

	// Health Attribute 变化时在服务器重新计算阶段，零生命不会触发临死狂暴。
	void HandleBossPhaseHealthChanged(const FOnAttributeChangeData& Data);

	// Boss 死亡广播到达时播放本地 Montage，并由服务器清理状态及执行唯一死亡 Cue。
	UFUNCTION()
	void HandleBossPhaseDeath(AArenaEnemyCharacter* Enemy);

	// GameState 离开 Combat 时清理召唤物和阶段；重新进入时仅为仍存活 Boss 初始化 Phase 1。
	UFUNCTION()
	void HandleBossGamePhaseChanged(EArenaGamePhase OldPhase, EArenaGamePhase NewPhase);

	// 召唤物死亡广播到达时立即释放 Boss 容量，但不介入 WaveManager 的剩余数量。
	UFUNCTION()
	void HandleBossSummonDeath(AArenaEnemyCharacter* Enemy);

	// 召唤物被外部直接销毁时清理弱引用和委托，避免活动集合保留失效条目。
	UFUNCTION()
	void HandleBossSummonDestroyed(AActor* DestroyedActor);

	// 给召唤物 ASC 添加身份、OnKill 和 OnCrit 资格标签，并通过 replicated loose tag 同步。
	void AddBossSummonGameplayTags(AArenaEnemyCharacter* SummonedEnemy) const;

	// 对称移除本 Boss 添加的召唤身份和事件资格标签。
	void RemoveBossSummonGameplayTags(AArenaEnemyCharacter* SummonedEnemy) const;

	TWeakObjectPtr<AArenaGameState> BoundBossGameState;
	TSet<TWeakObjectPtr<AArenaEnemyCharacter>> ActiveBossSummons;
	FDelegateHandle BossPhaseHealthChangedDelegateHandle;
	FActiveGameplayEffectHandle EnrageEffectHandle;
	int32 ScalingPlayerCountSnapshot = 0;
	float AppliedHealthMultiplier = 1.0f;
	bool bHasAttemptedPlayerCountScaling = false;
	bool bSuppressBossPhaseEvaluation = false;
	bool bHasPlayedBossDeathPresentation = false;
	bool bHasExecutedBossDeathCue = false;
};
