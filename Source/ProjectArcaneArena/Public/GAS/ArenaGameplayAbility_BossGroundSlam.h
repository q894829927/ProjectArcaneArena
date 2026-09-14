#pragma once

#include "CoreMinimal.h"
#include "GAS/ArenaGameplayAbility_EnemyAttackBase.h"
#include "ArenaGameplayAbility_BossGroundSlam.generated.h"

class UAnimMontage;
class UGameplayEffect;

UCLASS(Blueprintable)
class PROJECTARCANEARENA_API UArenaGameplayAbility_BossGroundSlam : public UArenaGameplayAbility_EnemyAttackBase
{
	GENERATED_BODY()

public:
	// 配置 Boss GroundSlam 的服务器策略、标签和第一阶段默认数值。
	UArenaGameplayAbility_BossGroundSlam();

	// 返回 AI 进入攻击分支时使用的距离，实际伤害使用独立范围。
	virtual float GetAttackRange() const override { return AttackRange; }

protected:
	// 在服务器提交攻击后锁定 Boss 脚下圆心并启动持续预警 Cue。
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	// 取消、死亡或正常结束时幂等移除预警，避免世界中残留持续表现。
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	virtual float GetAttackRangeTolerance() const override { return RangeTolerance; }
	virtual UAnimMontage* GetAttackMontage() const override { return AttackMontage; }
	virtual float GetAttackMontagePlayRate() const override { return MontagePlayRate; }
	virtual float GetAttackReleaseDelay() const override { return HitDelay; }
	virtual FName GetAttackMontageStartSection() const override { return MontageStartSection; }
	// GroundSlam 锁定固定圆心，释放时不再追踪目标距离或视线。
	virtual bool ShouldRevalidateRangeAndLineOfSightAtRelease() const override { return false; }
	// 要求动画和伤害 GE 均已配置后才允许提交冷却。
	virtual bool HasRequiredAttackConfiguration() const override;
	// 在固定圆心搜索所有合法玩家，并为每个目标应用独立物理伤害 Spec。
	virtual void ExecuteAttack(
		AArenaEnemyCharacter* SourceEnemy,
		AActor* TargetActor,
		UAbilitySystemComponent* SourceASC,
		UAbilitySystemComponent* TargetASC) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Ground Slam")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Ground Slam", meta = (ClampMin = "0.0"))
	float AttackRange = 280.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Ground Slam", meta = (ClampMin = "0.0"))
	float RangeTolerance = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Ground Slam", meta = (ClampMin = "0.0"))
	float DamageRadius = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Ground Slam", meta = (ClampMin = "0.0"))
	float BaseDamage = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Ground Slam", meta = (ClampMin = "0.0"))
	float SkillMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Ground Slam|Animation")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Ground Slam|Animation", meta = (ClampMin = "0.01"))
	float MontagePlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Ground Slam|Animation", meta = (ClampMin = "0.0"))
	float HitDelay = 1.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Ground Slam|Animation")
	FName MontageStartSection = NAME_None;

private:
	// 根据胶囊底部计算固定地面圆心，后续不再跟随 Boss 或目标移动。
	FVector ResolveGroundSlamLocation(const AArenaEnemyCharacter* SourceEnemy) const;
	// 启动携带真实半径的持续预警 Cue。
	void AddTelegraphCue(UAbilitySystemComponent* SourceASC, AActor* SourceActor);
	// 幂等移除持续预警 Cue，供命中和所有取消路径复用。
	void RemoveTelegraphCue(UAbilitySystemComponent* SourceASC);

	FVector GroundSlamLocation = FVector::ZeroVector;
	bool bHasGroundSlamLocation = false;
	bool bTelegraphActive = false;
};
