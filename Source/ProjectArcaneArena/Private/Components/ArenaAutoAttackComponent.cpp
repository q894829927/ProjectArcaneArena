#include "Components/ArenaAutoAttackComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/ArenaEnemyCharacter.h"
#include "Core/ArenaGameState.h"
#include "Core/ArenaLogCategories.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameFramework/Actor.h"
#include "Projectile/ArenaProjectileSimulationSubsystem.h"
#include "Projectile/ArenaProjectileTypes.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "TimerManager.h"

// 创建不 Tick 的服务器自动攻击组件，实际调度由 TimerManager 驱动。
UArenaAutoAttackComponent::UArenaAutoAttackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
	DamageTypeTag = ArenaGameplayTags::Damage_Physical;
}

// Authority Avatar 进入世界后开始评估；客户端仅保留组件供调试/资产结构一致性。
void UArenaAutoAttackComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !bAutoAttackEnabled)
	{
		return;
	}

	TotalShotsFired = 0;
	LastAttackInstanceID = 0;
	NextAttackInstanceID = 1;
	ScheduleNextEvaluation(RetryInterval);

	if (!DamageEffectClass)
	{
		UE_LOG(
			LogArenaProjectile,
			Warning,
			TEXT("AutoAttack DamageEffectClass is not configured on %s; Data Projectiles will move but cannot resolve P3 damage hits."),
			*GetNameSafe(OwnerActor));
	}

	UE_LOG(
		LogArenaProjectile,
		Log,
		TEXT("AutoAttack ready. Owner=%s FireInterval=%.2f Range=%.1f Speed=%.1f Lifetime=%.2f."),
		*GetNameSafe(OwnerActor),
		FireInterval,
		TargetRange,
		ProjectileSpeed,
		ProjectileLifetime);
}

// Avatar 离开世界时终止定时器，保证重生/换 Pawn 后旧实例不会继续发射。
void UArenaAutoAttackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EvaluationTimerHandle);
	}

	LastFiredTarget.Reset();
	Super::EndPlay(EndPlayReason);
}

// 显式切换原型武器；重新开启时从下一次短重试开始，不在调用栈内立即 Spawn。
void UArenaAutoAttackComponent::SetAutoAttackEnabled(bool bEnabled)
{
	if (bAutoAttackEnabled == bEnabled)
	{
		return;
	}

	bAutoAttackEnabled = bEnabled;
	UWorld* World = GetWorld();
	AActor* OwnerActor = GetOwner();
	if (!World || !OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	World->GetTimerManager().ClearTimer(EvaluationTimerHandle);
	LastFiredTarget.Reset();

	if (bAutoAttackEnabled)
	{
		ScheduleNextEvaluation(RetryInterval);
	}
}

// 服务器每次只评估一轮攻击；失败条件采用 RetryInterval，成功发射后按 FireInterval 调度。
void UArenaAutoAttackComponent::EvaluateAutoAttack()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArenaAutoAttackEvaluate);

	if (!bAutoAttackEnabled || !CanAutoFire())
	{
		LastFiredTarget.Reset();
		ScheduleNextEvaluation(RetryInterval);
		return;
	}

	AActor* TargetActor = FindNearestLivingEnemy();
	if (!TargetActor)
	{
		LastFiredTarget.Reset();
		ScheduleNextEvaluation(RetryInterval);
		return;
	}

	if (FireAtTarget(TargetActor))
	{
		LastFiredTarget = TargetActor;
		++TotalShotsFired;
		ScheduleNextEvaluation(FireInterval);
		return;
	}

	ScheduleNextEvaluation(FireInterval);
}

// 自动武器只在服务器 Combat 阶段且玩家未死亡/眩晕时拥有发射权限。
bool UArenaAutoAttackComponent::CanAutoFire() const
{
	const AActor* OwnerActor = GetOwner();
	const UWorld* World = GetWorld();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !World)
	{
		return false;
	}

	const AArenaGameState* ArenaGameState = World->GetGameState<AArenaGameState>();
	if (!ArenaGameState || ArenaGameState->GetGamePhase() != EArenaGamePhase::Combat)
	{
		return false;
	}

	const UAbilitySystemComponent* OwnerASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<AActor*>(OwnerActor));
	if (!OwnerASC)
	{
		return false;
	}

	return !OwnerASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead)
		&& !OwnerASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Stunned);
}

