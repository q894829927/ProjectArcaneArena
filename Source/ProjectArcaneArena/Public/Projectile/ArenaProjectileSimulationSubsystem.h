#pragma once

#include "CoreMinimal.h"
#include "Projectile/ArenaProjectileTypes.h"
#include "Projectile/ArenaProjectileSpatialGrid.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArenaProjectileSimulationSubsystem.generated.h"

// World 级高密度 Data Projectile 数据池与集中模拟器；P3 已加入 Spatial Hash、Swept Collision 与 HitCommand GAS 结算。
UCLASS()
class PROJECTARCANEARENA_API UArenaProjectileSimulationSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// 根据 CVar 初始化预分配容量，并启用 WorldSubsystem Tick。
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// 世界释放前使全部 Handle 失效并清空数据池。
	virtual void Deinitialize() override;

	// 批量推进所有 Active Projectile 的位置与寿命，并在服务器通过 Spatial Hash 做 Swept Collision。
	virtual void Tick(float DeltaTime) override;

	// 为 TickableWorldSubsystem 提供独立性能统计 ID，避免运行时落入基类 PURE_VIRTUAL。
	virtual TStatId GetStatId() const override;

	// 从 Free List 获取一个槽位并写入本次发射快照；失败时增加 OverflowCount。
	bool SpawnProjectile(const FArenaProjectileSpawnParams& Params, FArenaProjectileHandle& OutHandle);

	// 仅当 Slot 与 Generation 同时匹配时释放 Projectile，旧代次句柄不会误删新 Projectile。
	bool ReleaseProjectile(const FArenaProjectileHandle& Handle);

	// 校验指定 Handle 是否仍指向当前 Active Projectile。
	bool IsProjectileAlive(const FArenaProjectileHandle& Handle) const;

	// 为调试或后续表现层读取当前位置；无效 Handle 返回 false。
	bool GetProjectilePosition(const FArenaProjectileHandle& Handle, FVector& OutPosition) const;

	// 使所有 Active Projectile 失效并归还槽位，通常用于压力测试切档或世界收尾。
	void ResetAllProjectiles();

	// 清零累计统计但不改变当前 Active Projectile。
	void ResetStatistics();

	// 返回当前容量、活跃量、峰值和溢出次数的只读快照。
	FArenaProjectileSimulationStats GetStats() const;

	// 返回当前 Active Projectile 数量，供自动武器和压力测试做轻量查询。
	int32 GetActiveProjectileCount() const
	{
		return ActiveSlots.Num();
	}

	// 敌人 BeginPlay 在服务器注册为 Data Projectile 可命中目标，避免每颗 Projectile 遍历全世界 Actor。
	void RegisterCollisionTarget(class AArenaEnemyCharacter* Target);

	// 敌人 EndPlay 在服务器注销目标；SpatialGrid 也会清理失效 WeakObjectPtr。
	void UnregisterCollisionTarget(class AArenaEnemyCharacter* Target);

protected:
	// 仅为 Game/PIE World 创建模拟器，编辑器预览世界不参与弹幕 Tick。
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
	// Free List 为空时按 GrowChunkSize 扩容，但不超过 MaxCapacity。
	bool EnsureFreeSlot();

	// 扩展所有 SoA 数组并把新增 Slot 注册到 Free List。
	void GrowStorage(int32 NewCapacity);

	// 通过 ActiveSlots 的稠密索引回收槽位，SwapRemove 后同步反向索引。
	void ReleaseSlotAtActiveIndex(int32 ActiveIndex);

	// 校验 Slot、Generation 和 ActiveListPosition，统一隔离迟到回调。
	bool IsSlotGenerationValid(int32 Slot, int32 Generation) const;

	// 对单颗 Projectile 的 PreviousPosition→Position 做 Swept Narrow Phase，并返回沿线最早命中的目标。
	bool FindFirstProjectileHit(int32 Slot, FArenaProjectileHitCommand& OutCommand) const;

	// 模拟阶段结束后统一在 GameThread 消费命中命令，所有属性修改继续通过 GE_Damage / ExecCalc。
	void ApplyPendingHitCommands();

	TArray<FVector> Positions;
	TArray<FVector> PreviousPositions;
	TArray<FVector> Velocities;
	TArray<float> Radii;
	TArray<float> RemainingLife;
	TArray<int32> PierceRemaining;
	TArray<int32> AttackInstanceIDs;
	TArray<int32> WeaponRuntimeIDs;
	TArray<TWeakObjectPtr<AActor>> SourceActors;
	TArray<TSubclassOf<UGameplayEffect>> DamageEffectClasses;
	TArray<FGameplayTag> DamageTypeTags;
	TArray<float> BaseDamages;
	TArray<float> SkillMultipliers;
	TArray<int32> Generations;

	TArray<int32> ActiveSlots;
	TArray<int32> ActiveListPositions;
	TArray<int32> FreeSlots;

	FArenaProjectileSpatialGrid SpatialGrid;
	TArray<FArenaProjectileHitCommand> PendingHitCommands;
	mutable TArray<class AArenaEnemyCharacter*> CollisionCandidates;

	int32 PeakActiveCount = 0;
	int32 OverflowCount = 0;
	int64 TotalSpawned = 0;
	int64 TotalReleased = 0;
};
