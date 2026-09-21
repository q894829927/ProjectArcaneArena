#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ArenaProjectileTypes.generated.h"

class AActor;
class UGameplayEffect;

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

	// P5 表现类型只影响共享 Niagara 外观，不参与碰撞、伤害或网络权威。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Projectile|Visual", meta = (ClampMin = "0"))
	int32 VisualTypeID = 0;

	// 同一轮散射共享 AttackInstanceID，但每颗 Pellet 有独立索引与 Projectile Handle。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Projectile|Spread")
	int32 PelletIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Projectile|Spread", meta = (ClampMin = "1"))
	int32 PelletCount = 1;

	// 同一次攻击对同一目标的后续 Pellet 伤害衰减参数；实际倍率在 HitCommand 消费阶段统一计算。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Projectile|Spread", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SameTargetPelletFalloff = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Projectile|Spread", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinPelletDamageMultiplier = 1.0f;

	// 发射者只在 SpawnParams 中短暂持有；SimulationSubsystem 内部转换为 WeakObjectPtr，避免 Projectile 延长 Avatar 生命周期。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Projectile|Damage")
	TObjectPtr<AActor> SourceActor = nullptr;

	// P3 命中后沿现有 GAS Damage Pipeline 创建 Spec；为空时该 Data Projectile 只移动、不参与伤害碰撞。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Projectile|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Projectile|Damage")
	FGameplayTag DamageTypeTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Projectile|Damage", meta = (ClampMin = "0.0"))
	float BaseDamage = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Projectile|Damage", meta = (ClampMin = "0.0"))
	float SkillMultiplier = 1.0f;
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


// P3 在集中模拟阶段生成的命中命令；复制完整结算快照后再回收 Projectile 槽位。
struct PROJECTARCANEARENA_API FArenaProjectileHitCommand
{
	TWeakObjectPtr<AActor> SourceActor;
	TWeakObjectPtr<AActor> TargetActor;
	TSubclassOf<UGameplayEffect> DamageEffectClass;
	FGameplayTag DamageTypeTag;
	float BaseDamage = 0.0f;
	float SkillMultiplier = 1.0f;
	float SameTargetPelletFalloff = 1.0f;
	float MinPelletDamageMultiplier = 1.0f;
	float PelletTrackingLifetime = 0.0f;
	int32 AttackInstanceID = 0;
	int32 WeaponRuntimeID = INDEX_NONE;
	int32 PelletIndex = 0;
	int32 PelletCount = 1;
	int32 ProjectileHitOrdinal = 1;
	int32 PierceRemainingAfterHit = 0;
	FHitResult HitResult;
};


// P5 从 SimulationSubsystem 批量复制给表现层的轻量快照；不包含 GAS/UObject 伤害状态。
struct PROJECTARCANEARENA_API FArenaProjectileVisualSample
{
	FArenaProjectileHandle Handle;
	FVector Position = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	float Radius = 0.0f;
	float RemainingLife = 0.0f;
	int32 VisualTypeID = 0;
	int32 WeaponRuntimeID = INDEX_NONE;
	int32 AttackInstanceID = 0;
	int32 PelletIndex = 0;
	int32 PelletCount = 1;
};

// P5 命中表现事件；权威伤害已经由 HitCommand/GAS 独立完成，这里只给共享 Niagara/NDC 消费。
struct PROJECTARCANEARENA_API FArenaProjectileImpactVisualEvent
{
	FVector Position = FVector::ZeroVector;
	FVector Normal = FVector::UpVector;
	int32 VisualTypeID = 0;
	int32 WeaponRuntimeID = INDEX_NONE;
	int32 AttackInstanceID = 0;
	int32 PelletIndex = 0;
	int32 ProjectileHitOrdinal = 1;
};
