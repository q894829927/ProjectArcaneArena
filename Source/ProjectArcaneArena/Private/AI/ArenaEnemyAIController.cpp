#include "AI/ArenaEnemyAIController.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/ArenaEnemyCharacter.h"
#include "Core/ArenaGameState.h"
#include "GameFramework/PlayerState.h"
#include "GAS/ArenaGameplayTags.h"
#include "NavigationSystem.h"
#include "Navigation/CrowdFollowingComponent.h"

// 使用低频 Tick 驱动最小 Chase/Attack 状态，并让 Detour Crowd 负责共享路径上的动态避让。
AArenaEnemyAIController::AArenaEnemyAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = DecisionInterval;
	bAttachToPawn = true;
}

void AArenaEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	PrimaryActorTick.TickInterval = FMath::Max(DecisionInterval, 0.05f);
	ControlledEnemy = Cast<AArenaEnemyCharacter>(InPawn);
	ConfigureCrowdFollowing();
	ResetCrowdRecovery(InPawn ? InPawn->GetActorLocation() : FVector::ZeroVector);
	bNextRecoveryUsesRightSide = InPawn && (InPawn->GetUniqueID() & 1) != 0;
	SetActorTickEnabled(HasAuthority() && ControlledEnemy.IsValid());
}

void AArenaEnemyAIController::OnUnPossess()
{
	StopCombatMovement();
	ControlledEnemy.Reset();
	CurrentTarget.Reset();
	Super::OnUnPossess();
}

// 在服务器按主攻击距离和实际攻击路径驱动追击；路径拥堵时执行短侧移后继续绕路。
void AArenaEnemyAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	AArenaEnemyCharacter* Enemy = ControlledEnemy.Get();
	const AArenaGameState* ArenaGameState = GetWorld() ? GetWorld()->GetGameState<AArenaGameState>() : nullptr;
	if (!HasAuthority() || !Enemy || Enemy->IsDeadOrStunned()
		|| (ArenaGameState && (ArenaGameState->GetGamePhase() == EArenaGamePhase::Defeat
			|| ArenaGameState->GetGamePhase() == EArenaGamePhase::Victory)))
	{
		StopCombatMovement();
		return;
	}

	// 先处理死亡/失效目标，再判断攻击冻结；否则 State.Attacking 会让 AI 永久守在旧目标旁边。
	if (!IsValidCombatTarget(CurrentTarget.Get()))
	{
		StopMovement();
		ClearFocus(EAIFocusPriority::Gameplay);
		Enemy->SetCombatTarget(nullptr);
		Enemy->CancelPrimaryAttack();

		CurrentTarget = FindNearestLivingPlayer();
		ResetCrowdRecovery(Enemy->GetActorLocation());
	}

	// 有效目标的攻击前摇期间冻结寻路，避免敌人跟随目标滑动。
	if (Enemy->IsAttacking())
	{
		StopMovement();
		ResetCrowdRecovery(Enemy->GetActorLocation());
		return;
	}

	AActor* Target = CurrentTarget.Get();
	if (!Target)
	{
		StopCombatMovement();
		return;
	}

	Enemy->SetCombatTarget(Target);
	SetFocus(Target);
	const float AttackRange = Enemy->GetPrimaryAttackRange();
	const float Distance = FVector::Dist2D(Enemy->GetActorLocation(), Target->GetActorLocation());
	const bool bHasAttackPath = Enemy->HasPrimaryAttackPath(Target);
	if (Distance <= AttackRange && bHasAttackPath)
	{
		StopMovement();
		ResetCrowdRecovery(Enemy->GetActorLocation());
		const FVector FacingDirection = (Target->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D();
		if (!FacingDirection.IsNearlyZero())
		{
			Enemy->SetActorRotation(FacingDirection.Rotation());
		}
		Enemy->TryActivatePrimaryAttack();
		return;
	}

	if (TryRunCrowdRecovery(Enemy, Target, DeltaSeconds))
	{
		return;
	}

	// 无视线时不能继续使用攻击距离作为到达半径，否则位于墙后但已进射程会被误判为寻路完成。
	const float AcceptanceRadius = bHasAttackPath
		? FMath::Max(AttackRange - MoveAcceptancePadding, 0.0f)
		: OccludedMoveAcceptanceRadius;
	MoveToActor(Target, AcceptanceRadius, false, true, true, nullptr, true);
}

// 从 GameState PlayerArray 选择最近的存活 Pawn，兼容未来 Listen Server 双人模式。
AActor* AArenaEnemyAIController::FindNearestLivingPlayer() const
{
	const AArenaEnemyCharacter* Enemy = ControlledEnemy.Get();
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!Enemy || !GameState)
	{
		return nullptr;
	}

	AActor* BestTarget = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	for (APlayerState* CandidatePlayerState : GameState->PlayerArray)
	{
		AActor* Candidate = CandidatePlayerState ? CandidatePlayerState->GetPawn() : nullptr;
		if (!IsValidCombatTarget(Candidate))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared2D(Enemy->GetActorLocation(), Candidate->GetActorLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestTarget = Candidate;
		}
	}

	return BestTarget;
}

bool AArenaEnemyAIController::IsValidCombatTarget(const AActor* Candidate) const
{
	const UAbilitySystemComponent* TargetASC = Candidate
		? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<AActor*>(Candidate))
		: nullptr;
	return IsValid(Candidate)
		&& TargetASC
		&& !TargetASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead);
}

