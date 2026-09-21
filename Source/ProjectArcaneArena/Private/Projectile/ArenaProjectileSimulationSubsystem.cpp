#include "Projectile/ArenaProjectileSimulationSubsystem.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/ArenaEnemyCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffect.h"
#include "Core/ArenaLogCategories.h"
#include "HAL/IConsoleManager.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Stats/Stats.h"

namespace
{
	TAutoConsoleVariable<int32> CVarArenaProjectileInitialCapacity(
		TEXT("arena.Projectile.InitialCapacity"),
		5000,
		TEXT("Initial slot capacity for the Arena Data Projectile pool."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarArenaProjectileMaxCapacity(
		TEXT("arena.Projectile.MaxCapacity"),
		10000,
		TEXT("Maximum slot capacity for the Arena Data Projectile pool."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarArenaProjectileGrowChunkSize(
		TEXT("arena.Projectile.GrowChunkSize"),
		1024,
		TEXT("Number of slots added when the Arena Data Projectile pool grows."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarArenaProjectileSpatialCellSize(
		TEXT("arena.Projectile.SpatialCellSize"),
		300.0f,
		TEXT("2D spatial hash cell size used by Arena Data Projectile collision."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarArenaProjectileLogHits(
		TEXT("arena.Projectile.LogHits"),
		0,
		TEXT("Log authoritative Data Projectile hit commands when non-zero."),
		ECVF_Default);

	// 将敌人 Capsule 近似为“水平圆柱 + Z 高度区间”，求 Previous→Current 线段第一次进入扩张体积的 Alpha。
	// 相比旧的最近中心点 Alpha，这个入口时间可以稳定排序同一帧高速穿过的多个目标。
	bool ComputeSweptTargetEntry(
		const FVector& Start,
		const FVector& End,
		const FVector& TargetLocation,
		float CapsuleRadius,
		float CapsuleHalfHeight,
		float ProjectileRadius,
		float& OutAlpha,
		FVector& OutImpactPoint,
		FVector& OutImpactNormal)
	{
		const float ExpandedRadius = FMath::Max(CapsuleRadius + ProjectileRadius, 1.0f);
		const float ExpandedHalfHeight = FMath::Max(CapsuleHalfHeight + ProjectileRadius, ExpandedRadius);

		const FVector2D Start2D(Start.X, Start.Y);
		const FVector2D End2D(End.X, End.Y);
		const FVector2D Target2D(TargetLocation.X, TargetLocation.Y);
		const FVector2D Segment2D = End2D - Start2D;
		const FVector2D Offset2D = Start2D - Target2D;

		float XYEnter = 0.0f;
		float XYExit = 1.0f;
		const float A = FVector2D::DotProduct(Segment2D, Segment2D);
		const float RadiusSquared = FMath::Square(ExpandedRadius);

		if (A <= KINDA_SMALL_NUMBER)
		{
			if (Offset2D.SizeSquared() > RadiusSquared)
			{
				return false;
			}
		}
		else
		{
			const float B = 2.0f * FVector2D::DotProduct(Offset2D, Segment2D);
			const float C = FVector2D::DotProduct(Offset2D, Offset2D) - RadiusSquared;
			const float Discriminant = B * B - 4.0f * A * C;
			if (Discriminant < 0.0f)
			{
				return false;
			}

			const float SqrtDiscriminant = FMath::Sqrt(FMath::Max(Discriminant, 0.0f));
			float T0 = (-B - SqrtDiscriminant) / (2.0f * A);
			float T1 = (-B + SqrtDiscriminant) / (2.0f * A);
			if (T0 > T1)
			{
				Swap(T0, T1);
			}
			if (T1 < 0.0f || T0 > 1.0f)
			{
				return false;
			}

			XYEnter = FMath::Clamp(T0, 0.0f, 1.0f);
			XYExit = FMath::Clamp(T1, 0.0f, 1.0f);
		}

		float ZEnter = 0.0f;
		float ZExit = 1.0f;
		const float MinZ = TargetLocation.Z - ExpandedHalfHeight;
		const float MaxZ = TargetLocation.Z + ExpandedHalfHeight;
		const float DeltaZ = End.Z - Start.Z;

		if (FMath::Abs(DeltaZ) <= KINDA_SMALL_NUMBER)
		{
			if (Start.Z < MinZ || Start.Z > MaxZ)
			{
				return false;
			}
		}
		else
		{
			float T0 = (MinZ - Start.Z) / DeltaZ;
			float T1 = (MaxZ - Start.Z) / DeltaZ;
			if (T0 > T1)
			{
				Swap(T0, T1);
			}
			if (T1 < 0.0f || T0 > 1.0f)
			{
				return false;
			}

			ZEnter = FMath::Clamp(T0, 0.0f, 1.0f);
			ZExit = FMath::Clamp(T1, 0.0f, 1.0f);
		}

		const float EnterAlpha = FMath::Max(XYEnter, ZEnter);
		const float ExitAlpha = FMath::Min(XYExit, ZExit);
		if (EnterAlpha > ExitAlpha + KINDA_SMALL_NUMBER)
		{
			return false;
		}

		OutAlpha = FMath::Clamp(EnterAlpha, 0.0f, 1.0f);
		OutImpactPoint = FMath::Lerp(Start, End, OutAlpha);

		OutImpactNormal = OutImpactPoint - TargetLocation;
		OutImpactNormal.Z = 0.0f;
		OutImpactNormal = OutImpactNormal.GetSafeNormal();
		if (OutImpactNormal.IsNearlyZero())
		{
			OutImpactNormal = -(End - Start).GetSafeNormal();
		}
		return true;
	}
}

// 初始化连续数据槽位；调用 Super 保证 UTickableWorldSubsystem 正确启停 Tick。
void UArenaProjectileSimulationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const int32 MaxCapacity = FMath::Max(CVarArenaProjectileMaxCapacity.GetValueOnGameThread(), 1);
	const int32 InitialCapacity = FMath::Clamp(
		CVarArenaProjectileInitialCapacity.GetValueOnGameThread(),
		1,
		MaxCapacity);

	GrowStorage(InitialCapacity);
	ResetStatistics();

	UE_LOG(
		LogArenaProjectile,
		Log,
		TEXT("Data Projectile pool initialized. Capacity=%d MaxCapacity=%d."),
		Positions.Num(),
		MaxCapacity);
}

// 世界销毁时先让所有 Handle 失效，再释放连续数组内存。
void UArenaProjectileSimulationSubsystem::Deinitialize()
{
	ResetAllProjectiles();

	Positions.Empty();
	PreviousPositions.Empty();
	Velocities.Empty();
	Radii.Empty();
	RemainingLife.Empty();
	PierceRemaining.Empty();
	AttackInstanceIDs.Empty();
	WeaponRuntimeIDs.Empty();
	VisualTypeIDs.Empty();
	PelletIndices.Empty();
	PelletCounts.Empty();
	SameTargetPelletFalloffs.Empty();
	MinPelletDamageMultipliers.Empty();
	SourceActors.Empty();
	DamageEffectClasses.Empty();
	DamageTypeTags.Empty();
	BaseDamages.Empty();
	SkillMultipliers.Empty();
	Generations.Empty();
	ActiveSlots.Empty();
	ActiveListPositions.Empty();
	FreeSlots.Empty();
	PendingHitCommands.Empty();
	FrameImpactVisualEvents.Empty();
	CollisionCandidates.Empty();
	SweepHitCandidates.Empty();
	ProjectileHitTargets.Empty();
	PelletHitStates.Empty();
	LastPelletHitStateCleanupTime = 0.0;
	SpatialGrid = FArenaProjectileSpatialGrid();

	Super::Deinitialize();
}

// 只在真实 Game 与 PIE 世界创建，避免编辑器预览和资产世界产生无意义模拟器。
bool UArenaProjectileSimulationSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

// 返回 Tickable 统计 ID，供引擎 Tick/Stat 系统安全采样本 Subsystem。
TStatId UArenaProjectileSimulationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UArenaProjectileSimulationSubsystem, STATGROUP_Tickables);
}

// P3 集中推进 Data Projectile，并在 Authority World 通过 Spatial Hash + Swept Collision 生成命中命令。
void UArenaProjectileSimulationSubsystem::Tick(float DeltaTime)
{
	if (DeltaTime <= 0.0f)
	{
		return;
	}

	FrameImpactVisualEvents.Reset();
	if (ActiveSlots.IsEmpty())
	{
		SimulationUpdated.Broadcast();
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArenaProjectileSimulation);

	UWorld* World = GetWorld();
	const bool bResolveAuthoritativeHits = World && World->GetNetMode() != NM_Client;
	PendingHitCommands.Reset();

	if (bResolveAuthoritativeHits)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ArenaProjectileSpatialGridBuild);
		SpatialGrid.Rebuild(CVarArenaProjectileSpatialCellSize.GetValueOnGameThread());
	}

	for (int32 ActiveIndex = ActiveSlots.Num() - 1; ActiveIndex >= 0; --ActiveIndex)
	{
		const int32 Slot = ActiveSlots[ActiveIndex];
		PreviousPositions[Slot] = Positions[Slot];
		Positions[Slot] += Velocities[Slot] * DeltaTime;
		RemainingLife[Slot] -= DeltaTime;

		if (bResolveAuthoritativeHits
			&& SourceActors[Slot].IsValid()
			&& DamageEffectClasses[Slot])
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ArenaProjectileSweptCollision);
			if (ResolveProjectileSweptHits(Slot))
			{
				ReleaseSlotAtActiveIndex(ActiveIndex);
				continue;
			}
		}

		if (RemainingLife[Slot] <= 0.0f)
		{
			ReleaseSlotAtActiveIndex(ActiveIndex);
		}
	}

	if (!PendingHitCommands.IsEmpty())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ArenaProjectileHitCommands);
		ApplyPendingHitCommands();
	}

	SimulationUpdated.Broadcast();
}

