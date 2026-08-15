#include "Projectile/ArenaEnemyProjectile.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/ArenaPlayerCharacter.h"
#include "Components/SphereComponent.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameplayEffect.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"

// 初始化复制、碰撞、飞行与本地粒子组件；伤害始终只在服务器 overlap 路径结算。
AArenaEnemyProjectile::AArenaEnemyProjectile()
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
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	CollisionComponent->SetGenerateOverlapEvents(true);
	CollisionComponent->SetNotifyRigidBodyCollision(true);
	RootComponent = CollisionComponent;

	CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &AArenaEnemyProjectile::OnProjectileOverlap);
	CollisionComponent->OnComponentHit.AddDynamic(this, &AArenaEnemyProjectile::OnProjectileHit);

	ProjectileVisual = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("ProjectileVisual"));
	ProjectileVisual->SetupAttachment(CollisionComponent);
	ProjectileVisual->SetAutoActivate(false);
	ProjectileVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionComponent;
	ProjectileMovement->InitialSpeed = InitialSpeed;
	ProjectileMovement->MaxSpeed = InitialSpeed;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bIsHomingProjectile = false;
	ProjectileMovement->bShouldBounce = false;

	InitialLifeSpan = ProjectileLifeSpan;
}

// 保存发射瞬间的伤害配置；Projectile 不读取之后变化的 AI 目标或 Ability 状态。
void AArenaEnemyProjectile::InitializeProjectile(
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

// 各端使用同一蓝图默认速度；非专服播放视觉模板，移动结果由服务器复制校正。
void AArenaEnemyProjectile::BeginPlay()
{
	Super::BeginPlay();

	CollisionComponent->SetSphereRadius(SphereRadius, true);
	ProjectileMovement->InitialSpeed = InitialSpeed;
	ProjectileMovement->MaxSpeed = InitialSpeed;
	ProjectileMovement->Velocity = GetActorForwardVector() * InitialSpeed;

	if (GetNetMode() != NM_DedicatedServer && ProjectileEffect)
	{
		ProjectileVisual->SetTemplate(ProjectileEffect);
		ProjectileVisual->Activate(true);
	}

	SetLifeSpan(ProjectileLifeSpan);
}

// 服务器让任意存活玩家拦截 Projectile；敌人或无 ASC Pawn 会被忽略并继续飞行。
void AArenaEnemyProjectile::OnProjectileOverlap(
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
	if (!CanDamagePlayer(OtherActor, TargetASC))
	{
		return;
	}

	ApplyDamageToTarget(TargetASC, SweepResult);
	FinishProjectile();
}

// 世界静态或动态阻挡命中只负责终止飞行，Pawn 伤害由 overlap 路径唯一处理。
void AArenaEnemyProjectile::OnProjectileHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	if (HasAuthority() && !bHasImpacted)
	{
		FinishProjectile();
	}
}

// 只接受项目玩家角色和有效 ASC；无敌不在这里过滤，以便 Dash 玩家仍可挡弹。
bool AArenaEnemyProjectile::CanDamagePlayer(AActor* TargetActor, UAbilitySystemComponent* TargetASC) const
{
	const AArenaPlayerCharacter* PlayerTarget = Cast<AArenaPlayerCharacter>(TargetActor);
	return PlayerTarget
		&& TargetActor != SourceActor.Get()
		&& TargetASC
		&& (!SourceAbilitySystemComponent.IsValid() || TargetASC != SourceAbilitySystemComponent.Get())
		&& !TargetASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead);
}

// 构造物理伤害 Spec 并附带命中信息，Shield、Defense 与无敌统一交给现有伤害管线。
void AArenaEnemyProjectile::ApplyDamageToTarget(UAbilitySystemComponent* TargetASC, const FHitResult& HitResult)
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

// 标记已消费、停止本地运动并由服务器销毁复制 Actor，保证一次 Projectile 只结算一次。
void AArenaEnemyProjectile::FinishProjectile()
{
	if (bHasImpacted)
	{
		return;
	}

	bHasImpacted = true;
	SetActorEnableCollision(false);
	if (ProjectileMovement)
	{
		ProjectileMovement->StopMovementImmediately();
	}
	Destroy();
}
