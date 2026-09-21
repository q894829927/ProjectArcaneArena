#include "Projectile/ArenaProjectileSimulationSubsystem.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/ArenaEnemyCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffect.h"
#include "Core/ArenaLogCategories.h"
#include "HAL/IConsoleManager.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Stats/Stats.h"

namespace
{
	TAutoConsoleVariable<int32> CVarArenaProjectileInitialCapacity(
		TEXT("arena.Projectile.InitialCapacity"),
		5000,
		TEXT("Initial slot capacity for the Arena Data Projectile pool."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarArenaProjectileMaxCapacity(
		TEXT("arena.Projectile.MaxCapacity"),
		10000,
		TEXT("Maximum slot capacity for the Arena Data Projectile pool."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarArenaProjectileGrowChunkSize(
		TEXT("arena.Projectile.GrowChunkSize"),
		1024,
		TEXT("Number of slots added when the Arena Data Projectile pool grows."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarArenaProjectileSpatialCellSize(
		TEXT("arena.Projectile.SpatialCellSize"),
		300.0f,
		TEXT("2D spatial hash cell size used by Arena Data Projectile collision."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarArenaProjectileLogHits(
		TEXT("arena.Projectile.LogHits"),
		0,
		TEXT("Log authoritative Data Projectile hit commands when non-zero."),
		ECVF_Default);
}

// 初始化连续数据槽位；调用 Super 保证 UTickableWorldSubsystem 正确启停 Tick。
void UArenaProjectileSimulationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const int32 MaxCapacity = FMath::Max(CVarArenaProjectileMaxCapacity.GetValueOnGameThread(), 1);
	const int32 InitialCapacity = FMath::Clamp(
		CVarArenaProjectileInitialCapacity.GetValueOnGameThread(),
		1,
		MaxCapacity);

	GrowStorage(InitialCapacity);
	ResetStatistics();

	UE_LOG(
		LogArenaProjectile,
		Log,
		TEXT("Data Projectile pool initialized. Capacity=%d MaxCapacity=%d."),
		Positions.Num(),
		MaxCapacity);
}

// 世界销毁时先让所有 Handle 失效，再释放连续数组内存。
void UArenaProjectileSimulationSubsystem::Deinitialize()
{
	ResetAllProjectiles();

	Positions.Empty();
	PreviousPositions.Empty();
	Velocities.Empty();
	Radii.Empty();
	RemainingLife.Empty();
	PierceRemaining.Empty();
	AttackInstanceIDs.Empty();
	WeaponRuntimeIDs.Empty();
	SourceActors.Empty();
	DamageEffectClasses.Empty();
	DamageTypeTags.Empty();
	BaseDamages.Empty();
	SkillMultipliers.Empty();
	Generations.Empty();
	ActiveSlots.Empty();
	ActiveListPositions.Empty();
	FreeSlots.Empty();
	PendingHitCommands.Empty();
	CollisionCandidates.Empty();
	SpatialGrid = FArenaProjectileSpatialGrid();

	Super::Deinitialize();
}

// 只在真实 Game 与 PIE 世界创建，避免编辑器预览和资产世界产生无意义模拟器。
bool UArenaProjectileSimulationSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

// 返回 Tickable 统计 ID，供引擎 Tick/Stat 系统安全采样本 Subsystem。
TStatId UArenaProjectileSimulationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UArenaProjectileSimulationSubsystem, STATGROUP_Tickables);
}

// P3 集中推进 Data Projectile，并在 Authority World 通过 Spatial Hash + Swept Collision 生成命中命令。
void UArenaProjectileSimulationSubsystem::Tick(float DeltaTime)
{
	if (DeltaTime <= 0.0f || ActiveSlots.IsEmpty())
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArenaProjectileSimulation);

	UWorld* World = GetWorld();
	const bool bResolveAuthoritativeHits = World && World->GetNetMode() != NM_Client;
	PendingHitCommands.Reset();

	if (bResolveAuthoritativeHits)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ArenaProjectileSpatialGridBuild);
		SpatialGrid.Rebuild(CVarArenaProjectileSpatialCellSize.GetValueOnGameThread());
	}

	for (int32 ActiveIndex = ActiveSlots.Num() - 1; ActiveIndex >= 0; --ActiveIndex)
	{
		const int32 Slot = ActiveSlots[ActiveIndex];
		PreviousPositions[Slot] = Positions[Slot];
		Positions[Slot] += Velocities[Slot] * DeltaTime;
		RemainingLife[Slot] -= DeltaTime;

		if (bResolveAuthoritativeHits
			&& SourceActors[Slot].IsValid()
			&& DamageEffectClasses[Slot])
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ArenaProjectileSweptCollision);
			FArenaProjectileHitCommand HitCommand;
			if (FindFirstProjectileHit(Slot, HitCommand))
			{
				PendingHitCommands.Add(MoveTemp(HitCommand));
				ReleaseSlotAtActiveIndex(ActiveIndex);
				continue;
			}
		}

		if (RemainingLife[Slot] <= 0.0f)
		{
			ReleaseSlotAtActiveIndex(ActiveIndex);
		}
	}

