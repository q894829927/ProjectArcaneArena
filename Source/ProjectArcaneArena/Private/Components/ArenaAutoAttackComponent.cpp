#include "Components/ArenaAutoAttackComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/ArenaEnemyCharacter.h"
#include "Core/ArenaGameState.h"
#include "Core/ArenaLogCategories.h"
#include "Core/ArenaPlayerState.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Projectile/ArenaProjectileSimulationSubsystem.h"
#include "Projectile/ArenaProjectileTypes.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "TimerManager.h"
#include "Weapon/ArenaWeaponLoadoutComponent.h"
#include "Weapon/ArenaWeaponDataAsset.h"

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

	int32 ResolvedRuntimeID = WeaponRuntimeID;
	const UArenaWeaponDataAsset* WeaponDefinition = GetPrimaryWeaponDefinition(ResolvedRuntimeID);
	const TSubclassOf<UGameplayEffect> ResolvedDamageEffect =
		WeaponDefinition ? WeaponDefinition->DamageEffectClass : DamageEffectClass;
	if (!ResolvedDamageEffect)
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
		TEXT("AutoAttack ready. Owner=%s Weapon=%s RuntimeID=%d FireInterval=%.2f Range=%.1f Speed=%.1f Lifetime=%.2f."),
		*GetNameSafe(OwnerActor),
		WeaponDefinition ? *WeaponDefinition->WeaponID.ToString() : TEXT("LegacyInlineConfig"),
		ResolvedRuntimeID,
		GetCurrentFireInterval(),
		GetCurrentTargetRange(),
		WeaponDefinition ? WeaponDefinition->ProjectileSpeed : ProjectileSpeed,
		WeaponDefinition ? WeaponDefinition->ProjectileLifetime : ProjectileLifetime);
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
		ScheduleNextEvaluation(GetCurrentFireInterval());
		return;
	}

	ScheduleNextEvaluation(GetCurrentFireInterval());
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
	const float ResolvedTargetRange = GetCurrentTargetRange();
	if (!OwnerActor || !World || ResolvedTargetRange <= KINDA_SMALL_NUMBER)
	{
		return nullptr;
	}

	const FVector OwnerLocation = OwnerActor->GetActorLocation();
	const float MaxDistanceSquared = FMath::Square(ResolvedTargetRange);
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

	int32 ResolvedWeaponRuntimeID = WeaponRuntimeID;
	const UArenaWeaponDataAsset* WeaponDefinition = GetPrimaryWeaponDefinition(ResolvedWeaponRuntimeID);
	const float ResolvedSpawnHeight = WeaponDefinition ? WeaponDefinition->ProjectileSpawnHeight : ProjectileSpawnHeight;
	const float ResolvedForwardOffset = WeaponDefinition ? WeaponDefinition->ProjectileForwardOffset : ProjectileForwardOffset;
	const float ResolvedProjectileSpeed = WeaponDefinition ? WeaponDefinition->ProjectileSpeed : ProjectileSpeed;
	const float ResolvedProjectileLifetime = WeaponDefinition ? WeaponDefinition->ProjectileLifetime : ProjectileLifetime;
	const float ResolvedProjectileRadius = WeaponDefinition ? WeaponDefinition->ProjectileRadius : ProjectileRadius;
	const TSubclassOf<UGameplayEffect> ResolvedDamageEffect = WeaponDefinition ? WeaponDefinition->DamageEffectClass : DamageEffectClass;
	const FGameplayTag ResolvedDamageType = WeaponDefinition ? WeaponDefinition->DamageTypeTag : DamageTypeTag;
	const float ResolvedBaseDamage = WeaponDefinition ? WeaponDefinition->BaseDamage : BaseDamage;
	const float ResolvedSkillMultiplier = WeaponDefinition ? WeaponDefinition->SkillMultiplier : SkillMultiplier;

	const FVector OriginBase = OwnerActor->GetActorLocation() + FVector::UpVector * ResolvedSpawnHeight;
	const FVector TargetPoint = TargetActor->GetActorLocation() + FVector::UpVector * ResolvedSpawnHeight;
	const FVector ToTarget = TargetPoint - OriginBase;
	const float TargetDistance = ToTarget.Size();
	if (TargetDistance <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector Direction = ToTarget / TargetDistance;
	const float SafeForwardOffset = FMath::Clamp(
		ResolvedForwardOffset,
		0.0f,
		FMath::Max(TargetDistance - 1.0f, 0.0f));

	FArenaProjectileSpawnParams Params;
	Params.Position = OriginBase + Direction * SafeForwardOffset;
	Params.Velocity = Direction * FMath::Max(ResolvedProjectileSpeed, 0.0f);
	Params.Radius = FMath::Max(ResolvedProjectileRadius, 0.0f);
	Params.Lifetime = FMath::Max(ResolvedProjectileLifetime, 0.05f);
	Params.PierceRemaining = 0;
	const int32 AttackInstanceID = AllocateAttackInstanceIDForRuntime(ResolvedWeaponRuntimeID);
	if (AttackInstanceID <= 0)
	{
		return false;
	}
	Params.AttackInstanceID = AttackInstanceID;
	Params.WeaponRuntimeID = ResolvedWeaponRuntimeID;
	Params.SourceActor = OwnerActor;
	Params.DamageEffectClass = ResolvedDamageEffect;
	Params.DamageTypeTag = ResolvedDamageType;
	Params.BaseDamage = ResolvedBaseDamage;
	Params.SkillMultiplier = ResolvedSkillMultiplier;

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
			ResolvedWeaponRuntimeID,
			Handle.Slot,
			Handle.Generation);
	}
	return true;
}