// 校验输入后获取可复用槽位；数据池耗尽会明确计数而不是静默覆盖飞行中的 Projectile。
bool UArenaProjectileSimulationSubsystem::SpawnProjectile(
	const FArenaProjectileSpawnParams& Params,
	FArenaProjectileHandle& OutHandle)
{
	OutHandle.Reset();

	if (Params.Position.ContainsNaN()
		|| Params.Velocity.ContainsNaN()
		|| !FMath::IsFinite(Params.Radius)
		|| !FMath::IsFinite(Params.Lifetime)
		|| Params.Radius < 0.0f
		|| Params.Lifetime <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogArenaProjectile, Warning, TEXT("Rejected invalid Data Projectile spawn parameters."));
		return false;
	}

	if (!EnsureFreeSlot())
	{
		++OverflowCount;
		return false;
	}

	const int32 Slot = FreeSlots.Pop(EAllowShrinking::No);
	check(ActiveListPositions.IsValidIndex(Slot));
	check(ActiveListPositions[Slot] == INDEX_NONE);

	ProjectileHitTargets.Remove(Slot);
	Positions[Slot] = Params.Position;
	PreviousPositions[Slot] = Params.Position;
	Velocities[Slot] = Params.Velocity;
	Radii[Slot] = Params.Radius;
	RemainingLife[Slot] = Params.Lifetime;
	PierceRemaining[Slot] = FMath::Max(Params.PierceRemaining, 0);
	AttackInstanceIDs[Slot] = Params.AttackInstanceID;
	WeaponRuntimeIDs[Slot] = Params.WeaponRuntimeID;
	VisualTypeIDs[Slot] = FMath::Max(Params.VisualTypeID, 0);
	PelletIndices[Slot] = FMath::Max(Params.PelletIndex, 0);
	PelletCounts[Slot] = FMath::Max(Params.PelletCount, 1);
	SameTargetPelletFalloffs[Slot] = FMath::Clamp(Params.SameTargetPelletFalloff, 0.0f, 1.0f);
	MinPelletDamageMultipliers[Slot] = FMath::Clamp(Params.MinPelletDamageMultiplier, 0.0f, 1.0f);
	SourceActors[Slot] = Params.SourceActor.Get();
	DamageEffectClasses[Slot] = Params.DamageEffectClass;
	DamageTypeTags[Slot] = Params.DamageTypeTag;
	BaseDamages[Slot] = FMath::Max(Params.BaseDamage, 0.0f);
	SkillMultipliers[Slot] = FMath::Max(Params.SkillMultiplier, 0.0f);

	ActiveListPositions[Slot] = ActiveSlots.Add(Slot);

	OutHandle.Slot = Slot;
	OutHandle.Generation = Generations[Slot];

	++TotalSpawned;
	PeakActiveCount = FMath::Max(PeakActiveCount, ActiveSlots.Num());
	return true;
}

