#pragma once

#include "CoreMinimal.h"
#include "GAS/ArenaGameplayAbility_EnemyAttackBase.h"
#include "ArenaGameplayAbility_BossSummonMinions.generated.h"

class AArenaBossCharacter;
class AArenaEnemyCharacter;
class UAnimMontage;

UCLASS(Blueprintable)
class PROJECTARCANEARENA_API UArenaGameplayAbility_BossSummonMinions
	: public UArenaGameplayAbility_EnemyAttackBase
{
	GENERATED_BODY()

public:
	// 配置仅 Phase 3 可用的服务器召唤技能、冷却和第一版生成参数。
	UArenaGameplayAbility_BossSummonMinions();

	// 在 GAS 通用资格之外检查 Boss 容量和至少一个可用 NavMesh 生成点。
	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	// 返回 BT 允许进入召唤分支的最大二维目标距离。
	virtual float GetAttackRange() const override { return AttackRange; }

protected:
	// Commit 前锁定本次合法生成点，成功后停止寻路并进入复制的 Casting 前摇。
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	// 正常完成或异常取消时移除 Casting 标签并清空本次锁定生成点。
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	// 返回 BT 和服务器激活校验共享的距离容差。
	virtual float GetAttackRangeTolerance() const override { return RangeTolerance; }
	// 返回负责同步 Boss 召唤动作的 Montage。
	virtual UAnimMontage* GetAttackMontage() const override { return SummonMontage; }
	// 返回蓝图可调的召唤动作播放倍率。
	virtual float GetAttackMontagePlayRate() const override { return MontagePlayRate; }
	// 从 Montage 起播时刻计算固定召唤前摇。
	virtual float GetAttackReleaseDelay() const override { return CastDuration; }
	// Commit 后生成点已经固定，释放时不因原目标移动或失去视线重新判定。
	virtual bool ShouldRevalidateRangeAndLineOfSightAtRelease() const override { return false; }
	// Commit 后原目标死亡不取消召唤；全员死亡会由 Defeat 生命周期取消整个 Ability。
	virtual bool ShouldRequireLivingTargetAtRelease() const override { return false; }
	// 至少存在 Montage、有效敌人 Class 和正生成参数时才允许 Commit。
	virtual bool HasRequiredAttackConfiguration() const override;
	// 前摇结束后只在服务器生成剩余容量允许的敌人，并逐个注册到 Boss 私有集合。
	virtual void ExecuteAttack(
		AArenaEnemyCharacter* SourceEnemy,
		AActor* TargetActor,
		UAbilitySystemComponent* SourceASC,
		UAbilitySystemComponent* TargetASC) override;
	// 返回附着 Boss 的一次性召唤施法表现标签。
	virtual FGameplayTag GetAttackActivationCueTag() const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Summon")
	TArray<TSubclassOf<AArenaEnemyCharacter>> SummonClasses;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Summon", meta = (ClampMin = "1"))
	int32 SummonsPerCast = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Summon", meta = (ClampMin = "0.0"))
	float AttackRange = 1200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Summon", meta = (ClampMin = "0.0"))
	float RangeTolerance = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Summon", meta = (ClampMin = "0.0"))
	float CastDuration = 0.9f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Summon", meta = (ClampMin = "1.0"))
	float SpawnRadius = 320.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Summon|Navigation", meta = (ClampMin = "1"))
	int32 SpawnCandidateCount = 16;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Summon|Navigation", meta = (ClampMin = "1.0"))
	float SpawnCandidateRingStep = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Summon|Navigation", meta = (ClampMin = "1.0"))
	float SpawnPointSeparation = 160.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Summon|Navigation", meta = (ClampMin = "1.0"))
	float SpawnCapsuleRadius = 50.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Summon|Navigation", meta = (ClampMin = "1.0"))
	float SpawnCapsuleHalfHeight = 90.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Summon|Animation")
	TObjectPtr<UAnimMontage> SummonMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Summon|Animation", meta = (ClampMin = "0.01"))
	float MontagePlayRate = 1.25f;

private:
	// 在 Boss 周围按固定环形顺序投影 NavMesh，并过滤断路、占位和过近候选点。
	bool FindSummonSpawnLocations(
		const AArenaBossCharacter* SourceBoss,
		int32 RequestedCount,
		TArray<FVector>& OutSpawnLocations) const;

	// 使用 Pawn 胶囊形状检查固定生成中心不会与世界或现有角色发生阻挡重叠。
	bool IsSummonSpawnLocationClear(
		const AArenaBossCharacter* SourceBoss,
		const FVector& SpawnLocation) const;

	// 前摇开始时添加本地及复制的 State.Casting，供 BT、客户端调试和异常取消观察。
	void AddCastingStateTag(UAbilitySystemComponent* SourceASC);

	// 释放或取消时仅移除本次 Ability 添加的 State.Casting 标签。
	void RemoveCastingStateTag(UAbilitySystemComponent* SourceASC);

	// 为每个服务器确认生成的敌人执行一次固定世界位置 Spawn Cue。
	void ExecuteSummonSpawnCue(
		UAbilitySystemComponent* SourceASC,
		AArenaEnemyCharacter* SummonedEnemy,
		const FVector& SpawnLocation) const;

	TArray<FVector> LockedSpawnLocations;
	bool bAppliedCastingStateTag = false;
};
