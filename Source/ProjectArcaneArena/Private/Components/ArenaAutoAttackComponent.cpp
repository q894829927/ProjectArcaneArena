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
#include "Weapon/ArenaWeaponDataAsset.h"
#include "Weapon/ArenaWeaponLoadoutComponent.h"

// 创建不 Tick 的服务器自动攻击组件；P4 仍只使用一个 Timer，但可调度多个独立 WeaponRuntime。
UArenaAutoAttackComponent::UArenaAutoAttackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
	DamageTypeTag = ArenaGameplayTags::Damage_Physical;
}

// Authority Avatar 进入世界后开始评估；默认武器会在 PlayerState Loadout 可用后幂等播种一次。
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
	NextFireTimeByRuntime.Reset();
	bDefaultWeaponSeedAttempted = false;

	TrySeedDefaultWeaponRuntimes();
	ScheduleNextEvaluation(RetryInterval);

	UArenaWeaponLoadoutComponent* Loadout = GetWeaponLoadoutComponent();
	const TArray<FArenaWeaponRuntime> Runtimes = Loadout ? Loadout->GetWeaponRuntimes() : TArray<FArenaWeaponRuntime>();
	bool bLoggedRuntime = false;
	for (const FArenaWeaponRuntime& Runtime : Runtimes)
	{
		const UArenaWeaponDataAsset* WeaponDefinition = Runtime.WeaponDefinition;
		if (!Runtime.IsValid() || !WeaponDefinition)
		{
			continue;
		}

		bLoggedRuntime = true;
		if (!WeaponDefinition->DamageEffectClass)
		{
			UE_LOG(
				LogArenaProjectile,
				Warning,
				TEXT("AutoAttack weapon %s RuntimeID=%d has no DamageEffectClass; projectiles can move but cannot resolve GAS damage."),
				*WeaponDefinition->WeaponID.ToString(),
				Runtime.WeaponRuntimeID);
		}

		UE_LOG(
			LogArenaProjectile,
			Log,
			TEXT("AutoAttack weapon ready. Owner=%s Slot=%d Weapon=%s RuntimeID=%d FireInterval=%.2f Range=%.1f Speed=%.1f Lifetime=%.2f."),
			*GetNameSafe(OwnerActor),
			Runtime.SlotIndex,
			*WeaponDefinition->WeaponID.ToString(),
			Runtime.WeaponRuntimeID,
			WeaponDefinition->FireInterval,
			WeaponDefinition->TargetRange,
			WeaponDefinition->ProjectileSpeed,
			WeaponDefinition->ProjectileLifetime);
	}

	if (!bLoggedRuntime)
	{
		if (!DamageEffectClass)
		{
			UE_LOG(
				LogArenaProjectile,
				Warning,
				TEXT("AutoAttack legacy DamageEffectClass is not configured on %s; Data Projectiles will move but cannot resolve GAS damage."),
				*GetNameSafe(OwnerActor));
		}

		UE_LOG(
			LogArenaProjectile,
			Log,
			TEXT("AutoAttack legacy config ready. Owner=%s RuntimeID=%d FireInterval=%.2f Range=%.1f Speed=%.1f Lifetime=%.2f."),
			*GetNameSafe(OwnerActor),
			WeaponRuntimeID,
			FireInterval,
			TargetRange,
			ProjectileSpeed,
			ProjectileLifetime);
	}
}

// Avatar 离开世界时终止唯一调度 Timer，并丢弃 Avatar 本地的 Runtime 时间表。
void UArenaAutoAttackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EvaluationTimerHandle);
	}

	NextFireTimeByRuntime.Reset();
	LastFiredTarget.Reset();
	Super::EndPlay(EndPlayReason);
}

// 显式启停自动武器；重新开启时重新建立 Runtime 调度时间，不立即在当前调用栈 Spawn。
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
	NextFireTimeByRuntime.Reset();
	LastFiredTarget.Reset();

	if (bAutoAttackEnabled)
	{
		ScheduleNextEvaluation(RetryInterval);
	}
}