// 清理寻路、主攻击、朝向和临时目标，避免死亡、眩晕或阶段结束后继续释放。
void AArenaEnemyAIController::StopCombatMovement()
{
	StopMovement();
	ClearFocus(EAIFocusPriority::Gameplay);
	if (AArenaEnemyCharacter* Enemy = ControlledEnemy.Get())
	{
		ResetCrowdRecovery(Enemy->GetActorLocation());
		Enemy->CancelPrimaryAttack();
		Enemy->SetCombatTarget(nullptr);
	}
	else
	{
		ResetCrowdRecovery();
	}
}

// 启用障碍避让和邻居分离，使多名敌人能在 NavMesh 走廊内绕开前排并补入攻击位置。
void AArenaEnemyAIController::ConfigureCrowdFollowing()
{
	UCrowdFollowingComponent* CrowdFollowing = Cast<UCrowdFollowingComponent>(GetPathFollowingComponent());
	if (!CrowdFollowing)
	{
		return;
	}

	CrowdFollowing->SetCrowdObstacleAvoidance(true);
	CrowdFollowing->SetCrowdSeparation(true);
	CrowdFollowing->SetCrowdSeparationWeight(FMath::Max(CrowdSeparationWeight, 0.0f));
	CrowdFollowing->SetCrowdCollisionQueryRange(FMath::Max(CrowdCollisionQueryRange, 0.0f));
	CrowdFollowing->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::Good);
}

// 在追击请求持续无速度时选择切线方向的短侧移点，避免多个胶囊争抢同一目标中心而永久死锁。
bool AArenaEnemyAIController::TryRunCrowdRecovery(
	AArenaEnemyCharacter* Enemy,
	const AActor* Target,
	float DeltaSeconds)
{
	UWorld* World = GetWorld();
	if (!Enemy || !Target || !World)
	{
		ResetCrowdRecovery();
		return false;
	}

	const FVector CurrentLocation = Enemy->GetActorLocation();
	const double CurrentTime = World->GetTimeSeconds();
	if (bCrowdRecoveryActive)
	{
		if (CurrentTime >= CrowdRecoveryEndTime)
		{
			ResetCrowdRecovery(CurrentLocation);
			return false;
		}

		const EPathFollowingRequestResult::Type MoveResult = MoveToLocation(
			CrowdRecoveryLocation,
			5.0f,
			true,
			true,
			true,
			false,
			nullptr,
			true);
		if (MoveResult == EPathFollowingRequestResult::Failed
			|| MoveResult == EPathFollowingRequestResult::AlreadyAtGoal)
		{
			ResetCrowdRecovery(CurrentLocation);
			return false;
		}
		return true;
	}

	const float HorizontalSpeed = Enemy->GetVelocity().Size2D();
	if (!bHasCrowdSample)
	{
		LastCrowdSampleLocation = CurrentLocation;
		bHasCrowdSample = true;
	}

	const bool bHasMeaningfulDisplacement = FVector::DistSquared2D(
		CurrentLocation,
		LastCrowdSampleLocation) > FMath::Square(FMath::Max(StuckSpeedThreshold, 0.0f) * FMath::Max(DeltaSeconds, 0.0f));
	LastCrowdSampleLocation = CurrentLocation;
	if (HorizontalSpeed > FMath::Max(StuckSpeedThreshold, 0.0f) || bHasMeaningfulDisplacement)
	{
		StuckAccumulatedTime = 0.0f;
		return false;
	}

	StuckAccumulatedTime += FMath::Max(DeltaSeconds, 0.0f);
	if (StuckAccumulatedTime < FMath::Max(StuckDetectionDuration, 0.1f))
	{
		return false;
	}

	const FVector DirectionToTarget = (Target->GetActorLocation() - CurrentLocation).GetSafeNormal2D();
	if (DirectionToTarget.IsNearlyZero())
	{
		ResetCrowdRecovery(CurrentLocation);
		return false;
	}

	const float SideSign = bNextRecoveryUsesRightSide ? 1.0f : -1.0f;
	bNextRecoveryUsesRightSide = !bNextRecoveryUsesRightSide;
	const FVector SideDirection(-DirectionToTarget.Y * SideSign, DirectionToTarget.X * SideSign, 0.0f);
	const FVector DesiredLocation = CurrentLocation
		+ SideDirection * FMath::Max(StuckSidestepDistance, 10.0f)
		- DirectionToTarget * 25.0f;
	const UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	FNavLocation ProjectedLocation;
	if (!NavigationSystem
		|| !NavigationSystem->ProjectPointToNavigation(
			DesiredLocation,
			ProjectedLocation,
			FVector(100.0f, 100.0f, 200.0f)))
	{
		ResetCrowdRecovery(CurrentLocation);
		return false;
	}

	CrowdRecoveryLocation = ProjectedLocation.Location;
	CrowdRecoveryEndTime = CurrentTime + FMath::Max(StuckSidestepDuration, 0.1f);
	bCrowdRecoveryActive = true;
	StuckAccumulatedTime = 0.0f;
	const EPathFollowingRequestResult::Type MoveResult = MoveToLocation(
		CrowdRecoveryLocation,
		5.0f,
		true,
		true,
		true,
		false,
		nullptr,
		true);
	if (MoveResult == EPathFollowingRequestResult::Failed
		|| MoveResult == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		ResetCrowdRecovery(CurrentLocation);
		return false;
	}
	return true;
}

// 清空当前侧移请求并重新初始化位置采样，使后续停滞判断只观察新的追击阶段。
void AArenaEnemyAIController::ResetCrowdRecovery(const FVector& CurrentLocation)
{
	LastCrowdSampleLocation = CurrentLocation;
	CrowdRecoveryLocation = FVector::ZeroVector;
	StuckAccumulatedTime = 0.0f;
	CrowdRecoveryEndTime = 0.0;
	bHasCrowdSample = !CurrentLocation.IsNearlyZero();
	bCrowdRecoveryActive = false;
}
