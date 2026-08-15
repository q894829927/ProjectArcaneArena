#include "GAS/ArenaGameplayAbility_BossSummonMinions.h"

#include "AbilitySystemComponent.h"
#include "Character/ArenaBossCharacter.h"
#include "Character/ArenaEnemyCharacter.h"
#include "Core/ArenaGameState.h"
#include "Core/ArenaLogCategories.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GAS/ArenaGameplayEffect_BossSummonMinionsCooldown.h"
#include "GAS/ArenaGameplayTags.h"
#include "NavigationSystem.h"

namespace ArenaBossSummon
{
	// 每圈固定使用八个方向，起始偏转避免所有召唤物总是生成在 Boss 正前方。
	constexpr int32 CandidatesPerRing = 8;
	constexpr float InitialAngleOffsetDegrees = 45.0f;
}

// 召唤技能只在 Phase 3 开放，并沿用敌人攻击基类的服务器执行和并行攻击阻断。
UArenaGameplayAbility_BossSummonMinions::UArenaGameplayAbility_BossSummonMinions()
{
	SetAssetTags(FGameplayTagContainer(ArenaGameplayTags::Ability_Enemy_Boss_SummonMinions));
	ActivationRequiredTags.AddTag(ArenaGameplayTags::Boss_Phase_Three);
	ActivationBlockedTags.AddTag(ArenaGameplayTags::Cooldown_Enemy_Boss_SummonMinions);
	ActivationBlockedTags.AddTag(ArenaGameplayTags::State_Casting);
	CooldownGameplayEffectClass = UArenaGameplayEffect_BossSummonMinionsCooldown::StaticClass();
}

// BT 高频资格检查只确认容量和至少一个合法位置，不修改集合、冷却或锁定结果。
bool UArenaGameplayAbility_BossSummonMinions::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const AArenaBossCharacter* SourceBoss = ActorInfo
		? Cast<AArenaBossCharacter>(ActorInfo->AvatarActor.Get())
		: nullptr;
	if (!SourceBoss || !SourceBoss->HasAuthority() || !SourceBoss->CanAcceptBossSummons(SummonsPerCast))
	{
		return false;
	}

	const int32 RequestedCount = FMath::Min(
		FMath::Max(SummonsPerCast, 1),
		SourceBoss->GetRemainingSummonCapacity());
	TArray<FVector> AvailableLocations;
	return FindSummonSpawnLocations(SourceBoss, RequestedCount, AvailableLocations)
		&& !AvailableLocations.IsEmpty();
}

// 激活时重新解析并锁定固定生成点，避免资格检查和 Commit 之间的世界变化产生无位置冷却。
void UArenaGameplayAbility_BossSummonMinions::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	AArenaBossCharacter* SourceBoss = ActorInfo
		? Cast<AArenaBossCharacter>(ActorInfo->AvatarActor.Get())
		: nullptr;
	const AArenaGameState* ArenaGameState = SourceBoss && SourceBoss->GetWorld()
		? SourceBoss->GetWorld()->GetGameState<AArenaGameState>()
		: nullptr;
	if (!SourceBoss
		|| !SourceBoss->HasAuthority()
		|| !ArenaGameState
		|| ArenaGameState->GetGamePhase() != EArenaGamePhase::Combat
		|| !SourceBoss->CanAcceptBossSummons(SummonsPerCast))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const int32 RequestedCount = FMath::Min(
		FMath::Max(SummonsPerCast, 1),
		SourceBoss->GetRemainingSummonCapacity());
	LockedSpawnLocations.Reset();
	if (!FindSummonSpawnLocations(SourceBoss, RequestedCount, LockedSpawnLocations)
		|| LockedSpawnLocations.IsEmpty())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive())
	{
		LockedSpawnLocations.Reset();
		return;
	}

	if (UCharacterMovementComponent* MovementComponent = SourceBoss->GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
	}
	if (AController* Controller = SourceBoss->GetController())
	{
		Controller->StopMovement();
	}

	AddCastingStateTag(ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr);
}

