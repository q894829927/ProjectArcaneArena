#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaProjectileStressTestActor.generated.h"

class UArenaProjectileSimulationSubsystem;

UENUM(BlueprintType)
enum class EArenaProjectileStressMode : uint8
{
	DataPool UMETA(DisplayName = "Data Pool"),
	LegacyActor UMETA(DisplayName = "Legacy Actor")
};

// P0/P1 独立压力入口；以相同发射率和寿命对比传统 Actor 与 Data Projectile Pool。
UCLASS(Blueprintable)
class PROJECTARCANEARENA_API AArenaProjectileStressTestActor : public AActor
{
	GENERATED_BODY()

public:
	AArenaProjectileStressTestActor();

	// 清空本轮统计并开始持续发射；DataPool 模式会重置数据池以保证基线可重复。
	UFUNCTION(BlueprintCallable, Category = "Arena|ProjectileStress")
	void StartStressTest();

	// 停止继续发射；可选清理本压力 Actor 创建的数据状态。
	UFUNCTION(BlueprintCallable, Category = "Arena|ProjectileStress")
	void StopStressTest(bool bCleanupProjectiles = true);

	// 立即输出当前窗口与 Projectile Pool 快照，便于配合 Unreal Insights 标记采样时点。
	UFUNCTION(BlueprintCallable, Category = "Arena|ProjectileStress")
	void ReportNow();

protected:
	// 自动启动压力场景；AuthorityOnly 可避免多人 PIE 每端重复创建逻辑 Projectile。
	virtual void BeginPlay() override;

	// 按固定发射率提交 Projectile，并采集当前统计窗口帧时间样本。
	virtual void Tick(float DeltaTime) override;

	// 世界退出前停止压力测试并清理 DataPool 测试状态。
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|ProjectileStress")
	EArenaProjectileStressMode StressMode = EArenaProjectileStressMode::DataPool;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|ProjectileStress")
	bool bAutoStart = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|ProjectileStress")
	bool bRunOnAuthorityOnly = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|ProjectileStress", meta = (ClampMin = "1", ClampMax = "5000"))
	int32 TargetActiveProjectiles = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|ProjectileStress")
	bool bDeriveSpawnRateFromTarget = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|ProjectileStress", meta = (ClampMin = "0.0"))
	float SpawnPerSecond = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|ProjectileStress", meta = (ClampMin = "0.05"))
	float ProjectileLifetime = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|ProjectileStress", meta = (ClampMin = "0.0"))
	float ProjectileSpeed = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|ProjectileStress", meta = (ClampMin = "0.0"))
	float SpawnRadius = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|ProjectileStress", meta = (ClampMin = "1"))
	int32 MaxSpawnPerFrame = 512;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|ProjectileStress")
	bool bPrefillTargetOnStart = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|ProjectileStress|Legacy")
	bool bLegacyEnableMovement = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|ProjectileStress|Legacy")
	bool bLegacyEnableCollision = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|ProjectileStress|Legacy")
	bool bLegacyEnableReplication = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|ProjectileStress|Reporting", meta = (ClampMin = "0.25"))
	float ReportIntervalSeconds = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|ProjectileStress|Reporting")
	int32 RandomSeed = 1337;

private:
	// 生成一批同配置 Projectile；Legacy 与 DataPool 使用同一随机方向序列。
	void SpawnBatch(int32 Count);

	// 提交一个 Data Projectile 槽位，失败由 Subsystem OverflowCount 记录。
	void SpawnDataProjectile(const FVector& SpawnLocation, const FVector& Velocity);

	// 生成一个传统 Actor Projectile，用于 P0 对照 Spawn/Destroy 与组件成本。
	void SpawnLegacyProjectile(const FVector& SpawnLocation, const FVector& Velocity);

	// 计算当前模式真实 Active 数量；Legacy 只在报告窗口遍历一次 World。
	int32 CountActiveProjectiles() const;

	// 结束当前采样窗口并输出 Average/P95/P99/Max 与容量统计。
	void FlushReportWindow();

	UArenaProjectileSimulationSubsystem* GetProjectileSubsystem() const;

	bool bRunning = false;
	double SpawnAccumulator = 0.0;
	double ReportElapsedSeconds = 0.0;
	int64 SubmittedSpawnCount = 0;
	int64 FailedSpawnCount = 0;
	FRandomStream SpawnRandom;
	TArray<float> FrameTimeSamplesMs;
};
