#include "Tests/ArenaProjectileStressTestActor.h"

#include "Core/ArenaLogCategories.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Projectile/ArenaProjectileSimulationSubsystem.h"
#include "Tests/ArenaProjectileLegacyBenchmarkActor.h"

// 创建无表现、无真实伤害的压力入口，避免 Benchmark 本身引入额外 Tick 系统。
AArenaProjectileStressTestActor::AArenaProjectileStressTestActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	bReplicates = true;
	SetReplicateMovement(false);
	FrameTimeSamplesMs.Reserve(1024);
}

// AuthorityOnly 默认让 Listen Server 只生成一份逻辑压力；Standalone 仍正常运行。
void AArenaProjectileStressTestActor::BeginPlay()
{
	Super::BeginPlay();

	if (bRunOnAuthorityOnly && !HasAuthority())
	{
		SetActorTickEnabled(false);
		return;
	}

	if (bAutoStart)
	{
		StartStressTest();
	}
}

// 停止旧轮次、清空统计并按固定 Seed 开始；Prefill 用于单独观察一次性创建尖峰。
void AArenaProjectileStressTestActor::StartStressTest()
{
	if (bRunOnAuthorityOnly && !HasAuthority())
	{
		return;
	}

	StopStressTest(true);

	SpawnRandom.Initialize(RandomSeed);
	SpawnAccumulator = 0.0;
	ReportElapsedSeconds = 0.0;
	SubmittedSpawnCount = 0;
	FailedSpawnCount = 0;
	FrameTimeSamplesMs.Reset();
	bRunning = true;
	SetActorTickEnabled(true);

	if (StressMode == EArenaProjectileStressMode::DataPool)
	{
		if (UArenaProjectileSimulationSubsystem* Subsystem = GetProjectileSubsystem())
		{
			Subsystem->ResetAllProjectiles();
			Subsystem->ResetStatistics();
		}
	}

	if (bPrefillTargetOnStart)
	{
		SpawnBatch(FMath::Max(TargetActiveProjectiles, 0));
	}

	UE_LOG(
		LogArenaProjectile,
		Log,
		TEXT("Projectile stress started. Mode=%s TargetActive=%d Lifetime=%.2f Speed=%.1f Seed=%d."),
		StressMode == EArenaProjectileStressMode::DataPool ? TEXT("DataPool") : TEXT("LegacyActor"),
		TargetActiveProjectiles,
		ProjectileLifetime,
		ProjectileSpeed,
		RandomSeed);
}

// 停止发射；DataPool 可立即归还全部槽位，Legacy Actor 仅销毁本 StressTest 拥有的参考 Actor。
void AArenaProjectileStressTestActor::StopStressTest(bool bCleanupProjectiles)
{
	bRunning = false;
	SpawnAccumulator = 0.0;

	if (!bCleanupProjectiles || !GetWorld())
	{
		return;
	}

	if (StressMode == EArenaProjectileStressMode::DataPool)
	{
		if (UArenaProjectileSimulationSubsystem* Subsystem = GetProjectileSubsystem())
		{
			Subsystem->ResetAllProjectiles();
		}
		return;
	}

	for (TActorIterator<AArenaProjectileLegacyBenchmarkActor> It(GetWorld()); It; ++It)
	{
		AArenaProjectileLegacyBenchmarkActor* Projectile = *It;
		if (Projectile && Projectile->GetOwner() == this)
		{
			Projectile->Destroy();
		}
	}
}

