#pragma once

#include "CoreMinimal.h"
#include "GAS/ArenaGameplayAbility.h"
#include "ArenaGameplayAbility_EnemyMeleeAttack.generated.h"

class UGameplayEffect;
class UAnimMontage;
class UAbilitySystemComponent;
class AArenaEnemyCharacter;

UCLASS(Blueprintable)
class PROJECTARCANEARENA_API UArenaGameplayAbility_EnemyMeleeAttack : public UArenaGameplayAbility
{
	GENERATED_BODY()

public:
	UArenaGameplayAbility_EnemyMeleeAttack();

	float GetAttackRange() const { return AttackRange; }

protected:
	// 服务器重新校验 AI 目标、距离和视线后提交冷却并应用物理伤害。
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	// 结束时统一移除攻击标签并释放缓存目标，取消时 AbilityTask 会停止 Montage/Delay。
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

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

private:
	bool IsAttackTargetValid(AArenaEnemyCharacter* SourceEnemy, AActor* TargetActor, UAbilitySystemComponent*& OutTargetASC) const;
	void ApplyAttackStateTag();
	void RemoveAttackStateTag();
	void FinishCurrentAttack(bool bWasCancelled);

	UFUNCTION()
	void HandleHitDelayFinished();

	UFUNCTION()
	void HandleMontageCompleted();

	UFUNCTION()
	void HandleMontageInterrupted();

	TWeakObjectPtr<AArenaEnemyCharacter> ActiveSourceEnemy;
	TWeakObjectPtr<AActor> ActiveTargetActor;
	TWeakObjectPtr<UAbilitySystemComponent> ActiveSourceASC;
	bool bAppliedAttackStateTag = false;
	bool bProcessedHit = false;
};
