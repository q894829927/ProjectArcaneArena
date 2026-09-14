#pragma once

#include "CoreMinimal.h"
#include "GAS/ArenaGameplayAbility_EnemyAttackBase.h"
#include "ArenaGameplayAbility_EnemyMeleeAttack.generated.h"

class UGameplayEffect;
class UAnimMontage;

UCLASS(Blueprintable)
class PROJECTARCANEARENA_API UArenaGameplayAbility_EnemyMeleeAttack : public UArenaGameplayAbility_EnemyAttackBase
{
	GENERATED_BODY()

public:
	// 配置近战专属 Ability/Cooldown Tag，并保留现有蓝图可调字段。
	UArenaGameplayAbility_EnemyMeleeAttack();

	// 返回近战攻击距离供通用 AI 选择 Chase 或 Attack。
	virtual float GetAttackRange() const override { return AttackRange; }

protected:
	// 返回近战命中允许的距离容差。
	virtual float GetAttackRangeTolerance() const override { return RangeTolerance; }
	// 返回近战攻击 Montage。
	virtual UAnimMontage* GetAttackMontage() const override { return AttackMontage; }
	// 返回近战 Montage 播放倍率。
	virtual float GetAttackMontagePlayRate() const override { return MontagePlayRate; }
	// 返回服务器近战命中延迟。
	virtual float GetAttackReleaseDelay() const override { return HitDelay; }
	// 返回近战 Montage 起始段。
	virtual FName GetAttackMontageStartSection() const override { return MontageStartSection; }
	// 要求近战同时配置 Montage 与伤害 GameplayEffect。
	virtual bool HasRequiredAttackConfiguration() const override;
	// 近战只让墙体等场景阻挡攻击路径，同阵营敌人不会把后排永久锁在不可攻击状态。
	virtual bool HasAttackLineOfSight(AArenaEnemyCharacter* SourceEnemy, AActor* TargetActor) const override;
	// 在权威命中窗口构造一次物理伤害 Spec 并应用给锁定目标。
	virtual void ExecuteAttack(
		AArenaEnemyCharacter* SourceEnemy,
		AActor* TargetActor,
		UAbilitySystemComponent* SourceASC,
		UAbilitySystemComponent* TargetASC) override;
	// 复用既有敌人近战激活 GameplayCue。
	virtual FGameplayTag GetAttackActivationCueTag() const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Enemy Attack")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Enemy Attack", meta = (ClampMin = "0.0"))
	float AttackRange = 170.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Enemy Attack", meta = (ClampMin = "0.0"))
	float RangeTolerance = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Enemy Attack", meta = (ClampMin = "0.0"))
	float BaseDamage = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Enemy Attack", meta = (ClampMin = "0.0"))
	float SkillMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Enemy Attack|Animation")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Enemy Attack|Animation", meta = (ClampMin = "0.01"))
	float MontagePlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Enemy Attack|Animation", meta = (ClampMin = "0.0"))
	float HitDelay = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Enemy Attack|Animation")
	FName MontageStartSection = NAME_None;
};