// 每帧只做发射调度和轻量帧时间采样；真正 Data Projectile 移动由 WorldSubsystem 集中执行。
void AArenaProjectileStressTestActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	TRACE_CPUPROFILER_EVENT_SCOPE(ArenaProjectileStressTest);

	if (!bRunning || DeltaTime <= 0.0f)
	{
		return;
	}

	const float FrameMs = DeltaTime * 1000.0f;
	FrameTimeSamplesMs.Add(FrameMs);
	ReportElapsedSeconds += DeltaTime;

	const float SafeLifetime = FMath::Max(ProjectileLifetime, 0.05f);
	const double EffectiveSpawnRate = bDeriveSpawnRateFromTarget
		? static_cast<double>(FMath::Max(TargetActiveProjectiles, 0)) / SafeLifetime
		: FMath::Max(static_cast<double>(SpawnPerSecond), 0.0);

	SpawnAccumulator += EffectiveSpawnRate * DeltaTime;
	const int32 RequestedSpawnCount = FMath::FloorToInt(SpawnAccumulator);
	const int32 SpawnCount = FMath::Clamp(RequestedSpawnCount, 0, FMath::Max(MaxSpawnPerFrame, 1));
	SpawnAccumulator -= SpawnCount;

	if (SpawnCount > 0)
	{
		SpawnBatch(SpawnCount);
	}

	if (ReportElapsedSeconds >= FMath::Max(ReportIntervalSeconds, 0.25f))
	{
		FlushReportWindow();
	}
}

// EndPlay 不保留测试 Projectile，防止 PIE 重启后旧数据影响下一次容量统计。
void AArenaProjectileStressTestActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopStressTest(true);
	Super::EndPlay(EndPlayReason);
}

// 立即结束当前窗口；用于开始/结束 Unreal Insights Capture 前后手动打日志标记。
void AArenaProjectileStressTestActor::ReportNow()
{
	FlushReportWindow();
}

// 每颗 Projectile 使用相同随机序列生成二维方向，保证 Legacy/Data 对照输入一致。
void AArenaProjectileStressTestActor::SpawnBatch(int32 Count)
{
	if (!GetWorld() || Count <= 0)
	{
		return;
	}

	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Angle = SpawnRandom.FRandRange(0.0f, 2.0f * PI);
		const FVector Direction(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
		const FVector RadialOffset = Direction * SpawnRandom.FRandRange(0.0f, FMath::Max(SpawnRadius, 0.0f));
		const FVector SpawnLocation = GetActorLocation() + RadialOffset;
		const FVector Velocity = Direction * FMath::Max(ProjectileSpeed, 0.0f);

		if (StressMode == EArenaProjectileStressMode::DataPool)
		{
			SpawnDataProjectile(SpawnLocation, Velocity);
		}
		else
		{
			SpawnLegacyProjectile(SpawnLocation, Velocity);
		}
	}
}

// DataPool 模式只提交纯数据，不创建 UObject、Component 或 Actor。
void AArenaProjectileStressTestActor::SpawnDataProjectile(
	const FVector& SpawnLocation,
	const FVector& Velocity)
{
	UArenaProjectileSimulationSubsystem* Subsystem = GetProjectileSubsystem();
	if (!Subsystem)
	{
		++FailedSpawnCount;
		return;
	}

	FArenaProjectileSpawnParams Params;
	Params.Position = SpawnLocation;
	Params.Velocity = Velocity;
	Params.Lifetime = FMath::Max(ProjectileLifetime, 0.05f);
	Params.Radius = 8.0f;

	FArenaProjectileHandle Handle;
	if (Subsystem->SpawnProjectile(Params, Handle))
	{
		++SubmittedSpawnCount;
	}
	else
	{
		++FailedSpawnCount;
	}
}

