#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "ArenaAutoAttackComponent.generated.h"

class AActor;

// P2/G-A 单武器自动攻击调度器；仅服务器选择目标并向 Data Projectile Pool 发射，不负责命中与伤害。
UCLASS(ClassGroup = (Arena), meta = (BlueprintSpawnableComponent))
class PROJECTARCANEARENA_API UArenaAutoAttackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UArenaAutoAttackComponent();

	// 运行时启停原型自动武器；关闭时立即取消后续调度，但不回收已经发射的 Projectile。
	UFUNCTION(BlueprintCallable, Category = "Arena|Auto Attack")
	void SetAutoAttackEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Arena|Auto Attack")
	bool IsAutoAttackEnabled() const { return bAutoAttackEnabled; }

	// 返回服务器本组件成功提交到 Data Pool 的累计攻击轮次，便于 P2 PIE 验证。
	UFUNCTION(BlueprintPure, Category = "Arena|Auto Attack")
	int32 GetTotalShotsFired() const { return TotalShotsFired; }

	// 返回最近一次成功发射时锁定的目标；该引用仅用于调试观察，不参与网络权威。
	UFUNCTION(BlueprintPure, Category = "Arena|Auto Attack")
	AActor* GetLastFiredTarget() const { return LastFiredTarget.Get(); }

	// 返回最近一次成功写入 Data Pool 的 AttackInstanceID，供 PIE 验证攻击轮次递增。
	UFUNCTION(BlueprintPure, Category = "Arena|Auto Attack")
	int32 GetLastAttackInstanceID() const { return LastAttackInstanceID; }

protected:
	// 仅在 Authority Avatar 上启动定时调度；客户端组件不创建玩法 Projectile。
	virtual void BeginPlay() override;

	// 销毁或换 Avatar 时清理 Timer 和弱引用，防止旧角色继续调度发射。
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// 执行一次服务器自动攻击评估；Combat/Dead/Stunned/无目标时只安排低频重试。
	void EvaluateAutoAttack();

	// 检查阶段与 GAS 状态是否允许生成下一轮权威 Projectile。
	bool CanAutoFire() const;

	// P2 暂时低频遍历最近存活敌人；P3 将替换为共享 Target Grid / Spatial Hash 查询。
	AActor* FindNearestLivingEnemy() const;

	// 将一发原型普通弹写入 Data Projectile Pool，并携带 Owner 局部递增的 AttackInstanceID。
	bool FireAtTarget(AActor* TargetActor);

	// 使用 TimerManager 安排下一次攻击或无目标重试，避免每帧 Tick。
	void ScheduleNextEvaluation(float DelaySeconds);

	// 生成正整数 AttackInstanceID；回绕后从 1 重新开始，0 保留为“未设置”。
	int32 AllocateAttackInstanceID();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Auto Attack", meta = (AllowPrivateAccess = "true"))
	bool bAutoAttackEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Auto Attack", meta = (AllowPrivateAccess = "true", ClampMin = "0.05"))
	float FireInterval = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Auto Attack", meta = (AllowPrivateAccess = "true", ClampMin = "0.05"))
	float RetryInterval = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Auto Attack", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float TargetRange = 1400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Auto Attack|Projectile", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float ProjectileSpeed = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Auto Attack|Projectile", meta = (AllowPrivateAccess = "true", ClampMin = "0.05"))
	float ProjectileLifetime = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Auto Attack|Projectile", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float ProjectileRadius = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Auto Attack|Projectile", meta = (AllowPrivateAccess = "true"))
	float ProjectileSpawnHeight = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Auto Attack|Projectile", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float ProjectileForwardOffset = 60.0f;

	// P2 单武器原型固定为 0；P4 多武器阶段由装备实例分配独立 RuntimeID。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Auto Attack", meta = (AllowPrivateAccess = "true"))
	int32 WeaponRuntimeID = 0;

	// 调试时输出成功发射的 AttackInstanceID/Handle/Target；默认关闭，避免正常战斗刷日志。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Auto Attack|Debug", meta = (AllowPrivateAccess = "true"))
	bool bLogSuccessfulShots = false;

	FTimerHandle EvaluationTimerHandle;
	TWeakObjectPtr<AActor> LastFiredTarget;
	int32 NextAttackInstanceID = 1;
	int32 LastAttackInstanceID = 0;
	int32 TotalShotsFired = 0;
};