// 单个 Timer 评估全部 WeaponRuntime；每把武器拥有自己的 NextFireTime、TargetRange、FireInterval 与 AttackInstanceID。
void UArenaAutoAttackComponent::EvaluateAutoAttack()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArenaAutoAttackEvaluate);
	TrySeedDefaultWeaponRuntimes();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!bAutoAttackEnabled || !CanAutoFire())
	{
		NextFireTimeByRuntime.Reset();
		LastFiredTarget.Reset();
		ScheduleNextEvaluation(RetryInterval);
		return;
	}

	UArenaWeaponLoadoutComponent* Loadout = GetWeaponLoadoutComponent();
	const TArray<FArenaWeaponRuntime> Runtimes = Loadout ? Loadout->GetWeaponRuntimes() : TArray<FArenaWeaponRuntime>();
	const double NowSeconds = static_cast<double>(World->GetTimeSeconds());
	double EarliestNextFireTime = TNumericLimits<double>::Max();
	TSet<int32> LiveRuntimeIDs;
	bool bHasValidRuntime = false;

	for (const FArenaWeaponRuntime& Runtime : Runtimes)
	{
		const UArenaWeaponDataAsset* WeaponDefinition = Runtime.WeaponDefinition;
		if (!Runtime.IsValid() || !WeaponDefinition)
		{
			continue;
		}

		bHasValidRuntime = true;
		LiveRuntimeIDs.Add(Runtime.WeaponRuntimeID);

		double* NextFireTime = NextFireTimeByRuntime.Find(Runtime.WeaponRuntimeID);
		if (!NextFireTime)
		{
			NextFireTime = &NextFireTimeByRuntime.Add(Runtime.WeaponRuntimeID, NowSeconds);
		}

		if (*NextFireTime <= NowSeconds + KINDA_SMALL_NUMBER)
		{
			AActor* TargetActor = FindNearestLivingEnemy(WeaponDefinition->TargetRange);
			if (TargetActor && FireAtTarget(
				TargetActor,
				Runtime.SlotIndex,
				Runtime.WeaponRuntimeID,
				WeaponDefinition))
			{
				LastFiredTarget = TargetActor;
				++TotalShotsFired;
				*NextFireTime = NowSeconds + FMath::Max(
					static_cast<double>(WeaponDefinition->FireInterval),
					0.05);
			}
			else
			{
				// 无目标时快速重试；有目标但 Spawn 失败时也保持有界重试，不在同帧忙循环。
				*NextFireTime = NowSeconds + FMath::Max(static_cast<double>(RetryInterval), 0.01);
			}
		}

		EarliestNextFireTime = FMath::Min(EarliestNextFireTime, *NextFireTime);
	}

	// 换装或卸装后移除旧 Runtime 的 Avatar 本地调度状态，新的 RuntimeID 会独立从“可立即开火”开始。
	for (auto It = NextFireTimeByRuntime.CreateIterator(); It; ++It)
	{
		if (!LiveRuntimeIDs.Contains(It.Key()))
		{
			It.RemoveCurrent();
		}
	}

	if (bHasValidRuntime)
	{
		const double DelaySeconds = EarliestNextFireTime == TNumericLimits<double>::Max()
			? static_cast<double>(RetryInterval)
			: FMath::Max(EarliestNextFireTime - NowSeconds, 0.01);
		ScheduleNextEvaluation(static_cast<float>(DelaySeconds));
		return;
	}

	// 未迁移 WeaponDataAsset 的旧关卡继续使用 P3 Inline Config，避免 P4 迁移阻塞现有测试内容。
	AActor* LegacyTarget = FindNearestLivingEnemy(TargetRange);
	if (!LegacyTarget)
	{
		LastFiredTarget.Reset();
		ScheduleNextEvaluation(RetryInterval);
		return;
	}

	if (FireAtTarget(LegacyTarget, INDEX_NONE, WeaponRuntimeID, nullptr))
	{
		LastFiredTarget = LegacyTarget;
		++TotalShotsFired;
	}

	ScheduleNextEvaluation(FMath::Max(FireInterval, 0.05f));
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