// 自动选敌保持低频最近敌人遍历；P3 的 Spatial Hash 优先解决高频 Projectile×Enemy 碰撞路径。
AActor* UArenaAutoAttackComponent::FindNearestLivingEnemy() const
{
	const AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	if (!OwnerActor || !World || TargetRange <= KINDA_SMALL_NUMBER)
	{
		return nullptr;
	}

	const FVector OwnerLocation = OwnerActor->GetActorLocation();
	const float MaxDistanceSquared = FMath::Square(TargetRange);
	float BestDistanceSquared = MaxDistanceSquared;
	AArenaEnemyCharacter* BestTarget = nullptr;

	for (TActorIterator<AArenaEnemyCharacter> It(World); It; ++It)
	{
		AArenaEnemyCharacter* Enemy = *It;
		if (!IsValid(Enemy))
		{
			continue;
		}

		const UAbilitySystemComponent* EnemyASC = Enemy->GetAbilitySystemComponent();
		if (!EnemyASC || EnemyASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared2D(OwnerLocation, Enemy->GetActorLocation());
		if (DistanceSquared <= BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestTarget = Enemy;
		}
	}

	return BestTarget;
}

// 计算当前目标方向并写入 Data Pool；P3 同时快照伤害配置，由 SimulationSubsystem 命中后统一结算。
bool UArenaAutoAttackComponent::FireAtTarget(AActor* TargetActor)
{
	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	if (!OwnerActor || !World || !IsValid(TargetActor))
	{
		return false;
	}

	UArenaProjectileSimulationSubsystem* ProjectileSubsystem =
		World->GetSubsystem<UArenaProjectileSimulationSubsystem>();
	if (!ProjectileSubsystem)
	{
		return false;
	}

	const FVector OriginBase = OwnerActor->GetActorLocation() + FVector::UpVector * ProjectileSpawnHeight;
	const FVector TargetPoint = TargetActor->GetActorLocation() + FVector::UpVector * ProjectileSpawnHeight;
	const FVector ToTarget = TargetPoint - OriginBase;
	const float TargetDistance = ToTarget.Size();
	if (TargetDistance <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector Direction = ToTarget / TargetDistance;
	const float SafeForwardOffset = FMath::Clamp(
		ProjectileForwardOffset,
		0.0f,
		FMath::Max(TargetDistance - 1.0f, 0.0f));

	FArenaProjectileSpawnParams Params;
	Params.Position = OriginBase + Direction * SafeForwardOffset;
	Params.Velocity = Direction * FMath::Max(ProjectileSpeed, 0.0f);
	Params.Radius = FMath::Max(ProjectileRadius, 0.0f);
	Params.Lifetime = FMath::Max(ProjectileLifetime, 0.05f);
	Params.PierceRemaining = 0;
	const int32 AttackInstanceID = AllocateAttackInstanceID();
	Params.AttackInstanceID = AttackInstanceID;
	Params.WeaponRuntimeID = WeaponRuntimeID;
	Params.SourceActor = OwnerActor;
	Params.DamageEffectClass = DamageEffectClass;
	Params.DamageTypeTag = DamageTypeTag;
	Params.BaseDamage = BaseDamage;
	Params.SkillMultiplier = SkillMultiplier;

	FArenaProjectileHandle Handle;
	if (!ProjectileSubsystem->SpawnProjectile(Params, Handle))
	{
		UE_LOG(
			LogArenaProjectile,
			Warning,
			TEXT("AutoAttack Data Projectile spawn failed. Owner=%s Target=%s."),
			*GetNameSafe(OwnerActor),
			*GetNameSafe(TargetActor));
		return false;
	}

	LastAttackInstanceID = AttackInstanceID;
	if (bLogSuccessfulShots)
	{
		UE_LOG(
			LogArenaProjectile,
			Log,
			TEXT("AutoAttack fired. Owner=%s Target=%s AttackID=%d WeaponRuntimeID=%d Handle=%d:%d."),
			*GetNameSafe(OwnerActor),
			*GetNameSafe(TargetActor),
			AttackInstanceID,
			WeaponRuntimeID,
			Handle.Slot,
			Handle.Generation);
	}
	return true;
}

// Timer 调度统一做最小正值保护，避免错误配置产生同帧递归或零间隔忙循环。
void UArenaAutoAttackComponent::ScheduleNextEvaluation(float DelaySeconds)
{
	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	if (!bAutoAttackEnabled || !OwnerActor || !OwnerActor->HasAuthority() || !World)
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		EvaluationTimerHandle,
		this,
		&UArenaAutoAttackComponent::EvaluateAutoAttack,
		FMath::Max(DelaySeconds, 0.01f),
		false);
}

// AttackInstanceID 在单个 AutoAttackComponent/WeaponRuntime 内单调递增，0 不作为有效攻击轮次。
int32 UArenaAutoAttackComponent::AllocateAttackInstanceID()
{
	const int32 AllocatedID = NextAttackInstanceID;
	++NextAttackInstanceID;
	if (NextAttackInstanceID <= 0)
	{
		NextAttackInstanceID = 1;
	}
	return AllocatedID;
}
