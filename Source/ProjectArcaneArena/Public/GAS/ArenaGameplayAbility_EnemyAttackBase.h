#pragma once

#include "CoreMinimal.h"
#include "GAS/ArenaGameplayAbility.h"
#include "ArenaGameplayAbility_EnemyAttackBase.generated.h"

class AArenaEnemyCharacter;
class UAbilitySystemComponent;
class UAnimMontage;

UCLASS(Abstract, Blueprintable)
class PROJECTARCANEARENA_API UArenaGameplayAbility_EnemyAttackBase : public UArenaGameplayAbility
{
	GENERATED_BODY()

public:
	// 初始化所有敌人主攻击共用的服务器执行策略与状态阻断规则。
	UArenaGameplayAbility_EnemyAttackBase();

	// 返回 AI 决策和激活校验使用的主攻击距离，派生类决定释放时是否再次复验。
	virtual float GetAttackRange() const { return 0.0f; }

	// 返回攻击分支允许进入的最小二维距离，默认零以保持现有攻击行为。
	virtual float GetMinimumAttackRange() const { return 0.0f; }

	// 让 AI 与 Ability 共用同一套攻击路径检查，避免视线判断和实际释放条件分叉。
	bool HasAttackPathForAI(AArenaEnemyCharacter* SourceEnemy, AActor* TargetActor) const;

protected:
	// 复用服务器校验、Commit、目标缓存和 State.Attacking 初始化，供自定义攻击生命周期使用。
	bool BeginServerAttack(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		AArenaEnemyCharacter*& OutSourceEnemy,
		AActor*& OutTargetActor,
		UAbilitySystemComponent*& OutSourceASC);

	// 锁定当前 CombatTarget，提交冷却，并启动复制 Montage 与固定时间或动作结束同步的服务器释放。
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	// 统一移除攻击状态并释放本次攻击缓存，取消时由 GAS 同步终止仍在运行的任务。
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	// 返回激活及可选释放复验使用的额外距离容差。
	virtual float GetAttackRangeTolerance() const { return 0.0f; }
	// 返回负责攻击表现复制的 Montage。
	virtual UAnimMontage* GetAttackMontage() const { return nullptr; }
	// 返回 Montage 播放倍率并由基类执行最小值保护。
	virtual float GetAttackMontagePlayRate() const { return 1.0f; }
	// 返回非 BlendOut 同步攻击从 Montage 起播时刻计算的释放延迟；零表示同一帧释放。
	virtual float GetAttackReleaseDelay() const { return 0.0f; }
	// 决定是否在 Montage 进入 BlendOut 的动作结束点释放，而不是使用起播后的固定延迟。
	virtual bool ShouldReleaseOnMontageBlendOut() const { return false; }
	// 返回可选的 Montage 起始段。
	virtual FName GetAttackMontageStartSection() const { return NAME_None; }
	// 检查派生攻击的伤害、Projectile 与表现配置是否完整。
	virtual bool HasRequiredAttackConfiguration() const;
	// 检查当前攻击类型的权威视线或弹道路径，默认沿用 AIController 的目标视线判断。
	virtual bool HasAttackLineOfSight(AArenaEnemyCharacter* SourceEnemy, AActor* TargetActor) const;
	// 决定释放时是否再次检查距离和视线；近战默认复验，已完成前摇的远程攻击可选择保证发射。
	virtual bool ShouldRevalidateRangeAndLineOfSightAtRelease() const { return true; }
	// 在服务器释放时执行派生攻击，目标已通过存活校验和该攻击类型要求的可选空间复验。
	virtual void ExecuteAttack(
		AArenaEnemyCharacter* SourceEnemy,
		AActor* TargetActor,
		UAbilitySystemComponent* SourceASC,
		UAbilitySystemComponent* TargetASC);
	// 返回可选的一次性激活 Cue；无效 Tag 表示只使用 Montage 或 Projectile 表现。
	virtual FGameplayTag GetAttackActivationCueTag() const { return FGameplayTag(); }

	// 仅在 Ability 仍活跃时结束当前攻击，供自定义任务、碰撞和计时器共享。
	void FinishCurrentAttack(bool bWasCancelled);

private:
	// 复用服务器目标校验，并按派生类最小/最大距离决定是否在释放阶段再次检查空间条件。
	bool IsAttackTargetValid(
		AArenaEnemyCharacter* SourceEnemy,
		AActor* TargetActor,
		UAbilitySystemComponent*& OutTargetASC,
		bool bCheckRangeAndLineOfSight) const;
	// 为服务器与观察客户端同步攻击中的移动冻结状态。
	void ApplyAttackStateTag();
	// 移除本次 Ability 添加的本地及复制 loose Tag。
	void RemoveAttackStateTag();
	// 服务器释放窗口只消费一次，并按攻击类型完成存活及可选空间复验。
	UFUNCTION()
	void HandleReleaseDelayFinished();

	// Montage 正常结束后等待释放窗口完成，再结束攻击状态。
	UFUNCTION()
	void HandleMontageCompleted();

	// Montage 开始 BlendOut 时为需要动作结束同步的攻击执行权威释放。
	UFUNCTION()
	void HandleMontageBlendOut();

	// Montage 被取消或打断时立即取消 Ability，并阻止迟到释放。
	UFUNCTION()
	void HandleMontageInterrupted();

	TWeakObjectPtr<AArenaEnemyCharacter> ActiveSourceEnemy;
	TWeakObjectPtr<AActor> ActiveTargetActor;
	TWeakObjectPtr<UAbilitySystemComponent> ActiveSourceASC;
	bool bAppliedAttackStateTag = false;
	bool bProcessedRelease = false;
	bool bMontageCompleted = false;
};