	if (!PendingHitCommands.IsEmpty())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ArenaProjectileHitCommands);
		ApplyPendingHitCommands();
	}
}

// 校验输入后获取可复用槽位；数据池耗尽会明确计数而不是静默覆盖飞行中的 Projectile。
bool UArenaProjectileSimulationSubsystem::SpawnProjectile(
	const FArenaProjectileSpawnParams& Params,
	FArenaProjectileHandle& OutHandle)
{
	OutHandle.Reset();

	if (Params.Position.ContainsNaN()
		|| Params.Velocity.ContainsNaN()
		|| !FMath::IsFinite(Params.Radius)
		|| !FMath::IsFinite(Params.Lifetime)
		|| Params.Radius < 0.0f
		|| Params.Lifetime <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogArenaProjectile, Warning, TEXT("Rejected invalid Data Projectile spawn parameters."));
		return false;
	}

	if (!EnsureFreeSlot())
	{
		++OverflowCount;
		return false;
	}

	const int32 Slot = FreeSlots.Pop(EAllowShrinking::No);
	check(ActiveListPositions.IsValidIndex(Slot));
	check(ActiveListPositions[Slot] == INDEX_NONE);

	Positions[Slot] = Params.Position;
	PreviousPositions[Slot] = Params.Position;
	Velocities[Slot] = Params.Velocity;
	Radii[Slot] = Params.Radius;
	RemainingLife[Slot] = Params.Lifetime;
	PierceRemaining[Slot] = FMath::Max(Params.PierceRemaining, 0);
	AttackInstanceIDs[Slot] = Params.AttackInstanceID;
	WeaponRuntimeIDs[Slot] = Params.WeaponRuntimeID;
	SourceActors[Slot] = Params.SourceActor;
	DamageEffectClasses[Slot] = Params.DamageEffectClass;
	DamageTypeTags[Slot] = Params.DamageTypeTag;
	BaseDamages[Slot] = FMath::Max(Params.BaseDamage, 0.0f);
	SkillMultipliers[Slot] = FMath::Max(Params.SkillMultiplier, 0.0f);

	ActiveListPositions[Slot] = ActiveSlots.Add(Slot);

	OutHandle.Slot = Slot;
	OutHandle.Generation = Generations[Slot];

	++TotalSpawned;
	PeakActiveCount = FMath::Max(PeakActiveCount, ActiveSlots.Num());
	return true;
}

// 外部释放必须同时匹配 Slot 与 Generation，保证已经过期的 Handle 无副作用。
bool UArenaProjectileSimulationSubsystem::ReleaseProjectile(const FArenaProjectileHandle& Handle)
{
	if (!IsSlotGenerationValid(Handle.Slot, Handle.Generation))
	{
		return false;
	}

	ReleaseSlotAtActiveIndex(ActiveListPositions[Handle.Slot]);
	return true;
}

// Handle 只有仍位于 ActiveSlots 且代次一致时才视为存活。
bool UArenaProjectileSimulationSubsystem::IsProjectileAlive(const FArenaProjectileHandle& Handle) const
{
	return IsSlotGenerationValid(Handle.Slot, Handle.Generation);
}