// 所有结束路径都先清除召唤专属锁定数据和 Casting，再由基类销毁 Montage/Delay 任务与 Attacking。
void UArenaGameplayAbility_BossSummonMinions::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	RemoveCastingStateTag(ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr);
	LockedSpawnLocations.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// 蓝图至少要提供一个有效敌人 Class；非法空项被忽略而不会使其他可用类型失效。
bool UArenaGameplayAbility_BossSummonMinions::HasRequiredAttackConfiguration() const
{
	const bool bHasValidSummonClass = SummonClasses.ContainsByPredicate(
		[](const TSubclassOf<AArenaEnemyCharacter>& SummonClass)
		{
			return SummonClass != nullptr;
		});
	return Super::HasRequiredAttackConfiguration()
		&& bHasValidSummonClass
		&& SummonsPerCast > 0
		&& SpawnRadius > KINDA_SMALL_NUMBER
		&& SpawnCandidateCount > 0
		&& SpawnCapsuleRadius > KINDA_SMALL_NUMBER
		&& SpawnCapsuleHalfHeight >= SpawnCapsuleRadius;
}

// 固定前摇兑现时按 Class 配置顺序生成；容量或单点失败只减少本次成功数量，不重复尝试伤害或计数。
void UArenaGameplayAbility_BossSummonMinions::ExecuteAttack(
	AArenaEnemyCharacter* SourceEnemy,
	AActor* TargetActor,
	UAbilitySystemComponent* SourceASC,
	UAbilitySystemComponent* TargetASC)
{
	RemoveCastingStateTag(SourceASC);

	AArenaBossCharacter* SourceBoss = Cast<AArenaBossCharacter>(SourceEnemy);
	UWorld* World = SourceBoss ? SourceBoss->GetWorld() : nullptr;
	if (!SourceBoss || !World || !SourceASC || LockedSpawnLocations.IsEmpty())
	{
		return;
	}

	TArray<TSubclassOf<AArenaEnemyCharacter>> ValidSummonClasses;
	for (const TSubclassOf<AArenaEnemyCharacter>& SummonClass : SummonClasses)
	{
		if (SummonClass)
		{
			ValidSummonClasses.Add(SummonClass);
		}
	}
	if (ValidSummonClasses.IsEmpty())
	{
		return;
	}

	const int32 SpawnLimit = FMath::Min3(
		LockedSpawnLocations.Num(),
		FMath::Max(SummonsPerCast, 1),
		SourceBoss->GetRemainingSummonCapacity());
	int32 SuccessfulSpawnCount = 0;
	for (int32 SpawnIndex = 0; SpawnIndex < SpawnLimit; ++SpawnIndex)
	{
		const TSubclassOf<AArenaEnemyCharacter> SummonClass =
			ValidSummonClasses[SpawnIndex % ValidSummonClasses.Num()];
		const FVector SpawnLocation = LockedSpawnLocations[SpawnIndex];
		const FVector FacingDirection = (SpawnLocation - SourceBoss->GetActorLocation()).GetSafeNormal2D();
		const FRotator SpawnRotation = FacingDirection.IsNearlyZero()
			? SourceBoss->GetActorRotation()
			: FacingDirection.Rotation();

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = SourceBoss;
		SpawnParameters.Instigator = SourceBoss;
		SpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
		AArenaEnemyCharacter* SummonedEnemy = World->SpawnActor<AArenaEnemyCharacter>(
			SummonClass,
			SpawnLocation,
			SpawnRotation,
			SpawnParameters);
		if (!SummonedEnemy)
		{
			UE_LOG(
				LogArenaBoss,
				Warning,
				TEXT("Boss %s failed to spawn summon class %s at %s."),
				*GetNameSafe(SourceBoss),
				*GetNameSafe(SummonClass.Get()),
				*SpawnLocation.ToCompactString());
			continue;
		}

		if (!SourceBoss->RegisterBossSummon(SummonedEnemy))
		{
			SummonedEnemy->Destroy();
			continue;
		}

		++SuccessfulSpawnCount;
		ExecuteSummonSpawnCue(SourceASC, SummonedEnemy, SummonedEnemy->GetActorLocation());
	}

	UE_LOG(
		LogArenaBoss,
		Log,
		TEXT("Boss %s summon cast created %d/%d minion(s); active=%d."),
		*GetNameSafe(SourceBoss),
		SuccessfulSpawnCount,
		SpawnLimit,
		SourceBoss->GetActiveSummonCount());
}

