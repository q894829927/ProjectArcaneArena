#pragma once

#include "CoreMinimal.h"
#include "GAS/ArenaGameplayAbility_EnemyAttackBase.h"
#include "ArenaGameplayAbility_EnemyRangedAttack.generated.h"

class AArenaEnemyProjectile;
class UAnimMontage;
class UGameplayEffect;

UCLASS(Blueprintable)
class PROJECTARCANEARENA_API UArenaGameplayAbility_EnemyRangedAttack : public UArenaGameplayAbility_EnemyAttackBase
{
	GENERATED_BODY()

public:
	// 配置远程攻击 Tag、默认冷却 GE 与服务器执行参数。
	UArenaGameplayAbility_EnemyRangedAttack();

	// 返回远程 AI 进入站定施法状态的最大距离。
	virtual float GetAttackRange() const override { return AttackRange; }

protected:
	// 返回激活校验使用的额外距离容差；远程释放阶段不再重复检查距离。
	virtual float GetAttackRangeTolerance() const override { return RangeTolerance; }
	// 返回远程施法 Montage。
	virtual UAnimMontage* GetAttackMontage() const override { return AttackMontage; }
	// 返回远程 Montage 播放倍率。
	virtual float GetAttackMontagePlayRate() const override { return MontagePlayRate; }
	// 保留固定延迟配置兼容；当前 BlendOut 同步模式不使用该数值。
	virtual float GetAttackReleaseDelay() const override { return ReleaseDelay; }
	// 在施法 Montage 进入 BlendOut 的动作结束点立即发射 Projectile。
	virtual bool ShouldReleaseOnMontageBlendOut() const override { return true; }
	// 返回远程 Montage 起始段。
	virtual FName GetAttackMontageStartSection() const override { return MontageStartSection; }
	// 要求 Montage、伤害 GE 和 Projectile Class 均已配置。
	virtual bool HasRequiredAttackConfiguration() const override;
	// 沿实际发射路径执行 Projectile 尺寸 Sweep，忽略玩家并把场景阻挡物视为施法遮挡。
	virtual bool HasAttackLineOfSight(AArenaEnemyCharacter* SourceEnemy, AActor* TargetActor) const override;
	// 远程攻击在激活时完成权威校验，前摇开始后即使目标离开射程也保证发射。
	virtual bool ShouldRevalidateRangeAndLineOfSightAtRelease() const override { return false; }
	// 按释放瞬间目标位置计算一次方向并仅在服务器生成一个复制 Projectile。
	virtual void ExecuteAttack(
		AArenaEnemyCharacter* SourceEnemy,
		AActor* TargetActor,
		UAbilitySystemComponent* SourceASC,
		UAbilitySystemComponent* TargetASC) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Enemy Attack")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Enemy Attack")
	TSubclassOf<AArenaEnemyProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Enemy Attack", meta = (ClampMin = "0.0"))
	float AttackRange = 950.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Enemy Attack", meta = (ClampMin = "0.0"))
	float RangeTolerance = 50.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Enemy Attack", meta = (ClampMin = "0.0"))
	float BaseDamage = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Enemy Attack", meta = (ClampMin = "0.0"))
	float SkillMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Enemy Attack|Projectile")
	FVector ProjectileSpawnOffset = FVector(70.0f, 0.0f, 50.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Enemy Attack|Projectile")
	FVector TargetAimOffset = FVector(0.0f, 0.0f, 50.0f);

	// 使用接近 Projectile 碰撞体的 Sweep 半径判断发射路径是否真正畅通。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Enemy Attack|Projectile", meta = (ClampMin = "1.0"))
	float ProjectilePathTraceRadius = 16.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Enemy Attack|Animation")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Enemy Attack|Animation", meta = (ClampMin = "0.01"))
	float MontagePlayRate = 1.5f;

	// BlendOut 同步模式下保持为零，不在动画结束后追加固定延迟。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Enemy Attack|Animation", meta = (ClampMin = "0.0"))
	float ReleaseDelay = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Enemy Attack|Animation")
	FName MontageStartSection = NAME_None;
};
