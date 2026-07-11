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

// 服务器周期性执行 Idle/Chase/Attack 三态决策。
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

	// 攻击 Ability 自己持有目标并驱动 Montage/命中；AI 只冻结寻路，避免前摇期间追着目标滑动。
	if (Enemy->IsAttacking())
	{
		StopMovement();
		return;
	}

	if (!IsValidCombatTarget(CurrentTarget.Get()))
	{
		CurrentTarget = FindNearestLivingPlayer();
	}

	AActor* Target = CurrentTarget.Get();
	if (!Target)
	{
		StopCombatMovement();
		return;
	}

	Enemy->SetCombatTarget(Target);
	SetFocus(Target);
	const float AttackRange = Enemy->GetMeleeAttackRange();
	const float Distance = FVector::Dist2D(Enemy->GetActorLocation(), Target->GetActorLocation());
	if (Distance <= AttackRange && LineOfSightTo(Target))
	{
		StopMovement();
		const FVector FacingDirection = (Target->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D();
		if (!FacingDirection.IsNearlyZero())
		{
			Enemy->SetActorRotation(FacingDirection.Rotation());
		}
		Enemy->TryActivateMeleeAttack();
		return;
	}

	// 攻击距离按双方 Actor 中心计算，因此寻路到达判定不能额外叠加胶囊体半径。
	MoveToActor(Target, FMath::Max(AttackRange - MoveAcceptancePadding, 0.0f), false, true, true, nullptr, true);
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

// 清理寻路、朝向和 Enemy 上的临时目标，避免死亡或眩晕后继续攻击。
void AArenaEnemyAIController::StopCombatMovement()
{
	StopMovement();
	ClearFocus(EAIFocusPriority::Gameplay);
	if (AArenaEnemyCharacter* Enemy = ControlledEnemy.Get())
	{
		Enemy->SetCombatTarget(nullptr);
	}
}
