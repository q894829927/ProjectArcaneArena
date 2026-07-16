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

	// 按 PlayerState 中的 Dash 冷却升级缩放预测端与服务器使用的同一 Cooldown Spec。
	virtual void ApplyCooldown(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

	// 接收本地或服务器 TargetData，并在校验和 Commit 成功后启动同方向冲刺。
	UFUNCTION()
	void OnDashTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData);

	// 目标采集取消时结束本次预测激活，不产生完成事件。
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Dash|Upgrade", meta = (ClampMin = "0.01"))
	float MinimumCooldownDuration = 0.25f;

private:
	// 只接受一条有限、近水平的单位方向，服务端会对客户端提交数据执行相同校验。
	bool ExtractAndValidateDashDirection(const FGameplayAbilityTargetDataHandle& TargetData, FVector& OutDirection) const;
	// 播放预测 Montage；权威位移与事件不依赖动画通知。
	void PlayDashMontage();
	// 添加预测状态，并由服务器同步权威状态和持续 GameplayCue。
	void ApplyDashStateTags(UAbilitySystemComponent* ASC);
	// 成对移除本次 Dash 添加的状态和持续 GameplayCue。
	void RemoveDashStateTags();
	// 预测端只停止本地位移；服务器正常计时结束时额外发送唯一完成事件。
	void FinishDash();
	// 只在服务器为正常完成的 Dash 发送一次携带实际起终点的 OnDashEnd 事件。
	void SendDashEndEvent();
	// 从 PlayerState 数据聚合当前 Dash 冷却缩减比例。
	float GetDashCooldownReduction(const FGameplayAbilityActorInfo* ActorInfo) const;
	// 清除冲刺结束残留速度，避免 RootMotion 后继续滑动。
	void StopDashMovement(ACharacter* Character) const;

	FTimerHandle DashTimerHandle;
	TWeakObjectPtr<ACharacter> ActiveDashCharacter;
	TWeakObjectPtr<UAbilitySystemComponent> ActiveDashASC;
	FVector AuthorityDashStartLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitTargetData> ActiveTargetDataTask;

	bool bAppliedDashStateTags = false;
	bool bAddedDashGameplayCue = false;
	bool bConsumedTargetData = false;
	bool bSentDashEndEvent = false;
};
