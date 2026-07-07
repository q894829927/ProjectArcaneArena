#include "Projectile/ArenaFireballProjectile.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameplayEffect.h"

AArenaFireballProjectile::AArenaFireballProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	CollisionComponent->InitSphereRadius(SphereRadius);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollisionComponent->SetGenerateOverlapEvents(true);
	CollisionComponent->SetNotifyRigidBodyCollision(true);
	RootComponent = CollisionComponent;

	CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &AArenaFireballProjectile::OnProjectileOverlap);
	CollisionComponent->OnComponentHit.AddDynamic(this, &AArenaFireballProjectile::OnProjectileHit);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionComponent;
	ProjectileMovement->InitialSpeed = InitialSpeed;
	ProjectileMovement->MaxSpeed = InitialSpeed;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;

	InitialLifeSpan = ProjectileLifeSpan;
}

void AArenaFireballProjectile::InitializeProjectile(
	UAbilitySystemComponent* InSourceASC,
	AActor* InSourceActor,
	TSubclassOf<UGameplayEffect> InDamageEffectClass,
	FGameplayTag InDamageTypeTag,
	float InBaseDamage,
	float InSkillMultiplier)
{
	SourceAbilitySystemComponent = InSourceASC;
	SourceActor = InSourceActor;
	DamageEffectClass = InDamageEffectClass;
	DamageTypeTag = InDamageTypeTag;
	BaseDamage = FMath::Max(InBaseDamage, 0.0f);
	SkillMultiplier = FMath::Max(InSkillMultiplier, 0.0f);
}

void AArenaFireballProjectile::BeginPlay()
{
	Super::BeginPlay();

	CollisionComponent->SetSphereRadius(SphereRadius, true);

	ProjectileMovement->InitialSpeed = InitialSpeed;
	ProjectileMovement->MaxSpeed = InitialSpeed;
	ProjectileMovement->Velocity = GetActorForwardVector() * InitialSpeed;

	SetLifeSpan(ProjectileLifeSpan);
}

void AArenaFireballProjectile::OnProjectileOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!HasAuthority() || bHasImpacted || !OtherActor)
	{
		return;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor);
	if (!CanDamageTarget(OtherActor, TargetASC))
	{
		return;
	}

	ApplyDamageToTarget(TargetASC, SweepResult);
	FinishProjectile();
}

void AArenaFireballProjectile::OnProjectileHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	if (!HasAuthority() || bHasImpacted)
	{
		return;
	}

	// 非 Pawn 阻挡视为撞墙，Pawn 命中由 overlap 路径负责服务端伤害。
	FinishProjectile();
}

bool AArenaFireballProjectile::CanDamageTarget(AActor* TargetActor, UAbilitySystemComponent* TargetASC) const
{
	if (!TargetActor || TargetActor == this || TargetActor == SourceActor.Get() || !TargetASC)
	{
		return false;
	}

	if (SourceAbilitySystemComponent.IsValid() && TargetASC == SourceAbilitySystemComponent.Get())
	{
		return false;
	}

	if (TargetASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead))
	{
		return false;
	}

	return true;
}

void AArenaFireballProjectile::ApplyDamageToTarget(UAbilitySystemComponent* TargetASC, const FHitResult& HitResult)
{
	UAbilitySystemComponent* SourceASC = SourceAbilitySystemComponent.Get();
	if (!SourceASC || !TargetASC || !DamageEffectClass)
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);
	EffectContext.AddInstigator(SourceActor.Get(), this);
	EffectContext.AddHitResult(HitResult);

	FGameplayEffectSpecHandle DamageSpecHandle = SourceASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, EffectContext);
	if (!DamageSpecHandle.IsValid())
	{
		return;
	}

	FGameplayEffectSpec* DamageSpec = DamageSpecHandle.Data.Get();
	DamageSpec->SetSetByCallerMagnitude(ArenaGameplayTags::SetByCaller_Damage_Base, BaseDamage);
	DamageSpec->SetSetByCallerMagnitude(ArenaGameplayTags::SetByCaller_Damage_SkillMultiplier, SkillMultiplier);

	if (DamageTypeTag.IsValid())
	{
		DamageSpec->AddDynamicAssetTag(DamageTypeTag);
	}

	SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpec, TargetASC);
}

void AArenaFireballProjectile::FinishProjectile()
{
	if (bHasImpacted)
	{
		return;
	}

	bHasImpacted = true;
	SetActorEnableCollision(false);
	Destroy();
}
