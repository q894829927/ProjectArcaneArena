#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "GAS/ArenaGameplayAbility.h"
#include "ArenaGameplayAbility_BasicAttack.generated.h"

class AGameplayAbilityTargetActor;
class UAbilitySystemComponent;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitTargetData;
class UAnimMontage;
class UGameplayEffect;

UCLASS(Blueprintable)
class PROJECTARCANEARENA_API UArenaGameplayAbility_BasicAttack : public UArenaGameplayAbility
{
	GENERATED_BODY()

public:
	UArenaGameplayAbility_BasicAttack();

protected:
	// 客户端采集瞄准点，服务端提交冷却并沿校验后的方向执行近战扫描。
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	// 保持 Ability 活跃至预测 Montage 结束，使服务器拒绝或取消能停止本地预测表现。
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	UFUNCTION()
	void OnTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData);

	UFUNCTION()
	void OnTargetDataCancelled(const FGameplayAbilityTargetDataHandle& TargetData);

	UFUNCTION()
	void HandleAttackMontageCompleted();

	UFUNCTION()
	void HandleAttackMontageInterrupted();

	// 绘制攻击范围，帮助区分输入未触发和攻击未命中。
	void DrawAttackRangeDebug(UWorld* World, const FVector& Start, const FVector& End, bool bHitTarget) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage")
	FGameplayTag DamageTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage", meta = (ClampMin = "0.0"))
	float BaseDamage = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage", meta = (ClampMin = "0.0"))
	float SkillMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Trace", meta = (ClampMin = "0.0"))
	float AttackRange = 275.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Trace", meta = (ClampMin = "0.0"))
	float AttackRadius = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Targeting")
	TSubclassOf<AGameplayAbilityTargetActor> TargetActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Animation")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Animation", meta = (ClampMin = "0.01"))
	float MontagePlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Animation")
	FName MontageStartSection = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Debug")
	bool bDrawDebugAttackRange = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Debug", meta = (ClampMin = "0.0"))
	float DebugAttackRangeDuration = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Debug")
	FLinearColor DebugAttackRangeHitColor = FLinearColor::Green;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Debug")
	FLinearColor DebugAttackRangeMissColor = FLinearColor::Red;

private:
	bool ExtractAimDirection(const FGameplayAbilityTargetDataHandle& TargetData, AActor* AvatarActor, FVector& OutAimDirection) const;
	// 播放预测普攻 Montage；伤害判定仍由服务器 Sweep 独立完成。
	void PlayAttackMontage();
	void ExecuteServerAttack(AActor* AvatarActor, UAbilitySystemComponent* SourceASC, const FVector& AimDirection);

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitTargetData> ActiveTargetDataTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> ActiveMontageTask;

	bool bConsumedTargetData = false;
	bool bServerAttackExecuted = false;
};
