#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "GAS/ArenaGameplayAbility.h"
#include "TimerManager.h"
#include "ArenaGameplayAbility_Dash.generated.h"

class ACharacter;
class AGameplayAbilityTargetActor;
class UAbilitySystemComponent;
class UAbilityTask_WaitTargetData;
class UAnimMontage;

UCLASS(Blueprintable)
class PROJECTARCANEARENA_API UArenaGameplayAbility_Dash : public UArenaGameplayAbility
{
	GENERATED_BODY()

public:
	UArenaGameplayAbility_Dash();

	// 使用已经通过 TargetData 校验的同一水平单位方向启动客户端预测和服务器权威冲刺。
	void StartDashWithDirection(const FVector& DashDirection);

protected:
	// 启动客户端方向采集；服务器等待相同 TargetData 后再提交并执行权威冲刺。
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	// 统一清理 TargetData、RootMotion、Montage 关联状态和预测/复制标签。
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	UFUNCTION()
	void OnDashTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData);

	UFUNCTION()
	void OnDashTargetDataCancelled(const FGameplayAbilityTargetDataHandle& TargetData);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Dash", meta = (ClampMin = "0.0"))
	float DashDistance = 500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Dash", meta = (ClampMin = "0.01"))
	float DashDuration = 0.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Dash|Targeting")
	TSubclassOf<AGameplayAbilityTargetActor> DashDirectionTargetActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Dash|Animation")
	TObjectPtr<UAnimMontage> DashMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Dash|Animation", meta = (ClampMin = "0.01"))
	float DashMontagePlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Dash|Animation")
	FName DashMontageStartSection = NAME_None;

private:
	bool ExtractAndValidateDashDirection(const FGameplayAbilityTargetDataHandle& TargetData, FVector& OutDirection) const;
	void PlayDashMontage();
	void ApplyDashStateTags(UAbilitySystemComponent* ASC);
	void RemoveDashStateTags();
	void FinishDash();
	void StopDashMovement(ACharacter* Character) const;

	FTimerHandle DashTimerHandle;
	TWeakObjectPtr<ACharacter> ActiveDashCharacter;
	TWeakObjectPtr<UAbilitySystemComponent> ActiveDashASC;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitTargetData> ActiveTargetDataTask;

	bool bAppliedDashStateTags = false;
	bool bAddedDashGameplayCue = false;
	bool bConsumedTargetData = false;
};