// 外部释放必须同时匹配 Slot 与 Generation，保证已经过期的 Handle 无副作用。
bool UArenaProjectileSimulationSubsystem::ReleaseProjectile(const FArenaProjectileHandle& Handle)
{
	if (!IsSlotGenerationValid(Handle.Slot, Handle.Generation))
	{
		return false;
	}

	ReleaseSlotAtActiveIndex(ActiveListPositions[Handle.Slot]);
	return true;
}

// Handle 只有仍位于 ActiveSlots 且代次一致时才视为存活。
bool UArenaProjectileSimulationSubsystem::IsProjectileAlive(const FArenaProjectileHandle& Handle) const
{
	return IsSlotGenerationValid(Handle.Slot, Handle.Generation);
}

// 读取表现位置前重复校验代次，避免 VisualSubsystem 读取到同 Slot 的下一发 Projectile。
bool UArenaProjectileSimulationSubsystem::GetProjectilePosition(
	const FArenaProjectileHandle& Handle,
	FVector& OutPosition) const
{
	if (!IsSlotGenerationValid(Handle.Slot, Handle.Generation))
	{
		return false;
	}

	OutPosition = Positions[Handle.Slot];
	return true;
}


// 将当前 ActiveSlots 复制为紧凑只读视觉快照；表现预算截断不影响模拟数组和真实命中。
void UArenaProjectileSimulationSubsystem::BuildVisualSnapshot(
	TArray<FArenaProjectileVisualSample>& OutSamples,
	int32 MaxSamples) const
{
	OutSamples.Reset();
	const int32 SafeMaxSamples = FMath::Max(MaxSamples, 0);
	const int32 SampleCount = FMath::Min(ActiveSlots.Num(), SafeMaxSamples);
	OutSamples.Reserve(SampleCount);

	for (int32 ActiveIndex = 0; ActiveIndex < SampleCount; ++ActiveIndex)
	{
		const int32 Slot = ActiveSlots[ActiveIndex];
		if (!Positions.IsValidIndex(Slot)
			|| !Velocities.IsValidIndex(Slot)
			|| !Radii.IsValidIndex(Slot)
			|| !RemainingLife.IsValidIndex(Slot)
			|| !Generations.IsValidIndex(Slot)
			|| !VisualTypeIDs.IsValidIndex(Slot))
		{
			continue;
		}

		FArenaProjectileVisualSample& Sample = OutSamples.AddDefaulted_GetRef();
		Sample.Handle.Slot = Slot;
		Sample.Handle.Generation = Generations[Slot];
		Sample.Position = Positions[Slot];
		Sample.Velocity = Velocities[Slot];
		Sample.Radius = Radii[Slot];
		Sample.RemainingLife = RemainingLife[Slot];
		Sample.VisualTypeID = VisualTypeIDs[Slot];
		Sample.WeaponRuntimeID = WeaponRuntimeIDs[Slot];
		Sample.AttackInstanceID = AttackInstanceIDs[Slot];
		Sample.PelletIndex = PelletIndices[Slot];
		Sample.PelletCount = PelletCounts[Slot];
	}
}