// Legacy 模式生成最小 AActor + Sphere + ProjectileMovement 参考，不接入真实技能或 GAS。
void AArenaProjectileStressTestActor::SpawnLegacyProjectile(
	const FVector& SpawnLocation,
	const FVector& Velocity)
{
	if (!GetWorld())
	{
		++FailedSpawnCount;
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AArenaProjectileLegacyBenchmarkActor* Projectile =
		GetWorld()->SpawnActor<AArenaProjectileLegacyBenchmarkActor>(
			AArenaProjectileLegacyBenchmarkActor::StaticClass(),
			SpawnLocation,
			Velocity.IsNearlyZero() ? FRotator::ZeroRotator : Velocity.Rotation(),
			SpawnParams);

	if (!Projectile)
	{
		++FailedSpawnCount;
		return;
	}

	Projectile->ConfigureBenchmark(
		Velocity,
		FMath::Max(ProjectileLifetime, 0.05f),
		bLegacyEnableMovement,
		bLegacyEnableCollision,
		bLegacyEnableReplication);
	++SubmittedSpawnCount;
}

// DataPool 直接读取 O(1) ActiveCount；Legacy 只在报告窗口遍历一次对应测试 Actor。
int32 AArenaProjectileStressTestActor::CountActiveProjectiles() const
{
	if (!GetWorld())
	{
		return 0;
	}

	if (StressMode == EArenaProjectileStressMode::DataPool)
	{
		const UArenaProjectileSimulationSubsystem* Subsystem = GetProjectileSubsystem();
		return Subsystem ? Subsystem->GetActiveProjectileCount() : 0;
	}

	int32 Count = 0;
	for (TActorIterator<AArenaProjectileLegacyBenchmarkActor> It(GetWorld()); It; ++It)
	{
		const AArenaProjectileLegacyBenchmarkActor* Projectile = *It;
		if (Projectile && Projectile->GetOwner() == this)
		{
			++Count;
		}
	}
	return Count;
}

// 排序当前窗口的帧样本并记录百分位；正式性能结论仍以 Unreal Insights Capture 为准。
void AArenaProjectileStressTestActor::FlushReportWindow()
{
	if (FrameTimeSamplesMs.IsEmpty())
	{
		return;
	}

	TArray<float> SortedSamples = FrameTimeSamplesMs;
	SortedSamples.Sort();

	double SumMs = 0.0;
	float MaxMs = 0.0f;
	for (const float Sample : SortedSamples)
	{
		SumMs += Sample;
		MaxMs = FMath::Max(MaxMs, Sample);
	}

	const int32 SampleCount = SortedSamples.Num();
	const auto Percentile = [&SortedSamples, SampleCount](float Fraction)
	{
		const int32 Index = FMath::Clamp(
			FMath::CeilToInt(Fraction * SampleCount) - 1,
			0,
			SampleCount - 1);
		return SortedSamples[Index];
	};

	const float AverageMs = static_cast<float>(SumMs / SampleCount);
	const float P95Ms = Percentile(0.95f);
	const float P99Ms = Percentile(0.99f);
	const int32 ActiveCount = CountActiveProjectiles();

	FArenaProjectileSimulationStats PoolStats;
	if (const UArenaProjectileSimulationSubsystem* Subsystem = GetProjectileSubsystem())
	{
		PoolStats = Subsystem->GetStats();
	}

	UE_LOG(
		LogArenaProjectile,
		Log,
		TEXT("ProjectileStress Mode=%s Active=%d Target=%d Frames=%d Avg=%.3fms P95=%.3fms P99=%.3fms Max=%.3fms Submitted=%lld Failed=%lld PoolCapacity=%d PoolPeak=%d Overflow=%d."),
		StressMode == EArenaProjectileStressMode::DataPool ? TEXT("DataPool") : TEXT("LegacyActor"),
		ActiveCount,
		TargetActiveProjectiles,
		SampleCount,
		AverageMs,
		P95Ms,
		P99Ms,
		MaxMs,
		SubmittedSpawnCount,
		FailedSpawnCount,
		PoolStats.Capacity,
		PoolStats.PeakActiveCount,
		PoolStats.OverflowCount);

	FrameTimeSamplesMs.Reset();
	ReportElapsedSeconds = 0.0;
}

// 从当前 World 获取唯一 Data Projectile SimulationSubsystem。
UArenaProjectileSimulationSubsystem* AArenaProjectileStressTestActor::GetProjectileSubsystem() const
{
	return GetWorld() ? GetWorld()->GetSubsystem<UArenaProjectileSimulationSubsystem>() : nullptr;
}
