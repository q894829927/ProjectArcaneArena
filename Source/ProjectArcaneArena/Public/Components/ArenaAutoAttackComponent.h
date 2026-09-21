#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "ArenaAutoAttackComponent.generated.h"

class AActor;
class UGameplayEffect;
class UArenaWeaponDataAsset;
class UArenaWeaponLoadoutComponent;

// P4 多武器自动攻击调度器；Authority 用单个 Timer 驱动多个独立 WeaponRuntime，并向 Data Projectile Pool 发射。
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

	// 按当前武器 TargetRange 查找最近存活敌人；多 Runtime 可以拥有不同索敌半径。
	AActor* FindNearestLivingEnemy(float InTargetRange) const;

	// 将一轮单发普通弹写入 Data Projectile Pool；WeaponDefinition 为空时使用 P3 Inline Config 回退。
	bool FireAtTarget(
		AActor* TargetActor,
		int32 SlotIndex,
		int32 ResolvedWeaponRuntimeID,
		const UArenaWeaponDataAsset* WeaponDefinition);

	// PlayerState 可晚于 Character BeginPlay 关联；找到 Loadout 后只执行一次默认武器播种，之后卸装不会被自动补回。
	void TrySeedDefaultWeaponRuntimes();

	// 获取 PlayerState 上的武器装备 Model；Avatar 更换后仍可读取同一装备状态。
	UArenaWeaponLoadoutComponent* GetWeaponLoadoutComponent() const;

	// 使用 TimerManager 安排下一次攻击或无目标重试，避免每帧 Tick。
	void ScheduleNextEvaluation(float DelaySeconds);

	// 优先从 WeaponRuntime 分配独立攻击轮次；没有 Runtime 时回退到 P2 单武器局部计数器。
	int32 AllocateAttackInstanceIDForRuntime(int32 ResolvedWeaponRuntimeID);

	// P2 兼容回退计数器；只有尚未配置 WeaponDataAsset/WeaponRuntime 时使用。
	int32 AllocateAttackInstanceID();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Auto Attack", meta = (AllowPrivateAccess = "true"))
	bool bAutoAttackEnabled = true;

	// 默认武器数组索引直接对应 Loadout SlotIndex：[0]→Slot 0、[1]→Slot 1；超过 MaxWeaponSlots 的元素忽略。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Auto Attack|Weapon", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UArenaWeaponDataAsset>> DefaultWeaponDefinitions;

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

	// P3 使用现有 GE_Damage；需在 BP_ArenaPlayerCharacter 的 AutoAttackComponent 上配置该资产。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Auto Attack|Damage", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Auto Attack|Damage", meta = (AllowPrivateAccess = "true"))
	FGameplayTag DamageTypeTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Auto Attack|Damage", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float BaseDamage = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Auto Attack|Damage", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float SkillMultiplier = 1.0f;

	// 旧 Inline Config 的兼容 RuntimeID；正常 P4 WeaponRuntime 使用 PlayerState Loadout 分配的正数 ID。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Auto Attack", meta = (AllowPrivateAccess = "true"))
	int32 WeaponRuntimeID = 0;

	// 调试时输出成功发射的 AttackInstanceID/Handle/Target；默认关闭，避免正常战斗刷日志。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Auto Attack|Debug", meta = (AllowPrivateAccess = "true"))
	bool bLogSuccessfulShots = false;

	FTimerHandle EvaluationTimerHandle;
	TWeakObjectPtr<AActor> LastFiredTarget;

	// 单个 Timer 调度多个武器；Key 为稳定 WeaponRuntimeID，Value 为 World TimeSeconds 下次可开火时间。
	TMap<int32, double> NextFireTimeByRuntime;
	bool bDefaultWeaponSeedAttempted = false;

	int32 NextAttackInstanceID = 1;
	int32 LastAttackInstanceID = 0;
	int32 TotalShotsFired = 0;
};