// Impact 事件只存活一个 Simulation Tick；表现层在 SimulationUpdated 回调中同步读取。
void UArenaProjectileSimulationSubsystem::CopyFrameImpactVisualEvents(
	TArray<FArenaProjectileImpactVisualEvent>& OutEvents) const
{
	OutEvents = FrameImpactVisualEvents;
}

// 压力测试切档或世界收尾时一次性回收全部槽位，并递增 Generation 使旧 Handle 失效。
void UArenaProjectileSimulationSubsystem::ResetAllProjectiles()
{
	TotalReleased += ActiveSlots.Num();

	for (const int32 Slot : ActiveSlots)
	{
		if (Generations.IsValidIndex(Slot))
		{
			++Generations[Slot];
			if (Generations[Slot] <= 0)
			{
				Generations[Slot] = 1;
			}
		}
	}

	ActiveSlots.Reset();
	FreeSlots.Reset(Positions.Num());

	for (int32 Slot = 0; Slot < Positions.Num(); ++Slot)
	{
		ActiveListPositions[Slot] = INDEX_NONE;
		Positions[Slot] = FVector::ZeroVector;
		PreviousPositions[Slot] = FVector::ZeroVector;
		Velocities[Slot] = FVector::ZeroVector;
		Radii[Slot] = 0.0f;
		RemainingLife[Slot] = 0.0f;
		PierceRemaining[Slot] = 0;
		AttackInstanceIDs[Slot] = 0;
		WeaponRuntimeIDs[Slot] = INDEX_NONE;
		VisualTypeIDs[Slot] = 0;
		PelletIndices[Slot] = 0;
		PelletCounts[Slot] = 1;
		SameTargetPelletFalloffs[Slot] = 1.0f;
		MinPelletDamageMultipliers[Slot] = 1.0f;
		SourceActors[Slot].Reset();
		DamageEffectClasses[Slot] = nullptr;
		DamageTypeTags[Slot] = FGameplayTag();
		BaseDamages[Slot] = 0.0f;
		SkillMultipliers[Slot] = 1.0f;
	}

	for (int32 Slot = Positions.Num() - 1; Slot >= 0; --Slot)
	{
		FreeSlots.Add(Slot);
	}

	ProjectileHitTargets.Reset();
	SweepHitCandidates.Reset();
	FrameImpactVisualEvents.Reset();
	PelletHitStates.Reset();
	LastPelletHitStateCleanupTime = 0.0;
}

// 清空 P0/P1 累计统计，保留当前 ActiveCount 作为新的峰值起点。
void UArenaProjectileSimulationSubsystem::ResetStatistics()
{
	PeakActiveCount = ActiveSlots.Num();
	OverflowCount = 0;
	TotalSpawned = 0;
	TotalReleased = 0;
}

// 生成轻量统计快照，供 StressTest 日志和后续 HUD/Telemetry 使用。
FArenaProjectileSimulationStats UArenaProjectileSimulationSubsystem::GetStats() const
{
	FArenaProjectileSimulationStats Stats;
	Stats.Capacity = Positions.Num();
	Stats.ActiveCount = ActiveSlots.Num();
	Stats.FreeCount = FreeSlots.Num();
	Stats.PeakActiveCount = PeakActiveCount;
	Stats.OverflowCount = OverflowCount;
	Stats.TotalSpawned = TotalSpawned;
	Stats.TotalReleased = TotalReleased;
	return Stats;
}

