#include "GAS/ArenaGameplayAbility_EnemyRangedAttack.h"

#include "AbilitySystemComponent.h"
#include "Character/ArenaEnemyCharacter.h"
#include "Character/ArenaPlayerCharacter.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GAS/ArenaGameplayEffect_EnemyRangedCooldown.h"
#include "GAS/ArenaGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "Projectile/ArenaEnemyProjectile.h"

// 初始化远程攻击标签和原生冷却后备类，蓝图仍可覆盖所有可调配置。
UArenaGameplayAbility_EnemyRangedAttack::UArenaGameplayAbility_EnemyRangedAttack()
{
	SetAssetTags(FGameplayTagContainer(ArenaGameplayTags::Ability_Enemy_RangedAttack));
	ActivationBlockedTags.AddTag(ArenaGameplayTags::Cooldown_Enemy_RangedAttack);
	CooldownGameplayEffectClass = UArenaGameplayEffect_EnemyRangedCooldown::StaticClass();
}

// 在 Commit 前确保动画、伤害与 Projectile 配置完整，避免进入无法释放的攻击状态。
bool UArenaGameplayAbility_EnemyRangedAttack::HasRequiredAttackConfiguration() const
{
	return Super::HasRequiredAttackConfiguration()
		&& DamageEffectClass != nullptr
		&& ProjectileClass != nullptr;
}

// 从实际发射点到目标瞄准点执行 Projectile 尺寸 Sweep，玩家拦截只在飞行阶段处理。
bool UArenaGameplayAbility_EnemyRangedAttack::HasAttackLineOfSight(
	AArenaEnemyCharacter* SourceEnemy,
	AActor* TargetActor) const
{
	UWorld* World = SourceEnemy ? SourceEnemy->GetWorld() : nullptr;
	if (!World || !TargetActor)
	{
		return false;
	}

	const FVector TraceStart = SourceEnemy->GetActorTransform().TransformPositionNoScale(ProjectileSpawnOffset);
	const FVector TraceEnd = TargetActor->GetActorLocation() + TargetAimOffset;
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyRangedAttackLineOfSight), false, SourceEnemy);
	for (AArenaPlayerCharacter* PlayerCharacter : TActorRange<AArenaPlayerCharacter>(World))
	{
		QueryParams.AddIgnoredActor(PlayerCharacter);
	}
	const FCollisionShape ProjectileShape = FCollisionShape::MakeSphere(
		FMath::Max(ProjectilePathTraceRadius, 1.0f));
	return !World->SweepTestByObjectType(
		TraceStart,
		TraceEnd,
		FQuat::Identity,
		ObjectQueryParams,
		ProjectileShape,
		QueryParams);
}

// 使用释放瞬间的目标位置生成一次直线 Projectile，生成后不再追踪或修正方向。
void UArenaGameplayAbility_EnemyRangedAttack::ExecuteAttack(
	AArenaEnemyCharacter* SourceEnemy,
	AActor* TargetActor,
	UAbilitySystemComponent* SourceASC,
	UAbilitySystemComponent* TargetASC)
{
	UWorld* World = SourceEnemy ? SourceEnemy->GetWorld() : nullptr;
	if (!World || !SourceEnemy->HasAuthority() || !TargetActor || !SourceASC || !ProjectileClass || !DamageEffectClass)
	{
		return;
	}

	const FVector SpawnLocation = SourceEnemy->GetActorTransform().TransformPositionNoScale(ProjectileSpawnOffset);
	const FVector AimLocation = TargetActor->GetActorLocation() + TargetAimOffset;
	const FVector LaunchDirection = (AimLocation - SpawnLocation).GetSafeNormal();
	if (LaunchDirection.IsNearlyZero())
	{
		return;
	}

	const FTransform SpawnTransform(LaunchDirection.Rotation(), SpawnLocation);
	AArenaEnemyProjectile* Projectile = World->SpawnActorDeferred<AArenaEnemyProjectile>(
		ProjectileClass,
		SpawnTransform,
		SourceEnemy,
		SourceEnemy,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Projectile)
	{
		return;
	}

	Projectile->InitializeProjectile(
		SourceASC,
		SourceEnemy,
		DamageEffectClass,
		ArenaGameplayTags::Damage_Physical,
		BaseDamage,
		SkillMultiplier);
	UGameplayStatics::FinishSpawningActor(Projectile, SpawnTransform);
}
