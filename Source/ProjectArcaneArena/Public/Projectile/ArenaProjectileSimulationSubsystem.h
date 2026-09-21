#pragma once

#include "CoreMinimal.h"
#include "Projectile/ArenaProjectileTypes.h"
#include "Projectile/ArenaProjectileSpatialGrid.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/ObjectKey.h"
#include "ArenaProjectileSimulationSubsystem.generated.h"

class AActor;
class AArenaEnemyCharacter;

DECLARE_MULTICAST_DELEGATE(FArenaProjectileSimulationUpdated);

// 同一轮霰弹对同一目标的命中计数 Key；FObjectKey 不持有 UObject 强引用，避免伤害衰减状态延长 Actor 生命周期。
struct FArenaPelletHitKey
{
	FObjectKey SourceActor;
	FObjectKey TargetActor;
	int32 WeaponRuntimeID = INDEX_NONE;
	int32 AttackInstanceID = 0;

	FArenaPelletHitKey(AActor* InSourceActor, AActor* InTargetActor, int32 InWeaponRuntimeID, int32 InAttackInstanceID)
		: SourceActor(InSourceActor)
		, TargetActor(InTargetActor)
		, WeaponRuntimeID(InWeaponRuntimeID)
		, AttackInstanceID(InAttackInstanceID)
	{
	}

	bool operator==(const FArenaPelletHitKey& Other) const
	{
		return SourceActor == Other.SourceActor
			&& TargetActor == Other.TargetActor
			&& WeaponRuntimeID == Other.WeaponRuntimeID
			&& AttackInstanceID == Other.AttackInstanceID;
	}

	friend uint32 GetTypeHash(const FArenaPelletHitKey& Key)
	{
		uint32 Hash = HashCombine(GetTypeHash(Key.SourceActor), GetTypeHash(Key.TargetActor));
		Hash = HashCombine(Hash, GetTypeHash(Key.WeaponRuntimeID));
		return HashCombine(Hash, GetTypeHash(Key.AttackInstanceID));
	}
};

struct FArenaPelletHitState
{
	int32 AppliedHitCount = 0;
	double ExpireWorldTime = 0.0;
};

// 单颗 Projectile 当前帧扫掠得到的候选命中；只作为 GameThread 临时排序数据，不持有跨帧玩法状态。
struct FArenaProjectileSweepCandidate
{
	AArenaEnemyCharacter* Target = nullptr;
	float Alpha = 0.0f;
	FVector ImpactPoint = FVector::ZeroVector;
	FVector ImpactNormal = FVector::ZeroVector;
};

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

	// P5 批量复制当前 Active Projectile 的纯表现快照；MaxSamples 只限制视觉预算，不改变权威 Projectile。
	void BuildVisualSnapshot(TArray<FArenaProjectileVisualSample>& OutSamples, int32 MaxSamples) const;

	// 复制当前 Tick 已产生的命中表现事件；只读，不参与 GAS 伤害结算。
	void CopyFrameImpactVisualEvents(TArray<FArenaProjectileImpactVisualEvent>& OutEvents) const;

	// 每次 Simulation Tick 完成后广播，表现层可在同一帧读取稳定位置和本帧 Impact。
	FArenaProjectileSimulationUpdated& OnSimulationUpdated()
	{
		return SimulationUpdated;
	}

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

	// 对单颗 Projectile 的 PreviousPosition→Position 做直线 Swept Collision；按沿线顺序生成本帧所有允许的 Pierce 命中。
	// 返回 true 表示该 Projectile 已耗尽 Pierce 预算，应在当前 Tick 立即回收。
	bool ResolveProjectileSweptHits(int32 Slot);

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
	TArray<int32> VisualTypeIDs;
	TArray<int32> PelletIndices;
	TArray<int32> PelletCounts;
	TArray<float> SameTargetPelletFalloffs;
	TArray<float> MinPelletDamageMultipliers;
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
	TArray<FArenaProjectileImpactVisualEvent> FrameImpactVisualEvents;
	TArray<AArenaEnemyCharacter*> CollisionCandidates;
	TArray<FArenaProjectileSweepCandidate> SweepHitCandidates;

	// 只为真正发生过穿透命中的 Active Slot 保存目标历史；释放/复用 Slot 时立即清理，避免同一 Projectile 跨帧重复命中同一 Actor。
	TMap<int32, TArray<FObjectKey>> ProjectileHitTargets;

	// 仅为同一 AttackInstanceID 的霰弹跨帧命中保存短生命周期计数；到期后延迟清理，避免每颗 Pellet 永久留状态。
	TMap<FArenaPelletHitKey, FArenaPelletHitState> PelletHitStates;
	double LastPelletHitStateCleanupTime = 0.0;

	FArenaProjectileSimulationUpdated SimulationUpdated;

	int32 PeakActiveCount = 0;
	int32 OverflowCount = 0;
	int64 TotalSpawned = 0;
	int64 TotalReleased = 0;
};