// 读取表现位置前重复校验代次，避免 VisualSubsystem 读取到同 Slot 的下一发 Projectile。
bool UArenaProjectileSimulationSubsystem::GetProjectilePosition(
	const FArenaProjectileHandle& Handle,
	FVector& OutPosition) const
{
	if (!IsSlotGenerationValid(Handle.Slot, Handle.Generation))
	{
		return false;
	}

	OutPosition = Positions[Handle.Slot];
	return true;
}

// 压力测试切档或世界收尾时一次性回收全部槽位，并递增 Generation 使旧 Handle 失效。
void UArenaProjectileSimulationSubsystem::ResetAllProjectiles()
{
	TotalReleased += ActiveSlots.Num();

	for (const int32 Slot : ActiveSlots)
	{
		if (Generations.IsValidIndex(Slot))
		{
			++Generations[Slot];
			if (Generations[Slot] <= 0)
			{
				Generations[Slot] = 1;
			}
		}
	}

	ActiveSlots.Reset();
	FreeSlots.Reset(Positions.Num());

	for (int32 Slot = 0; Slot < Positions.Num(); ++Slot)
	{
		ActiveListPositions[Slot] = INDEX_NONE;
		Positions[Slot] = FVector::ZeroVector;
		PreviousPositions[Slot] = FVector::ZeroVector;
		Velocities[Slot] = FVector::ZeroVector;
		Radii[Slot] = 0.0f;
		RemainingLife[Slot] = 0.0f;
		PierceRemaining[Slot] = 0;
		AttackInstanceIDs[Slot] = 0;
		WeaponRuntimeIDs[Slot] = INDEX_NONE;
		SourceActors[Slot].Reset();
		DamageEffectClasses[Slot] = nullptr;
		DamageTypeTags[Slot] = FGameplayTag();
		BaseDamages[Slot] = 0.0f;
		SkillMultipliers[Slot] = 1.0f;
	}

	for (int32 Slot = Positions.Num() - 1; Slot >= 0; --Slot)
	{
		FreeSlots.Add(Slot);
	}
}

// 清空 P0/P1 累计统计，保留当前 ActiveCount 作为新的峰值起点。
void UArenaProjectileSimulationSubsystem::ResetStatistics()
{
	PeakActiveCount = ActiveSlots.Num();
	OverflowCount = 0;
	TotalSpawned = 0;
	TotalReleased = 0;
}

// 生成轻量统计快照，供 StressTest 日志和后续 HUD/Telemetry 使用。
FArenaProjectileSimulationStats UArenaProjectileSimulationSubsystem::GetStats() const
{
	FArenaProjectileSimulationStats Stats;
	Stats.Capacity = Positions.Num();
	Stats.ActiveCount = ActiveSlots.Num();
	Stats.FreeCount = FreeSlots.Num();
	Stats.PeakActiveCount = PeakActiveCount;
	Stats.OverflowCount = OverflowCount;
	Stats.TotalSpawned = TotalSpawned;
	Stats.TotalReleased = TotalReleased;
	return Stats;
}

// Free List 耗尽后按 Chunk 扩容，超过 MaxCapacity 时由 SpawnProjectile 记录 Overflow。
bool UArenaProjectileSimulationSubsystem::EnsureFreeSlot()
{
	if (!FreeSlots.IsEmpty())
	{
		return true;
	}

	const int32 CurrentCapacity = Positions.Num();
	const int32 MaxCapacity = FMath::Max(CVarArenaProjectileMaxCapacity.GetValueOnGameThread(), 1);
	if (CurrentCapacity >= MaxCapacity)
	{
		return false;
	}

	const int32 GrowChunkSize = FMath::Max(CVarArenaProjectileGrowChunkSize.GetValueOnGameThread(), 1);
	const int32 NewCapacity = FMath::Min(CurrentCapacity + GrowChunkSize, MaxCapacity);
	GrowStorage(NewCapacity);
	return !FreeSlots.IsEmpty();
}