// P4-A/B 当前只把 Slot 0 作为主武器接入现有单 Timer 调度；多槽独立调度在下一子阶段扩展。
const UArenaWeaponDataAsset* UArenaAutoAttackComponent::GetPrimaryWeaponDefinition(int32& OutWeaponRuntimeID) const
{
	OutWeaponRuntimeID = WeaponRuntimeID;
	const UArenaWeaponLoadoutComponent* Loadout = GetWeaponLoadoutComponent();
	const FArenaWeaponRuntime* Runtime = Loadout ? Loadout->FindWeaponRuntimeAtSlot(0) : nullptr;
	if (!Runtime || !Runtime->IsValid())
	{
		return nullptr;
	}

	OutWeaponRuntimeID = Runtime->WeaponRuntimeID;
	return Runtime->WeaponDefinition;
}

// Avatar 只负责调度，装备 Model 始终从 PlayerState 获取，保证未来重生不会丢失本局武器状态。
UArenaWeaponLoadoutComponent* UArenaAutoAttackComponent::GetWeaponLoadoutComponent() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const AArenaPlayerState* PlayerState = OwnerPawn ? OwnerPawn->GetPlayerState<AArenaPlayerState>() : nullptr;
	return PlayerState ? PlayerState->GetWeaponLoadoutComponent() : nullptr;
}

// 调度间隔优先读取 WeaponDataAsset；没有配置新武器资产时保留当前 P3 Blueprint 参数，避免迁移期间中断玩法。
float UArenaAutoAttackComponent::GetCurrentFireInterval() const
{
	int32 ResolvedRuntimeID = WeaponRuntimeID;
	const UArenaWeaponDataAsset* WeaponDefinition = GetPrimaryWeaponDefinition(ResolvedRuntimeID);
	return FMath::Max(WeaponDefinition ? WeaponDefinition->FireInterval : FireInterval, 0.05f);
}

// 索敌半径与武器定义保持同一数据源，后续不同槽位可以拥有独立范围。
float UArenaAutoAttackComponent::GetCurrentTargetRange() const
{
	int32 ResolvedRuntimeID = WeaponRuntimeID;
	const UArenaWeaponDataAsset* WeaponDefinition = GetPrimaryWeaponDefinition(ResolvedRuntimeID);
	return FMath::Max(WeaponDefinition ? WeaponDefinition->TargetRange : TargetRange, 0.0f);
}

// 有有效 WeaponRuntime 时使用 PlayerState Model 中独立计数器；旧关卡未迁移武器资产时继续使用 P2 局部计数器。
int32 UArenaAutoAttackComponent::AllocateAttackInstanceIDForRuntime(int32 ResolvedWeaponRuntimeID)
{
	if (UArenaWeaponLoadoutComponent* Loadout = GetWeaponLoadoutComponent())
	{
		if (ResolvedWeaponRuntimeID > 0)
		{
			const int32 RuntimeAttackID = Loadout->AllocateAttackInstanceID(ResolvedWeaponRuntimeID);
			if (RuntimeAttackID > 0)
			{
				return RuntimeAttackID;
			}
		}
	}

	return AllocateAttackInstanceID();
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
