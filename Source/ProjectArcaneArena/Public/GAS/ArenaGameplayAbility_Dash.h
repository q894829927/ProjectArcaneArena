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
class UCapsuleComponent;

UCLASS(Blueprintable)
class PROJECTARCANEARENA_API UArenaGameplayAbility_Dash : public UArenaGameplayAbility
{
	GENERATED_BODY()

public:
	UArenaGameplayAbility_Dash();

	// 使用已校验方向预计算安全终点，再启动允许穿过 Pawn 的两端连续冲刺。
	void StartDashWithDirection(const FVector& DashDirection);

protected:
	// 启动客户端方向采集；服务器等待相同 TargetData 后再提交并执行权威冲刺。
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	// 统一清理 TargetData、RootMotion、碰撞响应、Montage 关联状态和预测/复制标签。
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Dash|Collision", meta = (ClampMin = "0.0"))
	float EndpointPawnPassThroughSearchDistance = 200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Dash|Collision", meta = (ClampMin = "0.0"))
	float ObstacleClearance = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Dash|Defense", meta = (ClampMin = "0.0"))
	float InvincibilityDuration = 0.15f;

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
	// 添加预测冲刺状态，并由服务器同步权威状态和持续 GameplayCue。
	void ApplyDashStateTags(UAbilitySystemComponent* ASC);
	// 成对移除本次 Dash 添加的冲刺状态和持续 GameplayCue，并回收尚未结束的无敌帧。
	void RemoveDashStateTags();
	// 添加独立的预测/权威无敌标签，并在配置窗口早于冲刺结束时启动移除计时器。
	void ApplyDashInvincibility(UAbilitySystemComponent* ASC);
	// 幂等移除本次 Dash 添加的无敌标签和计时器，供自然到期、取消与预测拒绝共用。
	void RemoveDashInvincibility();
	// 预测端只停止本地位移；服务器正常计时结束时额外发送唯一完成事件。
	void FinishDash();
	// 只在服务器为正常完成的 Dash 发送一次携带实际起终点的 OnDashEnd 事件。
	void SendDashEndEvent();
	// 从 PlayerState 数据聚合当前 Dash 冷却缩减比例。
	float GetDashCooldownReduction(const FGameplayAbilityActorInfo* ActorInfo) const;
	// 清除冲刺结束残留速度，避免 RootMotion 后继续滑动。
	void StopDashMovement(ACharacter* Character) const;
	// 在启动 RootMotion 前解析墙体截断和 Pawn 安全终点，返回本次实际连续移动距离。
	bool ResolveDashTravelDistance(
		const ACharacter* Character,
		const FVector& DashDirection,
		float& OutTravelDistance) const;
	// 使用忽略全部 Pawn 的胶囊扫描取得世界障碍物允许的最远冲刺距离。
	float FindWorldLimitedDashDistance(
		const ACharacter* Character,
		const UCapsuleComponent* CapsuleComponent,
		const FVector& DashDirection,
		float ProbeDistance) const;
	// 在理想终点附近先向前再向后搜索可恢复 Pawn 阻挡的安全位置。
	bool FindSafeDashEndpointDistance(
		const ACharacter* Character,
		const UCapsuleComponent* CapsuleComponent,
		const FVector& DashDirection,
		float PreferredDistance,
		float MaximumDistance,
		float& OutSafeDistance) const;
	// 在预测端和服务器临时把角色胶囊对 Pawn 的响应改为重叠，世界障碍物仍保持原碰撞。
	void EnablePawnPassThrough(ACharacter* Character);
	// 仅为动态碰撞变化和预测误差兜底，异常终点重叠时沿实际路径寻找最近安全位置。
	void ResolveDashEndOverlap();
	// 检查完整角色胶囊在候选位置恢复 Pawn 阻挡后是否会发生阻挡重叠。
	bool IsDashCapsuleBlockedAt(
		const ACharacter* Character,
		const UCapsuleComponent* CapsuleComponent,
		const FVector& CandidateLocation) const;
	// 在异常重叠保险检查后恢复冲刺前保存的 Pawn 碰撞响应。
	void RestorePawnCollision();

	FTimerHandle DashTimerHandle;
	FTimerHandle InvincibilityTimerHandle;
	TWeakObjectPtr<ACharacter> ActiveDashCharacter;
	TWeakObjectPtr<UAbilitySystemComponent> ActiveDashASC;
	FVector AuthorityDashStartLocation = FVector::ZeroVector;
	FVector DashCollisionStartLocation = FVector::ZeroVector;
	float ActiveDashDuration = 0.0f;
	TEnumAsByte<ECollisionResponse> PreviousPawnCollisionResponse = ECR_Block;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitTargetData> ActiveTargetDataTask;

	bool bAppliedDashStateTags = false;
	bool bAppliedDashInvincibilityTag = false;
	bool bAddedDashGameplayCue = false;
	bool bConsumedTargetData = false;
	bool bSentDashEndEvent = false;
	bool bPawnCollisionChanged = false;
};
