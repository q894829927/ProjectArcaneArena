#include "AI/ArenaEnemyAIController.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/ArenaEnemyCharacter.h"
#include "Core/ArenaGameState.h"
#include "GameFramework/PlayerState.h"
#include "GAS/ArenaGameplayTags.h"

// 使用低频 Tick 驱动最小 Chase/Attack 状态，不引入行为树和黑板。
AArenaEnemyAIController::AArenaEnemyAIController()
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
	SetActorTickEnabled(HasAuthority() && ControlledEnemy.IsValid());
}

void AArenaEnemyAIController::OnUnPossess()
{
	StopCombatMovement();
	ControlledEnemy.Reset();
	CurrentTarget.Reset();
	Super::OnUnPossess();
}

// 在服务器按主攻击距离和实际攻击路径驱动追击；弹道被挡时缩小到达半径并继续绕路。
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
	}

	// 有效目标的攻击前摇期间冻结寻路，避免敌人跟随目标滑动。
	if (Enemy->IsAttacking())
	{
		StopMovement();
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
		const FVector FacingDirection = (Target->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D();
		if (!FacingDirection.IsNearlyZero())
		{
			Enemy->SetActorRotation(FacingDirection.Rotation());
		}
		Enemy->TryActivatePrimaryAttack();
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
		Enemy->CancelPrimaryAttack();
		Enemy->SetCombatTarget(nullptr);
	}
}
