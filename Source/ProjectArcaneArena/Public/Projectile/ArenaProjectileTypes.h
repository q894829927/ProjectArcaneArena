#pragma once

#include "CoreMinimal.h"
#include "ArenaProjectileTypes.generated.h"

// Data Projectile 的稳定句柄；Slot 可以复用，但 Generation 必须匹配当前代次。
USTRUCT(BlueprintType)
struct PROJECTARCANEARENA_API FArenaProjectileHandle
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Projectile")
	int32 Slot = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Projectile")
	int32 Generation = 0;

	// 判断句柄是否具备可查询的基础格式，是否仍存活需由 SimulationSubsystem 再校验。
	bool IsValid() const
	{
		return Slot != INDEX_NONE && Generation > 0;
	}

	// 清空句柄，避免调用方继续使用已经释放的槽位。
	void Reset()
	{
		Slot = INDEX_NONE;
		Generation = 0;
	}
};

// 一次普通 Data Projectile 发射所需的最小运行参数，后续碰撞/GAS 阶段可继续扩展冷数据引用。
USTRUCT(BlueprintType)
struct PROJECTARCANEARENA_API FArenaProjectileSpawnParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Projectile")
	FVector Position = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Projectile")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Projectile", meta = (ClampMin = "0.0"))
	float Radius = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Projectile", meta = (ClampMin = "0.001"))
	float Lifetime = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Projectile")
	int32 PierceRemaining = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Projectile")
	int32 AttackInstanceID = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Projectile")
	int32 WeaponRuntimeID = INDEX_NONE;
};

// P0/P1 性能与容量统计快照；只描述数据池运行状态，不承担玩法逻辑。
USTRUCT(BlueprintType)
struct PROJECTARCANEARENA_API FArenaProjectileSimulationStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Projectile")
	int32 Capacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Projectile")
	int32 ActiveCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Projectile")
	int32 FreeCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Projectile")
	int32 PeakActiveCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Projectile")
	int32 OverflowCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Projectile")
	int64 TotalSpawned = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Projectile")
	int64 TotalReleased = 0;
};
