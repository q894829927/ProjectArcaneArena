#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GAS/ArenaGameplayAbility_EnemyAttackBase.h"
#include "TimerManager.h"
#include "ArenaGameplayAbility_BossCharge.generated.h"

class AArenaPlayerCharacter;
class UAbilityTask_ApplyRootMotionConstantForce;
class UAbilityTask_PlayMontageAndWait;
class UAnimMontage;
class UCapsuleComponent;
class UGameplayEffect;
class UPrimitiveComponent;

UCLASS(Blueprintable)
class PROJECTARCANEARENA_API UArenaGameplayAbility_BossCharge : public UArenaGameplayAbility_EnemyAttackBase
{
	GENERATED_BODY()

public:
	// 配置服务器权威 Charge 的标签、冷却和第一版战斗数值。
	UArenaGameplayAbility_BossCharge();

	// 返回 BT Charge 分支允许进入的最大锁定距离。
	virtual float GetAttackRange() const override { return AttackRange; }
	// 返回 BT Charge 分支允许进入的最小锁定距离。
	virtual float GetMinimumAttackRange() const override { return MinimumAttackRange; }

protected:
	// 提交后锁定方向和终点，启动固定世界预警，预警结束后再创建可沿地面行走的 RootMotion。
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	// 幂等清理计时器、碰撞、Montage、RootMotion、Cue 和本次命中缓存。
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	virtual float GetAttackRangeTolerance() const override { return RangeTolerance; }
	virtual UAnimMontage* GetAttackMontage() const override { return ChargeMontage; }
	virtual float GetAttackMontagePlayRate() const override { return ChargeMontagePlayRate; }
	// Charge 必须具备 Montage、伤害 GE 和有效位移参数才允许消耗冷却。
	virtual bool HasRequiredAttackConfiguration() const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Charge")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Charge", meta = (ClampMin = "0.0"))
	float MinimumAttackRange = 350.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Charge", meta = (ClampMin = "0.0"))
	float AttackRange = 900.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Charge", meta = (ClampMin = "0.0"))
	float RangeTolerance = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Charge", meta = (ClampMin = "0.0"))
	float TargetOvershootDistance = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Charge", meta = (ClampMin = "1.0"))
	float MaximumChargeDistance = 1050.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Charge", meta = (ClampMin = "0.0"))
	float TelegraphDuration = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Charge", meta = (ClampMin = "1.0"))
	float ChargeSpeed = 1200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Charge", meta = (ClampMin = "1.0"))
	float HitRadius = 110.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Charge", meta = (ClampMin = "0.0"))
	float BaseDamage = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Charge", meta = (ClampMin = "0.0"))
	float SkillMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Charge|Animation")
	TObjectPtr<UAnimMontage> ChargeMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Charge|Animation", meta = (ClampMin = "0.01"))
	float ChargeMontagePlayRate = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Charge|Animation")
	FName MontageStartSection = NAME_None;

private:
	// 预警结束后确认目标仍存活，再启动 Montage、Active Cue、碰撞贯穿和 RootMotion。
	UFUNCTION()
	void BeginChargeMovement();
	// RootMotion 正常走完计划距离后结束技能，不额外触发 Impact。
	UFUNCTION()
	void HandleChargeMovementFinished();
	// Montage 被取消或打断时同步取消 Charge，防止动画与位移生命周期分离。
	UFUNCTION()
	void HandleChargeMontageInterrupted();
	// 角色胶囊撞到不可行走的非玩家阻挡物时触发一次 Impact；可行走斜坡不会误停 Charge。
	UFUNCTION()
	void HandleChargeBlockingHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse,
		const FHitResult& Hit);

	// 从上一采样位置到当前位置做球形 Sweep，并让每名存活玩家每次 Charge 最多结算一次。
	void SweepChargePath();
	// 对首次接触的玩家应用独立物理伤害 Spec，并触发 Charge Impact 表现。
	void ApplyChargeDamage(AArenaPlayerCharacter* PlayerCharacter, const FVector& ImpactLocation);
	// 添加携带锁定起点、方向和距离的固定世界预警 Cue。
	void AddTelegraphCue();
	// 幂等移除固定预警 Cue。
	void RemoveTelegraphCue();
	// 添加附着 Boss 的冲锋持续 Cue。
	void AddActiveCue();
	// 幂等移除冲锋持续 Cue。
	void RemoveActiveCue();
	// 在玩家接触点或墙体阻挡点执行一次瞬时 Impact Cue。
	void ExecuteImpactCue(const FVector& ImpactLocation, const FVector& ImpactNormal);
	// 临时让 Pawn 不阻挡 Boss，同时保留世界碰撞用于撞墙停止。
	void EnablePawnPassThrough();
	// 恢复 Charge 前的 Pawn 碰撞响应并解绑墙体命中回调。
	void RestorePawnCollision();
	// 结束残余速度并把移动控制交还 Behavior Tree。
	void StopChargeMovement();

	TWeakObjectPtr<AArenaEnemyCharacter> ChargeSourceEnemy;
	TWeakObjectPtr<AActor> LockedTargetActor;
	TWeakObjectPtr<UAbilitySystemComponent> ChargeSourceASC;
	TSet<TWeakObjectPtr<AArenaPlayerCharacter>> HitPlayers;
	FVector LockedStartLocation = FVector::ZeroVector;
	FVector LockedDirection = FVector::ZeroVector;
	FVector LockedEndLocation = FVector::ZeroVector;
	FVector PreviousSweepLocation = FVector::ZeroVector;
	float LockedChargeDistance = 0.0f;
	FTimerHandle TelegraphTimerHandle;
	FTimerHandle SweepTimerHandle;
	TEnumAsByte<ECollisionResponse> PreviousPawnCollisionResponse = ECR_Block;
	bool bPawnCollisionChanged = false;
	bool bTelegraphCueActive = false;
	bool bActiveCueActive = false;
	bool bChargeMovementActive = false;
	bool bWallImpactExecuted = false;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_ApplyRootMotionConstantForce> ChargeMovementTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> ChargeMontageTask;
};