// Cast Cue 由敌人攻击基类在 Commit 成功后执行一次，不会在无合法生成点时误播。
FGameplayTag UArenaGameplayAbility_BossSummonMinions::GetAttackActivationCueTag() const
{
	return ArenaGameplayTags::GameplayCue_Ability_Boss_Summon_Cast;
}

// 固定候选顺序保证同一世界状态下结果稳定；NavMesh 断路与胶囊占位都会跳过当前候选。
bool UArenaGameplayAbility_BossSummonMinions::FindSummonSpawnLocations(
	const AArenaBossCharacter* SourceBoss,
	int32 RequestedCount,
	TArray<FVector>& OutSpawnLocations) const
{
	OutSpawnLocations.Reset();
	UWorld* World = SourceBoss ? SourceBoss->GetWorld() : nullptr;
	UNavigationSystemV1* NavigationSystem = World
		? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World)
		: nullptr;
	if (!SourceBoss || !World || !NavigationSystem || RequestedCount <= 0)
	{
		return false;
	}

	const int32 SafeCandidateCount = FMath::Max(SpawnCandidateCount, RequestedCount);
	const float SafeSpawnRadius = FMath::Max(SpawnRadius, 1.0f);
	const float SafeRingStep = FMath::Max(SpawnCandidateRingStep, 1.0f);
	const float BossYaw = SourceBoss->GetActorRotation().Yaw;
	const FVector BossLocation = SourceBoss->GetActorLocation();
	const FVector QueryExtent(
		FMath::Max(SpawnCapsuleRadius * 2.0f, 100.0f),
		FMath::Max(SpawnCapsuleRadius * 2.0f, 100.0f),
		FMath::Max(SpawnCapsuleHalfHeight * 3.0f, 300.0f));

	for (int32 CandidateIndex = 0;
		CandidateIndex < SafeCandidateCount && OutSpawnLocations.Num() < RequestedCount;
		++CandidateIndex)
	{
		const int32 RingIndex = CandidateIndex / ArenaBossSummon::CandidatesPerRing;
		const int32 DirectionIndex = CandidateIndex % ArenaBossSummon::CandidatesPerRing;
		const float CandidateAngle = BossYaw
			+ ArenaBossSummon::InitialAngleOffsetDegrees
			+ DirectionIndex * (360.0f / ArenaBossSummon::CandidatesPerRing);
		const float CandidateRadius = SafeSpawnRadius + RingIndex * SafeRingStep;
		const FVector CandidateDirection = FRotator(0.0f, CandidateAngle, 0.0f).Vector();
		const FVector CandidateLocation = BossLocation + CandidateDirection * CandidateRadius;

		FNavLocation ProjectedLocation;
		if (!NavigationSystem->ProjectPointToNavigation(
			CandidateLocation,
			ProjectedLocation,
			QueryExtent))
		{
			continue;
		}

		FVector NavigationHitLocation = SourceBoss->GetNavAgentLocation();
		if (UNavigationSystemV1::NavigationRaycast(
			World,
			SourceBoss->GetNavAgentLocation(),
			ProjectedLocation.Location,
			NavigationHitLocation,
			nullptr,
			SourceBoss->GetController()))
		{
			continue;
		}

		const FVector SpawnLocation =
			ProjectedLocation.Location + FVector(0.0f, 0.0f, SpawnCapsuleHalfHeight + 2.0f);
		const bool bTooCloseToLockedPoint = OutSpawnLocations.ContainsByPredicate(
			[this, &SpawnLocation](const FVector& ExistingLocation)
			{
				return FVector::Dist2D(ExistingLocation, SpawnLocation)
					< FMath::Max(SpawnPointSeparation, 1.0f);
			});
		if (!bTooCloseToLockedPoint && IsSummonSpawnLocationClear(SourceBoss, SpawnLocation))
		{
			OutSpawnLocations.Add(SpawnLocation);
		}
	}

	return !OutSpawnLocations.IsEmpty();
}