// Free List 耗尽后按 Chunk 扩容，超过 MaxCapacity 时由 SpawnProjectile 记录 Overflow。
bool UArenaProjectileSimulationSubsystem::EnsureFreeSlot()
{
	if (!FreeSlots.IsEmpty())
	{
		return true;
	}

	const int32 CurrentCapacity = Positions.Num();
	const int32 MaxCapacity = FMath::Max(CVarArenaProjectileMaxCapacity.GetValueOnGameThread(), 1);
	if (CurrentCapacity >= MaxCapacity)
	{
		return false;
	}

	const int32 GrowChunkSize = FMath::Max(CVarArenaProjectileGrowChunkSize.GetValueOnGameThread(), 1);
	const int32 NewCapacity = FMath::Min(CurrentCapacity + GrowChunkSize, MaxCapacity);
	GrowStorage(NewCapacity);
	return !FreeSlots.IsEmpty();
}

// 所有热数据数组保持同一 Slot 索引，并只把新增范围加入 Free List。
void UArenaProjectileSimulationSubsystem::GrowStorage(int32 NewCapacity)
{
	const int32 OldCapacity = Positions.Num();
	if (NewCapacity <= OldCapacity)
	{
		return;
	}

	Positions.SetNum(NewCapacity);
	PreviousPositions.SetNum(NewCapacity);
	Velocities.SetNum(NewCapacity);
	Radii.SetNum(NewCapacity);
	RemainingLife.SetNum(NewCapacity);
	PierceRemaining.SetNum(NewCapacity);
	AttackInstanceIDs.SetNum(NewCapacity);
	WeaponRuntimeIDs.SetNum(NewCapacity);
	VisualTypeIDs.SetNum(NewCapacity);
	PelletIndices.SetNum(NewCapacity);
	PelletCounts.SetNum(NewCapacity);
	SameTargetPelletFalloffs.SetNum(NewCapacity);
	MinPelletDamageMultipliers.SetNum(NewCapacity);
	SourceActors.SetNum(NewCapacity);
	DamageEffectClasses.SetNum(NewCapacity);
	DamageTypeTags.SetNum(NewCapacity);
	BaseDamages.SetNum(NewCapacity);
	SkillMultipliers.SetNum(NewCapacity);
	Generations.SetNum(NewCapacity);
	ActiveListPositions.SetNum(NewCapacity);

	FreeSlots.Reserve(NewCapacity);
	ActiveSlots.Reserve(NewCapacity);

	for (int32 Slot = OldCapacity; Slot < NewCapacity; ++Slot)
	{
		Positions[Slot] = FVector::ZeroVector;
		PreviousPositions[Slot] = FVector::ZeroVector;
		Velocities[Slot] = FVector::ZeroVector;
		Radii[Slot] = 0.0f;
		RemainingLife[Slot] = 0.0f;
		PierceRemaining[Slot] = 0;
		AttackInstanceIDs[Slot] = 0;
		WeaponRuntimeIDs[Slot] = INDEX_NONE;
		VisualTypeIDs[Slot] = 0;
		PelletIndices[Slot] = 0;
		PelletCounts[Slot] = 1;
		SameTargetPelletFalloffs[Slot] = 1.0f;
		MinPelletDamageMultipliers[Slot] = 1.0f;
		SourceActors[Slot].Reset();
		DamageEffectClasses[Slot] = nullptr;
		DamageTypeTags[Slot] = FGameplayTag();
		BaseDamages[Slot] = 0.0f;
		SkillMultipliers[Slot] = 1.0f;
		Generations[Slot] = 1;
		ActiveListPositions[Slot] = INDEX_NONE;
	}

	for (int32 Slot = NewCapacity - 1; Slot >= OldCapacity; --Slot)
	{
		FreeSlots.Add(Slot);
	}
}

// 稠密 ActiveSlots 使用 SwapRemove；被交换进来的 Slot 同步更新 ActiveListPositions。
void UArenaProjectileSimulationSubsystem::ReleaseSlotAtActiveIndex(int32 ActiveIndex)
{
	if (!ActiveSlots.IsValidIndex(ActiveIndex))
	{
		return;
	}

	const int32 Slot = ActiveSlots[ActiveIndex];
	ActiveSlots.RemoveAtSwap(ActiveIndex, 1, EAllowShrinking::No);

	if (ActiveSlots.IsValidIndex(ActiveIndex))
	{
		ActiveListPositions[ActiveSlots[ActiveIndex]] = ActiveIndex;
	}

	ActiveListPositions[Slot] = INDEX_NONE;
	Positions[Slot] = FVector::ZeroVector;
	PreviousPositions[Slot] = FVector::ZeroVector;
	Velocities[Slot] = FVector::ZeroVector;
	Radii[Slot] = 0.0f;
	RemainingLife[Slot] = 0.0f;
	PierceRemaining[Slot] = 0;
	AttackInstanceIDs[Slot] = 0;
	WeaponRuntimeIDs[Slot] = INDEX_NONE;
	VisualTypeIDs[Slot] = 0;
	PelletIndices[Slot] = 0;
	PelletCounts[Slot] = 1;
	SameTargetPelletFalloffs[Slot] = 1.0f;
	MinPelletDamageMultipliers[Slot] = 1.0f;
	SourceActors[Slot].Reset();
	DamageEffectClasses[Slot] = nullptr;
	DamageTypeTags[Slot] = FGameplayTag();
	BaseDamages[Slot] = 0.0f;
	SkillMultipliers[Slot] = 1.0f;
	ProjectileHitTargets.Remove(Slot);

	++Generations[Slot];
	if (Generations[Slot] <= 0)
	{
		Generations[Slot] = 1;
	}

	FreeSlots.Add(Slot);
	++TotalReleased;
}