// 所有热数据数组保持同一 Slot 索引，并只把新增范围加入 Free List。
void UArenaProjectileSimulationSubsystem::GrowStorage(int32 NewCapacity)
{
	const int32 OldCapacity = Positions.Num();
	if (NewCapacity <= OldCapacity)
	{
		return;
	}

	Positions.SetNum(NewCapacity);
	PreviousPositions.SetNum(NewCapacity);
	Velocities.SetNum(NewCapacity);
	Radii.SetNum(NewCapacity);
	RemainingLife.SetNum(NewCapacity);
	PierceRemaining.SetNum(NewCapacity);
	AttackInstanceIDs.SetNum(NewCapacity);
	WeaponRuntimeIDs.SetNum(NewCapacity);
	SourceActors.SetNum(NewCapacity);
	DamageEffectClasses.SetNum(NewCapacity);
	DamageTypeTags.SetNum(NewCapacity);
	BaseDamages.SetNum(NewCapacity);
	SkillMultipliers.SetNum(NewCapacity);
	Generations.SetNum(NewCapacity);
	ActiveListPositions.SetNum(NewCapacity);

	FreeSlots.Reserve(NewCapacity);
	ActiveSlots.Reserve(NewCapacity);

	for (int32 Slot = OldCapacity; Slot < NewCapacity; ++Slot)
	{
		Positions[Slot] = FVector::ZeroVector;
		PreviousPositions[Slot] = FVector::ZeroVector;
		Velocities[Slot] = FVector::ZeroVector;
		Radii[Slot] = 0.0f;
		RemainingLife[Slot] = 0.0f;
		PierceRemaining[Slot] = 0;
		AttackInstanceIDs[Slot] = 0;
		WeaponRuntimeIDs[Slot] = INDEX_NONE;
		SourceActors[Slot].Reset();
		DamageEffectClasses[Slot] = nullptr;
		DamageTypeTags[Slot] = FGameplayTag();
		BaseDamages[Slot] = 0.0f;
		SkillMultipliers[Slot] = 1.0f;
		Generations[Slot] = 1;
		ActiveListPositions[Slot] = INDEX_NONE;
	}

	for (int32 Slot = NewCapacity - 1; Slot >= OldCapacity; --Slot)
	{
		FreeSlots.Add(Slot);
	}
}

// 稠密 ActiveSlots 使用 SwapRemove；被交换进来的 Slot 同步更新 ActiveListPositions。
void UArenaProjectileSimulationSubsystem::ReleaseSlotAtActiveIndex(int32 ActiveIndex)
{
	if (!ActiveSlots.IsValidIndex(ActiveIndex))
	{
		return;
	}

	const int32 Slot = ActiveSlots[ActiveIndex];
	ActiveSlots.RemoveAtSwap(ActiveIndex, 1, EAllowShrinking::No);

	if (ActiveSlots.IsValidIndex(ActiveIndex))
	{
		ActiveListPositions[ActiveSlots[ActiveIndex]] = ActiveIndex;
	}

	ActiveListPositions[Slot] = INDEX_NONE;
	Positions[Slot] = FVector::ZeroVector;
	PreviousPositions[Slot] = FVector::ZeroVector;
	Velocities[Slot] = FVector::ZeroVector;
	Radii[Slot] = 0.0f;
	RemainingLife[Slot] = 0.0f;
	PierceRemaining[Slot] = 0;
	AttackInstanceIDs[Slot] = 0;
	WeaponRuntimeIDs[Slot] = INDEX_NONE;
	SourceActors[Slot].Reset();
	DamageEffectClasses[Slot] = nullptr;
	DamageTypeTags[Slot] = FGameplayTag();
	BaseDamages[Slot] = 0.0f;
	SkillMultipliers[Slot] = 1.0f;

	++Generations[Slot];
	if (Generations[Slot] <= 0)
	{
		Generations[Slot] = 1;
	}

	FreeSlots.Add(Slot);
	++TotalReleased;
}

// 统一校验范围、代次和反向索引，任何一项不匹配都视为旧 Handle。
bool UArenaProjectileSimulationSubsystem::IsSlotGenerationValid(int32 Slot, int32 Generation) const
{
	if (!Generations.IsValidIndex(Slot)
		|| Generation <= 0
		|| Generations[Slot] != Generation
		|| !ActiveListPositions.IsValidIndex(Slot))
	{
		return false;
	}

	const int32 ActiveIndex = ActiveListPositions[Slot];
	return ActiveSlots.IsValidIndex(ActiveIndex) && ActiveSlots[ActiveIndex] == Slot;
}