// 每个 WeaponRuntime 用自己的 TargetRange 做低频最近敌人查询；高频 Projectile 碰撞仍由 P3 Spatial Hash 承担。
AActor* UArenaAutoAttackComponent::FindNearestLivingEnemy(float InTargetRange) const
{
	const AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	const float ResolvedTargetRange = FMath::Max(InTargetRange, 0.0f);
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

// 根据指定 WeaponRuntime 生成一轮 Data Projectile；同一轮所有 Pellet 共享 AttackInstanceID，只拥有不同 Handle 与方向。
bool UArenaAutoAttackComponent::FireAtTarget(
	AActor* TargetActor,
	int32 SlotIndex,
	int32 ResolvedWeaponRuntimeID,
	const UArenaWeaponDataAsset* WeaponDefinition)
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

	const float ResolvedSpawnHeight = WeaponDefinition ? WeaponDefinition->ProjectileSpawnHeight : ProjectileSpawnHeight;
	const float ResolvedForwardOffset = WeaponDefinition ? WeaponDefinition->ProjectileForwardOffset : ProjectileForwardOffset;
	const float ResolvedProjectileSpeed = WeaponDefinition ? WeaponDefinition->ProjectileSpeed : ProjectileSpeed;
	const float ResolvedProjectileLifetime = WeaponDefinition ? WeaponDefinition->ProjectileLifetime : ProjectileLifetime;
	const float ResolvedProjectileRadius = WeaponDefinition ? WeaponDefinition->ProjectileRadius : ProjectileRadius;
	const TSubclassOf<UGameplayEffect> ResolvedDamageEffect = WeaponDefinition ? WeaponDefinition->DamageEffectClass : DamageEffectClass;
	const FGameplayTag ResolvedDamageType = WeaponDefinition ? WeaponDefinition->DamageTypeTag : DamageTypeTag;
	const float ResolvedBaseDamage = WeaponDefinition ? WeaponDefinition->BaseDamage : BaseDamage;
	const float ResolvedSkillMultiplier = WeaponDefinition ? WeaponDefinition->SkillMultiplier : SkillMultiplier;
	const int32 PelletCount = FMath::Clamp(WeaponDefinition ? WeaponDefinition->ProjectilesPerAttack : 1, 1, 64);
	const float SpreadDegrees = FMath::Clamp(WeaponDefinition ? WeaponDefinition->SpreadAngleDegrees : 0.0f, 0.0f, 360.0f);
	const float PelletFalloff = FMath::Clamp(WeaponDefinition ? WeaponDefinition->SameTargetPelletFalloff : 1.0f, 0.0f, 1.0f);
	const float MinPelletMultiplier = FMath::Clamp(WeaponDefinition ? WeaponDefinition->MinPelletDamageMultiplier : 1.0f, 0.0f, 1.0f);
	const int32 ResolvedPierceCount = FMath::Clamp(WeaponDefinition ? WeaponDefinition->PierceCount : 0, 0, 64);
	const int32 ResolvedVisualTypeID = FMath::Max(WeaponDefinition ? WeaponDefinition->ProjectileVisualTypeID : 0, 0);

	const FVector OriginBase = OwnerActor->GetActorLocation() + FVector::UpVector * ResolvedSpawnHeight;
	const FVector TargetPoint = TargetActor->GetActorLocation() + FVector::UpVector * ResolvedSpawnHeight;
	const FVector ToTarget = TargetPoint - OriginBase;
	const float TargetDistance = ToTarget.Size();
	if (TargetDistance <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector AimDirection = ToTarget / TargetDistance;
	const int32 AttackInstanceID = AllocateAttackInstanceIDForRuntime(ResolvedWeaponRuntimeID);
	if (AttackInstanceID <= 0)
	{
		return false;
	}

	int32 SpawnedPelletCount = 0;
	for (int32 PelletIndex = 0; PelletIndex < PelletCount; ++PelletIndex)
	{
		float PelletAngleDegrees = 0.0f;
		if (PelletCount > 1 && SpreadDegrees > KINDA_SMALL_NUMBER)
		{
			const float NormalizedIndex = static_cast<float>(PelletIndex) / static_cast<float>(PelletCount - 1);
			PelletAngleDegrees = FMath::Lerp(-SpreadDegrees * 0.5f, SpreadDegrees * 0.5f, NormalizedIndex);
		}

		const FVector PelletDirection = AimDirection.RotateAngleAxis(PelletAngleDegrees, FVector::UpVector).GetSafeNormal();
		if (PelletDirection.IsNearlyZero())
		{
			continue;
		}

		const float SafeForwardOffset = FMath::Clamp(
			ResolvedForwardOffset,
			0.0f,
			FMath::Max(TargetDistance - 1.0f, 0.0f));

		FArenaProjectileSpawnParams Params;
		Params.Position = OriginBase + PelletDirection * SafeForwardOffset;
		Params.Velocity = PelletDirection * FMath::Max(ResolvedProjectileSpeed, 0.0f);
		Params.Radius = FMath::Max(ResolvedProjectileRadius, 0.0f);
		Params.Lifetime = FMath::Max(ResolvedProjectileLifetime, 0.05f);
		// PierceCount 表示首个命中后还能继续穿过的额外目标数；SimulationSubsystem 负责沿直线顺序结算与目标去重。
		Params.PierceRemaining = ResolvedPierceCount;
		Params.AttackInstanceID = AttackInstanceID;
		Params.WeaponRuntimeID = ResolvedWeaponRuntimeID;
		Params.VisualTypeID = ResolvedVisualTypeID;
		Params.PelletIndex = PelletIndex;
		Params.PelletCount = PelletCount;
		Params.SameTargetPelletFalloff = PelletFalloff;
		Params.MinPelletDamageMultiplier = MinPelletMultiplier;
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
				TEXT("AutoAttack pellet spawn failed. Owner=%s Slot=%d RuntimeID=%d AttackID=%d Pellet=%d/%d Target=%s."),
				*GetNameSafe(OwnerActor),
				SlotIndex,
				ResolvedWeaponRuntimeID,
				AttackInstanceID,
				PelletIndex + 1,
				PelletCount,
				*GetNameSafe(TargetActor));
			continue;
		}

		++SpawnedPelletCount;
		if (bLogSuccessfulShots)
		{
			const FString WeaponLabel = WeaponDefinition
				? WeaponDefinition->WeaponID.ToString()
				: TEXT("LegacyInlineConfig");
			UE_LOG(
				LogArenaProjectile,
				Log,
				TEXT("AutoAttack fired. Owner=%s Slot=%d Weapon=%s Target=%s AttackID=%d WeaponRuntimeID=%d Pellet=%d/%d Angle=%.2f Pierce=%d Handle=%d:%d."),
				*GetNameSafe(OwnerActor),
				SlotIndex,
				*WeaponLabel,
				*GetNameSafe(TargetActor),
				AttackInstanceID,
				ResolvedWeaponRuntimeID,
				PelletIndex + 1,
				PelletCount,
				PelletAngleDegrees,
				ResolvedPierceCount,
				Handle.Slot,
				Handle.Generation);
		}
	}

	if (SpawnedPelletCount <= 0)
	{
		return false;
	}

	LastAttackInstanceID = AttackInstanceID;
	return true;
}

