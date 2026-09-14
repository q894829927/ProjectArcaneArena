#pragma once

#include "CoreMinimal.h"
#include "GAS/ArenaGameplayAbility_EnemyAttackBase.h"
#include "ArenaGameplayAbility_BossFireZone.generated.h"

class AArenaBossFireZoneArea;
class UAnimMontage;
class UGameplayEffect;

UCLASS(Blueprintable)
class PROJECTARCANEARENA_API UArenaGameplayAbility_BossFireZone : public UArenaGameplayAbility_EnemyAttackBase
{
	GENERATED_BODY()

public:
	// 配置服务器权威 FireZone 的标签、冷却和第一版默认数值。
	UArenaGameplayAbility_BossFireZone();

	// 返回 Behavior Tree 进入 FireZone 分支时使用的最大二维距离。
	virtual float GetAttackRange() const override { return AttackRange; }
	// 返回 FireZone 与 Charge 分工使用的最小二维距离。
	virtual float GetMinimumAttackRange() const override { return MinimumAttackRange; }

protected:
	// Commit 前解析玩家脚下地面；成功后锁定位置并启动固定预警和 Casting 状态。
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	// 取消、死亡或正常结束时幂等清理预警、Casting 状态和固定落点。
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	// 返回 BT 与服务器距离校验共用的边界容差。
	virtual float GetAttackRangeTolerance() const override { return RangeTolerance; }
	// 返回 FireZone 施法期间播放的复制 Montage。
	virtual UAnimMontage* GetAttackMontage() const override { return AttackMontage; }
	// 返回施法 Montage 的蓝图可调播放倍率。
	virtual float GetAttackMontagePlayRate() const override { return MontagePlayRate; }
	// 使用预警持续时间作为从 Montage 开始计算的权威释放延迟。
	virtual float GetAttackReleaseDelay() const override { return TelegraphDuration; }
	// 返回 Montage 可选的起始 Section。
	virtual FName GetAttackMontageStartSection() const override { return MontageStartSection; }
	// FireZone 在 Commit 后锁定世界落点，释放时不再检查距离或视线。
	virtual bool ShouldRevalidateRangeAndLineOfSightAtRelease() const override { return false; }
	// 原目标在前摇中移动或死亡时仍兑现固定预警对应的火区。
	virtual bool ShouldRequireLivingTargetAtRelease() const override { return false; }
	// BT 和 Commit 同时要求目标可见且脚下存在有效地面。
	virtual bool HasAttackLineOfSight(AArenaEnemyCharacter* SourceEnemy, AActor* TargetActor) const override;
	// FireZone 必须配置 Montage、伤害 GE 和 Area Class 后才允许 Commit。
	virtual bool HasRequiredAttackConfiguration() const override;
	// 预警到期时移除 Casting 状态，并在固定位置生成服务器复制 Area。
	virtual void ExecuteAttack(
		AArenaEnemyCharacter* SourceEnemy,
		AActor* TargetActor,
		UAbilitySystemComponent* SourceASC,
		UAbilitySystemComponent* TargetASC) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Fire Zone")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Fire Zone")
	TSubclassOf<AArenaBossFireZoneArea> FireZoneAreaClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Fire Zone", meta = (ClampMin = "0.0"))
	float MinimumAttackRange = 600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Fire Zone", meta = (ClampMin = "0.0"))
	float AttackRange = 1200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Fire Zone", meta = (ClampMin = "0.0"))
	float RangeTolerance = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Fire Zone", meta = (ClampMin = "0.01"))
	float TelegraphDuration = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Fire Zone", meta = (ClampMin = "0.0"))
	float ZoneRadius = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Fire Zone", meta = (ClampMin = "0.0"))
	float DamageHalfHeight = 180.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Fire Zone", meta = (ClampMin = "0.01"))
	float ZoneDuration = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Fire Zone", meta = (ClampMin = "0.01"))
	float DamageTickInterval = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Fire Zone", meta = (ClampMin = "0.0"))
	float BaseDamage = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Fire Zone", meta = (ClampMin = "0.0"))
	float SkillMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Fire Zone|Targeting", meta = (ClampMin = "0.0"))
	float GroundTraceStartHeight = 80.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Fire Zone|Targeting", meta = (ClampMin = "1.0"))
	float GroundTraceDistance = 1200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Fire Zone|Animation")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Fire Zone|Animation", meta = (ClampMin = "0.01"))
	float MontagePlayRate = 1.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Fire Zone|Animation")
	FName MontageStartSection = NAME_None;

private:
	// 从目标胶囊中心向下追踪地面，并忽略 Boss 与目标自身碰撞。
	bool ResolveFireZoneLocation(
		const AArenaEnemyCharacter* SourceEnemy,
		const AActor* TargetActor,
		FVector& OutLocation) const;
	// 启动携带真实半径的固定世界位置 Telegraph Cue。
	void AddTelegraphCue(UAbilitySystemComponent* SourceASC, AActor* SourceActor);
	// 幂等移除 Telegraph Cue，供释放和所有取消路径复用。
	void RemoveTelegraphCue(UAbilitySystemComponent* SourceASC);
	// 前摇期间添加本地和复制 State.Casting，供 BT 与客户端调试观察。
	void AddCastingStateTag(UAbilitySystemComponent* SourceASC);
	// 释放或取消时对称移除本次 Ability 添加的 State.Casting。
	void RemoveCastingStateTag(UAbilitySystemComponent* SourceASC);

	FVector FireZoneLocation = FVector::ZeroVector;
	bool bHasFireZoneLocation = false;
	bool bTelegraphActive = false;
	bool bAppliedCastingStateTag = false;
};