// Enemy 仅在服务器生命周期内注册一次，Projectile Tick 不再为每颗弹扫描 World Actor。
void UArenaProjectileSimulationSubsystem::RegisterCollisionTarget(AArenaEnemyCharacter* Target)
{
	if (Target && Target->HasAuthority())
	{
		SpatialGrid.RegisterTarget(Target);
	}
}

// Enemy EndPlay 主动注销，避免空间索引持有已经离场的弱引用直到下一帧。
void UArenaProjectileSimulationSubsystem::UnregisterCollisionTarget(AArenaEnemyCharacter* Target)
{
	SpatialGrid.UnregisterTarget(Target);
}

// 宽相候选来自 Spatial Hash；窄相用 Previous→Current 的二维扫掠和 Capsule 高度检查，选择本帧最早命中。
bool UArenaProjectileSimulationSubsystem::FindFirstProjectileHit(
	int32 Slot,
	FArenaProjectileHitCommand& OutCommand) const
{
	if (!Positions.IsValidIndex(Slot)
		|| !PreviousPositions.IsValidIndex(Slot)
		|| !Radii.IsValidIndex(Slot)
		|| !SourceActors.IsValidIndex(Slot)
		|| !DamageEffectClasses.IsValidIndex(Slot)
		|| !SourceActors[Slot].IsValid()
		|| !DamageEffectClasses[Slot])
	{
		return false;
	}

	const FVector Start = PreviousPositions[Slot];
	const FVector End = Positions[Slot];
	const float ProjectileRadius = FMath::Max(Radii[Slot], 0.0f);
	SpatialGrid.QuerySegment(Start, End, ProjectileRadius, CollisionCandidates);
	if (CollisionCandidates.IsEmpty())
	{
		return false;
	}

	const FVector Start2D(Start.X, Start.Y, 0.0f);
	const FVector End2D(End.X, End.Y, 0.0f);
	const FVector Segment2D = End2D - Start2D;
	const float SegmentLengthSquared2D = Segment2D.SizeSquared();

	float BestAlpha = TNumericLimits<float>::Max();
	AArenaEnemyCharacter* BestTarget = nullptr;
	FVector BestImpactPoint = FVector::ZeroVector;
	FVector BestImpactNormal = FVector::ZeroVector;

	for (AArenaEnemyCharacter* Target : CollisionCandidates)
	{
		if (!IsValid(Target))
		{
			continue;
		}

		const UAbilitySystemComponent* TargetASC = Target->GetAbilitySystemComponent();
		if (!TargetASC || TargetASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead))
		{
			continue;
		}

		float CapsuleRadius = 50.0f;
		float CapsuleHalfHeight = 90.0f;
		if (const UCapsuleComponent* Capsule = Target->GetCapsuleComponent())
		{
			CapsuleRadius = FMath::Max(Capsule->GetScaledCapsuleRadius(), 1.0f);
			CapsuleHalfHeight = FMath::Max(Capsule->GetScaledCapsuleHalfHeight(), CapsuleRadius);
		}

		const FVector TargetLocation = Target->GetActorLocation();
		const FVector Target2D(TargetLocation.X, TargetLocation.Y, 0.0f);

		float Alpha = 0.0f;
		if (SegmentLengthSquared2D > KINDA_SMALL_NUMBER)
		{
			Alpha = FMath::Clamp(
				FVector::DotProduct(Target2D - Start2D, Segment2D) / SegmentLengthSquared2D,
				0.0f,
				1.0f);
		}

		const FVector Closest2D = FMath::Lerp(Start2D, End2D, Alpha);
		const float CombinedRadius = CapsuleRadius + ProjectileRadius;
		if (FVector::DistSquared(Closest2D, Target2D) > FMath::Square(CombinedRadius))
		{
			continue;
		}

		const FVector Closest3D = FMath::Lerp(Start, End, Alpha);
		if (FMath::Abs(Closest3D.Z - TargetLocation.Z) > CapsuleHalfHeight + ProjectileRadius)
		{
			continue;
		}

		if (Alpha >= BestAlpha)
		{
			continue;
		}

		BestAlpha = Alpha;
		BestTarget = Target;
		BestImpactPoint = Closest3D;
		BestImpactNormal = BestImpactPoint - TargetLocation;
		BestImpactNormal.Z = 0.0f;
		BestImpactNormal = BestImpactNormal.GetSafeNormal();
		if (BestImpactNormal.IsNearlyZero())
		{
			BestImpactNormal = -Velocities[Slot].GetSafeNormal();
		}
	}

	if (!BestTarget)
	{
		return false;
	}

	UPrimitiveComponent* HitComponent = Cast<UPrimitiveComponent>(BestTarget->GetRootComponent());
	FHitResult HitResult(BestTarget, HitComponent, BestImpactPoint, BestImpactNormal);
	HitResult.TraceStart = Start;
	HitResult.TraceEnd = End;
	HitResult.Time = BestAlpha;
	HitResult.Distance = FVector::Distance(Start, BestImpactPoint);
	HitResult.bBlockingHit = true;

	OutCommand.SourceActor = SourceActors[Slot];
	OutCommand.TargetActor = BestTarget;
	OutCommand.DamageEffectClass = DamageEffectClasses[Slot];
	OutCommand.DamageTypeTag = DamageTypeTags[Slot];
	OutCommand.BaseDamage = BaseDamages[Slot];
	OutCommand.SkillMultiplier = SkillMultipliers[Slot];
	OutCommand.AttackInstanceID = AttackInstanceIDs[Slot];
	OutCommand.WeaponRuntimeID = WeaponRuntimeIDs[Slot];
	OutCommand.HitResult = HitResult;
	return true;
}