// 统一校验范围、代次和反向索引，任何一项不匹配都视为旧 Handle。
bool UArenaProjectileSimulationSubsystem::IsSlotGenerationValid(int32 Slot, int32 Generation) const
{
	if (!Generations.IsValidIndex(Slot)
		|| Generation <= 0
		|| Generations[Slot] != Generation
		|| !ActiveListPositions.IsValidIndex(Slot))
	{
		return false;
	}

	const int32 ActiveIndex = ActiveListPositions[Slot];
	return ActiveSlots.IsValidIndex(ActiveIndex) && ActiveSlots[ActiveIndex] == Slot;
}


// Enemy 仅在服务器生命周期内注册一次，Projectile Tick 不再为每颗弹扫描 World Actor。
void UArenaProjectileSimulationSubsystem::RegisterCollisionTarget(AArenaEnemyCharacter* Target)
{
	if (Target && Target->HasAuthority())
	{
		SpatialGrid.RegisterTarget(Target);
	}
}

// Enemy EndPlay 主动注销，避免空间索引持有已经离场的弱引用直到下一帧。
void UArenaProjectileSimulationSubsystem::UnregisterCollisionTarget(AArenaEnemyCharacter* Target)
{
	SpatialGrid.UnregisterTarget(Target);
}

// 宽相候选来自 Spatial Hash；窄相用 Previous→Current 的二维扫掠和 Capsule 高度检查，选择本帧最早命中。
bool UArenaProjectileSimulationSubsystem::ResolveProjectileSweptHits(int32 Slot)
{
	if (!Positions.IsValidIndex(Slot)
		|| !PreviousPositions.IsValidIndex(Slot)
		|| !Radii.IsValidIndex(Slot)
		|| !PierceRemaining.IsValidIndex(Slot)
		|| !PelletIndices.IsValidIndex(Slot)
		|| !PelletCounts.IsValidIndex(Slot)
		|| !SameTargetPelletFalloffs.IsValidIndex(Slot)
		|| !MinPelletDamageMultipliers.IsValidIndex(Slot)
		|| !SourceActors.IsValidIndex(Slot)
		|| !DamageEffectClasses.IsValidIndex(Slot)
		|| !SourceActors[Slot].IsValid()
		|| !DamageEffectClasses[Slot])
	{
		return false;
	}

	const FVector Start = PreviousPositions[Slot];
	const FVector End = Positions[Slot];
	const float ProjectileRadius = FMath::Max(Radii[Slot], 0.0f);
	SpatialGrid.QuerySegment(Start, End, ProjectileRadius, CollisionCandidates);
	if (CollisionCandidates.IsEmpty())
	{
		return false;
	}

	SweepHitCandidates.Reset();
	SweepHitCandidates.Reserve(CollisionCandidates.Num());
	const TArray<FObjectKey>* ExistingHitTargets = ProjectileHitTargets.Find(Slot);

	for (AArenaEnemyCharacter* Target : CollisionCandidates)
	{
		if (!IsValid(Target))
		{
			continue;
		}

		const FObjectKey TargetKey(Target);
		if (ExistingHitTargets && ExistingHitTargets->Contains(TargetKey))
		{
			continue;
		}

		const UAbilitySystemComponent* TargetASC = Target->GetAbilitySystemComponent();
		if (!TargetASC || TargetASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead))
		{
			continue;
		}

		float CapsuleRadius = 50.0f;
		float CapsuleHalfHeight = 90.0f;
		if (const UCapsuleComponent* Capsule = Target->GetCapsuleComponent())
		{
			CapsuleRadius = FMath::Max(Capsule->GetScaledCapsuleRadius(), 1.0f);
			CapsuleHalfHeight = FMath::Max(Capsule->GetScaledCapsuleHalfHeight(), CapsuleRadius);
		}

		FArenaProjectileSweepCandidate Candidate;
		Candidate.Target = Target;
		if (!ComputeSweptTargetEntry(
			Start,
			End,
			Target->GetActorLocation(),
			CapsuleRadius,
			CapsuleHalfHeight,
			ProjectileRadius,
			Candidate.Alpha,
			Candidate.ImpactPoint,
			Candidate.ImpactNormal))
		{
			continue;
		}

		SweepHitCandidates.Add(Candidate);
	}

	if (SweepHitCandidates.IsEmpty())
	{
		return false;
	}

	SweepHitCandidates.Sort([](const FArenaProjectileSweepCandidate& Left, const FArenaProjectileSweepCandidate& Right)
	{
		if (!FMath::IsNearlyEqual(Left.Alpha, Right.Alpha))
		{
			return Left.Alpha < Right.Alpha;
		}

		const int32 LeftID = IsValid(Left.Target) ? Left.Target->GetUniqueID() : TNumericLimits<int32>::Max();
		const int32 RightID = IsValid(Right.Target) ? Right.Target->GetUniqueID() : TNumericLimits<int32>::Max();
		return LeftID < RightID;
	});

	TArray<FObjectKey>& HitTargets = ProjectileHitTargets.FindOrAdd(Slot);
	for (const FArenaProjectileSweepCandidate& Candidate : SweepHitCandidates)
	{
		AArenaEnemyCharacter* Target = Candidate.Target;
		if (!IsValid(Target))
		{
			continue;
		}

		const FObjectKey TargetKey(Target);
		if (HitTargets.Contains(TargetKey))
		{
			continue;
		}

		const UAbilitySystemComponent* TargetASC = Target->GetAbilitySystemComponent();
		if (!TargetASC || TargetASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead))
		{
			continue;
		}

		const int32 ProjectileHitOrdinal = HitTargets.Num() + 1;
		const bool bCanContinueAfterThisHit = PierceRemaining[Slot] > 0;
		if (bCanContinueAfterThisHit)
		{
			--PierceRemaining[Slot];
		}

		UPrimitiveComponent* HitComponent = Cast<UPrimitiveComponent>(Target->GetRootComponent());
		FHitResult HitResult(Target, HitComponent, Candidate.ImpactPoint, Candidate.ImpactNormal);
		HitResult.TraceStart = Start;
		HitResult.TraceEnd = End;
		HitResult.Time = Candidate.Alpha;
		HitResult.Distance = FVector::Distance(Start, Candidate.ImpactPoint);
		HitResult.bBlockingHit = true;

		FArenaProjectileHitCommand HitCommand;
		HitCommand.SourceActor = SourceActors[Slot];
		HitCommand.TargetActor = Target;
		HitCommand.DamageEffectClass = DamageEffectClasses[Slot];
		HitCommand.DamageTypeTag = DamageTypeTags[Slot];
		HitCommand.BaseDamage = BaseDamages[Slot];
		HitCommand.SkillMultiplier = SkillMultipliers[Slot];
		HitCommand.SameTargetPelletFalloff = SameTargetPelletFalloffs[Slot];
		HitCommand.MinPelletDamageMultiplier = MinPelletDamageMultipliers[Slot];
		HitCommand.PelletTrackingLifetime = FMath::Max(RemainingLife[Slot], 0.1f);
		HitCommand.AttackInstanceID = AttackInstanceIDs[Slot];
		HitCommand.WeaponRuntimeID = WeaponRuntimeIDs[Slot];
		HitCommand.PelletIndex = PelletIndices[Slot];
		HitCommand.PelletCount = PelletCounts[Slot];
		HitCommand.ProjectileHitOrdinal = ProjectileHitOrdinal;
		HitCommand.PierceRemainingAfterHit = PierceRemaining[Slot];
		HitCommand.HitResult = HitResult;
		PendingHitCommands.Add(MoveTemp(HitCommand));

		FArenaProjectileImpactVisualEvent& ImpactEvent = FrameImpactVisualEvents.AddDefaulted_GetRef();
		ImpactEvent.Position = Candidate.ImpactPoint;
		ImpactEvent.Normal = Candidate.ImpactNormal;
		ImpactEvent.VisualTypeID = VisualTypeIDs[Slot];
		ImpactEvent.WeaponRuntimeID = WeaponRuntimeIDs[Slot];
		ImpactEvent.AttackInstanceID = AttackInstanceIDs[Slot];
		ImpactEvent.PelletIndex = PelletIndices[Slot];
		ImpactEvent.ProjectileHitOrdinal = ProjectileHitOrdinal;

		HitTargets.Add(TargetKey);

		// PierceCount 表示“首个目标之后还能继续穿过多少个目标”。预算为 0 的这次命中仍然有效，命中后立即回收。
		if (!bCanContinueAfterThisHit)
		{
			return true;
		}
	}

	return false;
}

