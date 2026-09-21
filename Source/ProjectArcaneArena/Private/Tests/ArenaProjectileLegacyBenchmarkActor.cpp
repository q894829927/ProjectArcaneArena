#include "Tests/ArenaProjectileLegacyBenchmarkActor.h"

#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"

// 创建最小传统 Projectile Actor；默认不移动、不碰撞、不复制，由 StressTest 在生成后显式开启。
AArenaProjectileLegacyBenchmarkActor::AArenaProjectileLegacyBenchmarkActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetReplicateMovement(false);

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	CollisionComponent->InitSphereRadius(8.0f);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollisionComponent->SetGenerateOverlapEvents(false);
	RootComponent = CollisionComponent;

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionComponent;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->bRotationFollowsVelocity = false;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->bAutoActivate = false;
}

// 只切换 P0 需要拆分的成本项；生命周期结束仍走普通 Actor Destroy，保留传统路径特征。
void AArenaProjectileLegacyBenchmarkActor::ConfigureBenchmark(
	const FVector& InVelocity,
	float InLifetime,
	bool bEnableMovement,
	bool bEnableCollision,
	bool bEnableReplication)
{
	SetReplicates(bEnableReplication);
	SetReplicateMovement(bEnableReplication);

	CollisionComponent->SetCollisionEnabled(
		bEnableCollision ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);

	if (bEnableMovement)
	{
		const float Speed = InVelocity.Size();
		ProjectileMovement->SetUpdatedComponent(CollisionComponent);
		ProjectileMovement->InitialSpeed = Speed;
		ProjectileMovement->MaxSpeed = Speed;
		ProjectileMovement->Velocity = InVelocity;
		ProjectileMovement->SetComponentTickEnabled(true);
		ProjectileMovement->Activate(true);
	}
	else
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
		ProjectileMovement->SetComponentTickEnabled(false);
	}

	SetLifeSpan(FMath::Max(InLifetime, 0.05f));
}
