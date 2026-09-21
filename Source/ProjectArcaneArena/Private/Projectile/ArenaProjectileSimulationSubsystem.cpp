#include "Projectile/ArenaProjectileSimulationSubsystem.h"

#include "Core/ArenaLogCategories.h"
#include "HAL/IConsoleManager.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

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
	Generations.Empty();
	ActiveSlots.Empty();
	ActiveListPositions.Empty();
	FreeSlots.Empty();

	Super::Deinitialize();
}

// 只在真实 Game 与 PIE 世界创建，避免编辑器预览和资产世界产生无意义模拟器。
bool UArenaProjectileSimulationSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

// P1 只进行数据导向的位置积分和寿命淘汰；碰撞会在 P2 复用 PreviousPositions 接入。
void UArenaProjectileSimulationSubsystem::Tick(float DeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArenaProjectileSimulation);

	if (DeltaTime <= 0.0f || ActiveSlots.IsEmpty())
	{
		return;
	}

	for (int32 ActiveIndex = ActiveSlots.Num() - 1; ActiveIndex >= 0; --ActiveIndex)
	{
		const int32 Slot = ActiveSlots[ActiveIndex];
		PreviousPositions[Slot] = Positions[Slot];
		Positions[Slot] += Velocities[Slot] * DeltaTime;
		RemainingLife[Slot] -= DeltaTime;

		if (RemainingLife[Slot] <= 0.0f)
		{
			ReleaseSlotAtActiveIndex(ActiveIndex);
		}
	}
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

	Positions[Slot] = Params.Position;
	PreviousPositions[Slot] = Params.Position;
	Velocities[Slot] = Params.Velocity;
	Radii[Slot] = Params.Radius;
	RemainingLife[Slot] = Params.Lifetime;
	PierceRemaining[Slot] = FMath::Max(Params.PierceRemaining, 0);
	AttackInstanceIDs[Slot] = Params.AttackInstanceID;
	WeaponRuntimeIDs[Slot] = Params.WeaponRuntimeID;

	ActiveListPositions[Slot] = ActiveSlots.Add(Slot);

	OutHandle.Slot = Slot;
	OutHandle.Generation = static_cast<int32>(Generations[Slot]);

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

// 压力测试切档或世界收尾时一次性回收全部槽位，并递增 Generation 使旧 Handle 失效。
void UArenaProjectileSimulationSubsystem::ResetAllProjectiles()
{
	for (const int32 Slot : ActiveSlots)
	{
		if (Generations.IsValidIndex(Slot))
		{
			++Generations[Slot];
			if (Generations[Slot] == 0)
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
	}

	for (int32 Slot = Positions.Num() - 1; Slot >= 0; --Slot)
	{
		FreeSlots.Add(Slot);
	}
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

	++Generations[Slot];
	if (Generations[Slot] == 0)
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
		|| Generations[Slot] != static_cast<uint32>(Generation)
		|| !ActiveListPositions.IsValidIndex(Slot))
	{
		return false;
	}

	const int32 ActiveIndex = ActiveListPositions[Slot];
	return ActiveSlots.IsValidIndex(ActiveIndex) && ActiveSlots[ActiveIndex] == Slot;
}