// HitCommand 在模拟循环结束后统一消费；伤害仍走 GE_Damage -> ExecCalc_Damage -> AttributeSet，不直接写属性。
void UArenaProjectileSimulationSubsystem::ApplyPendingHitCommands()
{
	UWorld* World = GetWorld();
	const double NowSeconds = World ? static_cast<double>(World->GetTimeSeconds()) : 0.0;

	// 霰弹衰减状态只需要覆盖同一轮 Projectile 的最大剩余寿命；每秒做一次惰性清理，避免高命中率下每帧全表扫描。
	if (NowSeconds - LastPelletHitStateCleanupTime >= 1.0)
	{
		for (auto It = PelletHitStates.CreateIterator(); It; ++It)
		{
			if (It.Value().ExpireWorldTime <= NowSeconds)
			{
				It.RemoveCurrent();
			}
		}
		LastPelletHitStateCleanupTime = NowSeconds;
	}

	for (const FArenaProjectileHitCommand& Command : PendingHitCommands)
	{
		AActor* SourceActor = Command.SourceActor.Get();
		AActor* TargetActor = Command.TargetActor.Get();
		if (!IsValid(SourceActor) || !IsValid(TargetActor) || !Command.DamageEffectClass)
		{
			continue;
		}

		UAbilitySystemComponent* SourceASC =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(SourceActor);
		UAbilitySystemComponent* TargetASC =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
		if (!SourceASC
			|| !TargetASC
			|| !SourceASC->IsOwnerActorAuthoritative()
			|| TargetASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead))
		{
			continue;
		}

		float PelletDamageMultiplier = 1.0f;
		int32 SameTargetHitIndex = 0;
		FArenaPelletHitState* PelletHitStateToCommit = nullptr;
		if (Command.PelletCount > 1
			&& Command.AttackInstanceID > 0
			&& Command.WeaponRuntimeID > 0)
		{
			const FArenaPelletHitKey PelletHitKey(
				SourceActor,
				TargetActor,
				Command.WeaponRuntimeID,
				Command.AttackInstanceID);
			FArenaPelletHitState& PelletHitState = PelletHitStates.FindOrAdd(PelletHitKey);
			SameTargetHitIndex = FMath::Max(PelletHitState.AppliedHitCount, 0);
			PelletHitStateToCommit = &PelletHitState;

			const float Falloff = FMath::Clamp(Command.SameTargetPelletFalloff, 0.0f, 1.0f);
			const float MinimumMultiplier = FMath::Clamp(Command.MinPelletDamageMultiplier, 0.0f, 1.0f);
			PelletDamageMultiplier = FMath::Max(
				MinimumMultiplier,
				FMath::Pow(Falloff, static_cast<float>(SameTargetHitIndex)));

		}

		FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
		EffectContext.AddSourceObject(SourceActor);
		EffectContext.AddHitResult(Command.HitResult, true);

		FGameplayEffectSpecHandle DamageSpecHandle =
			SourceASC->MakeOutgoingSpec(Command.DamageEffectClass, 1.0f, EffectContext);
		if (!DamageSpecHandle.IsValid())
		{
			continue;
		}

		if (PelletHitStateToCommit)
		{
			++PelletHitStateToCommit->AppliedHitCount;
			PelletHitStateToCommit->ExpireWorldTime = FMath::Max(
				PelletHitStateToCommit->ExpireWorldTime,
				NowSeconds + FMath::Max(static_cast<double>(Command.PelletTrackingLifetime), 0.1));
		}

		FGameplayEffectSpec* DamageSpec = DamageSpecHandle.Data.Get();
		DamageSpec->SetSetByCallerMagnitude(
			ArenaGameplayTags::SetByCaller_Damage_Base,
			FMath::Max(Command.BaseDamage, 0.0f));
		DamageSpec->SetSetByCallerMagnitude(
			ArenaGameplayTags::SetByCaller_Damage_SkillMultiplier,
			FMath::Max(Command.SkillMultiplier * PelletDamageMultiplier, 0.0f));
		if (Command.DamageTypeTag.IsValid())
		{
			DamageSpec->AddDynamicAssetTag(Command.DamageTypeTag);
		}

		TargetASC->ApplyGameplayEffectSpecToSelf(*DamageSpec);

		if (CVarArenaProjectileLogHits.GetValueOnGameThread() != 0)
		{
			UE_LOG(
				LogArenaProjectile,
				Log,
				TEXT("DataProjectile hit. Source=%s Target=%s AttackID=%d WeaponRuntimeID=%d Pellet=%d/%d ProjectileHit=%d PierceRemaining=%d SameTargetHit=%d PelletMultiplier=%.3f BaseDamage=%.2f."),
				*GetNameSafe(SourceActor),
				*GetNameSafe(TargetActor),
				Command.AttackInstanceID,
				Command.WeaponRuntimeID,
				Command.PelletIndex + 1,
				FMath::Max(Command.PelletCount, 1),
				FMath::Max(Command.ProjectileHitOrdinal, 1),
				FMath::Max(Command.PierceRemainingAfterHit, 0),
				SameTargetHitIndex + 1,
				PelletDamageMultiplier,
				Command.BaseDamage);
		}
	}

	PendingHitCommands.Reset();
}