// HitCommand 在模拟循环结束后统一消费；伤害仍走 GE_Damage -> ExecCalc_Damage -> AttributeSet，不直接写属性。
void UArenaProjectileSimulationSubsystem::ApplyPendingHitCommands()
{
	for (const FArenaProjectileHitCommand& Command : PendingHitCommands)
	{
		AActor* SourceActor = Command.SourceActor.Get();
		AActor* TargetActor = Command.TargetActor.Get();
		if (!IsValid(SourceActor) || !IsValid(TargetActor) || !Command.DamageEffectClass)
		{
			continue;
		}

		UAbilitySystemComponent* SourceASC =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(SourceActor);
		UAbilitySystemComponent* TargetASC =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
		if (!SourceASC
			|| !TargetASC
			|| !SourceASC->IsOwnerActorAuthoritative()
			|| TargetASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead))
		{
			continue;
		}

		FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
		EffectContext.AddSourceObject(SourceActor);
		EffectContext.AddHitResult(Command.HitResult, true);

		FGameplayEffectSpecHandle DamageSpecHandle =
			SourceASC->MakeOutgoingSpec(Command.DamageEffectClass, 1.0f, EffectContext);
		if (!DamageSpecHandle.IsValid())
		{
			continue;
		}

		FGameplayEffectSpec* DamageSpec = DamageSpecHandle.Data.Get();
		DamageSpec->SetSetByCallerMagnitude(
			ArenaGameplayTags::SetByCaller_Damage_Base,
			FMath::Max(Command.BaseDamage, 0.0f));
		DamageSpec->SetSetByCallerMagnitude(
			ArenaGameplayTags::SetByCaller_Damage_SkillMultiplier,
			FMath::Max(Command.SkillMultiplier, 0.0f));
		if (Command.DamageTypeTag.IsValid())
		{
			DamageSpec->AddDynamicAssetTag(Command.DamageTypeTag);
		}

		TargetASC->ApplyGameplayEffectSpecToSelf(*DamageSpec);

		if (CVarArenaProjectileLogHits.GetValueOnGameThread() != 0)
		{
			UE_LOG(
				LogArenaProjectile,
				Log,
				TEXT("DataProjectile hit. Source=%s Target=%s AttackID=%d WeaponRuntimeID=%d BaseDamage=%.2f."),
				*GetNameSafe(SourceActor),
				*GetNameSafe(TargetActor),
				Command.AttackInstanceID,
				Command.WeaponRuntimeID,
				Command.BaseDamage);
		}
	}

	PendingHitCommands.Reset();
}
