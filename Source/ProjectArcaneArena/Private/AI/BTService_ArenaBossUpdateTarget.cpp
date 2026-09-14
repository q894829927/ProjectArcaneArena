#include "AI/BTService_ArenaBossUpdateTarget.h"

#include "AI/ArenaBossAIController.h"
#include "AI/ArenaBossAIHelpers.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/ArenaBossCharacter.h"
#include "Core/ArenaGameState.h"
#include "GameFramework/PlayerState.h"

// Service 使用唯一 TargetActor Key；同一 Key 同时服务 MoveTo、Focus 和 Ability CombatTarget。
UBTService_ArenaBossUpdateTarget::UBTService_ArenaBossUpdateTarget()
{
	NodeName = TEXT("Update Nearest Living Player");
	TargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_ArenaBossUpdateTarget, TargetActorKey), AActor::StaticClass());
	TargetActorKey.SelectedKeyName = AArenaBossAIController::TargetActorKeyName;
	bCallTickOnSearchStart = true;
	bRestartTimerOnEachActivation = true;
	RandomDeviation = 0.0f;
}

// 让 BlackboardKeySelector 在加载 BT 后解析到确切 Key ID，错误资产不会写入其他键。
void UBTService_ArenaBossUpdateTarget::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);
	if (UBlackboardData* BlackboardAsset = GetBlackboardAsset())
	{
		TargetActorKey.ResolveSelectedKey(*BlackboardAsset);
	}
}

// 每轮保持当前有效目标；攻击期间锁定存活目标，目标失效时先取消攻击再选择替代玩家。
void UBTService_ArenaBossUpdateTarget::TickNode(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory,
	float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
	AArenaBossAIController* BossController = Cast<AArenaBossAIController>(OwnerComp.GetAIOwner());
	AArenaBossCharacter* Boss = BossController ? Cast<AArenaBossCharacter>(BossController->GetPawn()) : nullptr;
	UBlackboardComponent* BlackboardComponent = OwnerComp.GetBlackboardComponent();
	const AArenaGameState* ArenaGameState = Boss && Boss->GetWorld()
		? Boss->GetWorld()->GetGameState<AArenaGameState>()
		: nullptr;
	if (!BossController || !BossController->HasAuthority() || !Boss || !BlackboardComponent || !ArenaGameState
		|| Boss->IsDeadOrStunned() || ArenaGameState->GetGamePhase() != EArenaGamePhase::Combat)
	{
		if (BlackboardComponent)
		{
			BlackboardComponent->ClearValue(TargetActorKey.SelectedKeyName);
		}
		if (Boss)
		{
			Boss->SetCombatTarget(nullptr);
		}
		return;
	}

	AActor* CurrentTarget = Cast<AActor>(BlackboardComponent->GetValueAsObject(TargetActorKey.SelectedKeyName));
	bool bCurrentTargetWasInvalidated = (Boss->IsAttacking() && !CurrentTarget)
		|| (CurrentTarget && !ArenaBossAI::IsLivingCombatTarget(CurrentTarget));
	if (!ArenaBossAI::IsLivingCombatTarget(CurrentTarget))
	{
		CurrentTarget = nullptr;
	}

	AActor* NearestTarget = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();
	bool bCurrentTargetBelongsToPlayerArray = false;
	for (APlayerState* CandidatePlayerState : ArenaGameState->PlayerArray)
	{
		AActor* Candidate = CandidatePlayerState ? CandidatePlayerState->GetPawn() : nullptr;
		if (!ArenaBossAI::IsLivingCombatTarget(Candidate))
		{
			continue;
		}
		bCurrentTargetBelongsToPlayerArray |= Candidate == CurrentTarget;

		const float DistanceSquared = FVector::DistSquared2D(Boss->GetActorLocation(), Candidate->GetActorLocation());
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			NearestTarget = Candidate;
		}
	}
	if (CurrentTarget && !bCurrentTargetBelongsToPlayerArray)
	{
		bCurrentTargetWasInvalidated = true;
		CurrentTarget = nullptr;
	}
	if (bCurrentTargetWasInvalidated && Boss->IsAttacking())
	{
		Boss->CancelPrimaryAttack();
	}

	AActor* SelectedTarget = CurrentTarget;
	if (!SelectedTarget)
	{
		SelectedTarget = NearestTarget;
	}
	else if (!Boss->IsAttacking() && NearestTarget && NearestTarget != SelectedTarget)
	{
		const float CurrentDistance = FVector::Dist2D(Boss->GetActorLocation(), SelectedTarget->GetActorLocation());
		const float NearestDistance = FMath::Sqrt(NearestDistanceSquared);
		if (NearestDistance + FMath::Max(TargetSwitchDistanceAdvantage, 0.0f) < CurrentDistance)
		{
			SelectedTarget = NearestTarget;
		}
	}

	if (SelectedTarget)
	{
		if (SelectedTarget != BlackboardComponent->GetValueAsObject(TargetActorKey.SelectedKeyName))
		{
			BlackboardComponent->SetValueAsObject(TargetActorKey.SelectedKeyName, SelectedTarget);
		}
		Boss->SetCombatTarget(SelectedTarget);
	}
	else
	{
		BlackboardComponent->ClearValue(TargetActorKey.SelectedKeyName);
		Boss->SetCombatTarget(nullptr);
	}
}

// Service 调度只读取实例配置，确保编辑器修改 TargetUpdateInterval 后首轮之外立即生效。
void UBTService_ArenaBossUpdateTarget::ScheduleNextTick(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory)
{
	SetNextTickTime(NodeMemory, FMath::Max(TargetUpdateInterval, 0.05f));
}