// 默认武器数组索引直接映射 Loadout Slot；只在首次拿到 PlayerState Loadout 时播种，主动卸装后不会自动补回。
void UArenaAutoAttackComponent::TrySeedDefaultWeaponRuntimes()
{
	if (bDefaultWeaponSeedAttempted)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	UArenaWeaponLoadoutComponent* Loadout = GetWeaponLoadoutComponent();
	if (!Loadout)
	{
		return;
	}

	bDefaultWeaponSeedAttempted = true;
	const int32 WeaponCount = FMath::Min(DefaultWeaponDefinitions.Num(), Loadout->GetMaxWeaponSlots());
	for (int32 SlotIndex = 0; SlotIndex < WeaponCount; ++SlotIndex)
	{
		UArenaWeaponDataAsset* WeaponDefinition = DefaultWeaponDefinitions[SlotIndex];
		if (!WeaponDefinition)
		{
			continue;
		}

		const FArenaWeaponRuntime* ExistingRuntime = Loadout->FindWeaponRuntimeAtSlot(SlotIndex);
		if (!ExistingRuntime || !ExistingRuntime->IsValid())
		{
			Loadout->EquipWeapon(SlotIndex, WeaponDefinition);
		}
	}
}

// Avatar 只负责调度，装备 Model 始终从 PlayerState 获取，保证未来重生不会丢失本局武器状态。
UArenaWeaponLoadoutComponent* UArenaAutoAttackComponent::GetWeaponLoadoutComponent() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const AArenaPlayerState* PlayerState = OwnerPawn ? OwnerPawn->GetPlayerState<AArenaPlayerState>() : nullptr;
	return PlayerState ? PlayerState->GetWeaponLoadoutComponent() : nullptr;
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

// 兼容旧 Inline Config 的局部 AttackInstanceID；正常 P4 Runtime 不再共享该计数器。
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
