#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaProjectileLegacyBenchmarkActor.generated.h"

class UProjectileMovementComponent;
class USphereComponent;

// P0 专用传统 Actor Projectile 参考实现，用于对比 Actor/Movement/Collision/Replication 的基础成本。
UCLASS(NotBlueprintable, NotPlaceable)
class PROJECTARCANEARENA_API AArenaProjectileLegacyBenchmarkActor : public AActor
{
	GENERATED_BODY()

public:
	AArenaProjectileLegacyBenchmarkActor();

	// 按压力 Actor 的开关配置本次生命周期，不承载真实伤害或 GAS 逻辑。
	void ConfigureBenchmark(
		const FVector& InVelocity,
		float InLifetime,
		bool bEnableMovement,
		bool bEnableCollision,
		bool bEnableReplication);

private:
	UPROPERTY(VisibleAnywhere, Category = "Arena|ProjectileBenchmark")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, Category = "Arena|ProjectileBenchmark")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;
};