// 使用 Pawn 碰撞通道验证完整胶囊空间，忽略来源 Boss 但保留玩家、敌人和世界阻挡。
bool UArenaGameplayAbility_BossSummonMinions::IsSummonSpawnLocationClear(
	const AArenaBossCharacter* SourceBoss,
	const FVector& SpawnLocation) const
{
	UWorld* World = SourceBoss ? SourceBoss->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BossSummonSpawnClearance), false, SourceBoss);
	QueryParams.AddIgnoredActor(SourceBoss);
	const FCollisionShape SpawnShape = FCollisionShape::MakeCapsule(
		FMath::Max(SpawnCapsuleRadius, 1.0f),
		FMath::Max(SpawnCapsuleHalfHeight, SpawnCapsuleRadius));
	if (World->OverlapBlockingTestByChannel(
		SpawnLocation,
		FQuat::Identity,
		ECC_Pawn,
		SpawnShape,
		QueryParams))
	{
		return false;
	}

	TArray<FOverlapResult> PawnOverlaps;
	FCollisionObjectQueryParams PawnObjectQuery;
	PawnObjectQuery.AddObjectTypesToQuery(ECC_Pawn);
	World->OverlapMultiByObjectType(
		PawnOverlaps,
		SpawnLocation,
		FQuat::Identity,
		PawnObjectQuery,
		SpawnShape,
		QueryParams);
	return PawnOverlaps.IsEmpty();
}

// State.Casting 与 Attacking 分工表示前摇和完整攻击生命周期，并同步给观察客户端。
void UArenaGameplayAbility_BossSummonMinions::AddCastingStateTag(UAbilitySystemComponent* SourceASC)
{
	if (!SourceASC || bAppliedCastingStateTag)
	{
		return;
	}

	SourceASC->AddLooseGameplayTag(ArenaGameplayTags::State_Casting);
	SourceASC->AddReplicatedLooseGameplayTag(ArenaGameplayTags::State_Casting);
	bAppliedCastingStateTag = true;
}

// 仅在本 Ability 实际添加过 Casting 时移除一次，避免影响其他系统拥有的标签计数。
void UArenaGameplayAbility_BossSummonMinions::RemoveCastingStateTag(UAbilitySystemComponent* SourceASC)
{
	if (!SourceASC || !bAppliedCastingStateTag)
	{
		bAppliedCastingStateTag = false;
		return;
	}

	SourceASC->RemoveLooseGameplayTag(ArenaGameplayTags::State_Casting);
	SourceASC->RemoveReplicatedLooseGameplayTag(ArenaGameplayTags::State_Casting);
	bAppliedCastingStateTag = false;
}

// 每个成功注册的服务器召唤物独立触发一次 Spawn Cue，失败或被容量拒绝的 Actor 不播放。
void UArenaGameplayAbility_BossSummonMinions::ExecuteSummonSpawnCue(
	UAbilitySystemComponent* SourceASC,
	AArenaEnemyCharacter* SummonedEnemy,
	const FVector& SpawnLocation) const
{
	if (!SourceASC || !SummonedEnemy)
	{
		return;
	}

	FGameplayCueParameters CueParameters;
	CueParameters.Instigator = GetAvatarActorFromActorInfo();
	CueParameters.EffectCauser = SummonedEnemy;
	CueParameters.Location = SpawnLocation;
	CueParameters.Normal = FVector::UpVector;
	SourceASC->ExecuteGameplayCue(
		ArenaGameplayTags::GameplayCue_Ability_Boss_Summon_Spawn,
		CueParameters);
}
