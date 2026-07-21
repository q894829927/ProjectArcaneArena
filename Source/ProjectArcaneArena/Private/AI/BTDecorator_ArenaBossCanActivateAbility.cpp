#include "AI/BTDecorator_ArenaBossCanActivateAbility.h"

#include "AI/ArenaBossAIController.h"
#include "AI/ArenaBossAIHelpers.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/ArenaBossCharacter.h"
#include "Core/ArenaGameState.h"
#include "GAS/ArenaGameplayAbility_EnemyAttackBase.h"
#include "GAS/ArenaGameplayTags.h"

// Decorator 采用节点实例保存上次结果，避免多个 Boss 共享运行时比较状态。
UBTDecorator_ArenaBossCanActivateAbility::UBTDecorator_ArenaBossCanActivateAbility()
{
	NodeName = TEXT("Boss Can Activate Ability");
	bCreateNodeInstance = true;
	TargetActorKey.AddObjectFilter(
		this,
		GET_MEMBER_NAME_CHECKED(UBTDecorator_ArenaBossCanActivateAbility, TargetActorKey),
		AActor::StaticClass());
	TargetActorKey.SelectedKeyName = AArenaBossAIController::TargetActorKeyName;
	FlowAbortMode = EBTFlowAbortMode::LowerPriority;
	bTickIntervals = true;
	INIT_DECORATOR_NODE_NOTIFY_FLAGS();
}

// 节点描述把精确标签直接展示在图上，避免 GroundSlam 分支误填父标签。
FString UBTDecorator_ArenaBossCanActivateAbility::GetStaticDescription() const
{
	return FString::Printf(
		TEXT("%s\nTarget: %s\nAbility: %s"),
		*Super::GetStaticDescription(),
		*TargetActorKey.SelectedKeyName.ToString(),
		*AbilityTag.ToString());
}

// 解析 Blackboard Key 后才允许运行时读取，防止同名但错误类型的 Key 被接受。
void UBTDecorator_ArenaBossCanActivateAbility::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);
	if (UBlackboardData* BlackboardAsset = GetBlackboardAsset())
	{
		TargetActorKey.ResolveSelectedKey(*BlackboardAsset);
	}
	else
	{
		TargetActorKey.InvalidateResolvedKey();
	}
}

// Decorator 复用 Ability CDO 的攻击距离与路径检查，并把最终资格交给 GAS CanActivateAbility。
bool UBTDecorator_ArenaBossCanActivateAbility::CalculateRawConditionValue(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory) const
{
	const AArenaBossAIController* BossController = Cast<AArenaBossAIController>(OwnerComp.GetAIOwner());
	AArenaBossCharacter* Boss = BossController ? Cast<AArenaBossCharacter>(BossController->GetPawn()) : nullptr;
	UBlackboardComponent* BlackboardComponent = OwnerComp.GetBlackboardComponent();
	UAbilitySystemComponent* BossASC = Boss ? Boss->GetAbilitySystemComponent() : nullptr;
	const AArenaGameState* ArenaGameState = Boss && Boss->GetWorld()
		? Boss->GetWorld()->GetGameState<AArenaGameState>()
		: nullptr;
	AActor* TargetActor = BlackboardComponent
		? Cast<AActor>(BlackboardComponent->GetValueAsObject(TargetActorKey.SelectedKeyName))
		: nullptr;

	if (!BossController || !BossController->HasAuthority() || !Boss || !BossASC || !ArenaGameState
		|| ArenaGameState->GetGamePhase() != EArenaGamePhase::Combat
		|| !BossController->CanActivateBossAbilities()
		|| Boss->IsDeadOrStunned()
		|| BossASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Attacking)
		|| BossASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Casting)
		|| !ArenaBossAI::IsLivingCombatTarget(TargetActor))
	{
		return false;
	}

	int32 MatchCount = 0;
	FGameplayAbilitySpec* AbilitySpec = ArenaBossAI::FindUniqueAbilitySpecByTag(BossASC, AbilityTag, MatchCount);
	const UGameplayAbility* AbilityCDO = AbilitySpec ? AbilitySpec->Ability.Get() : nullptr;
	if (!AbilitySpec || !AbilityCDO || !BossASC->AbilityActorInfo.IsValid())
	{
		if (!bLoggedSpecConfigurationError)
		{
			UE_LOG(
				LogArenaBossAI,
				Error,
				TEXT("Boss %s expected exactly one AbilitySpec for tag %s but found %d, or its ActorInfo is invalid."),
				*GetNameSafe(Boss),
				*AbilityTag.ToString(),
				MatchCount);
			bLoggedSpecConfigurationError = true;
		}
		return false;
	}
	bLoggedSpecConfigurationError = false;

	if (const UArenaGameplayAbility_EnemyAttackBase* EnemyAttack =
		Cast<UArenaGameplayAbility_EnemyAttackBase>(AbilityCDO))
	{
		const float AttackRange = FMath::Max(EnemyAttack->GetAttackRange(), 0.0f);
		const float MinimumAttackRange = FMath::Clamp(
			EnemyAttack->GetMinimumAttackRange(),
			0.0f,
			AttackRange);
		const float DistanceSquared = FVector::DistSquared2D(
			Boss->GetActorLocation(),
			TargetActor->GetActorLocation());
		if (DistanceSquared < FMath::Square(MinimumAttackRange)
			|| DistanceSquared > FMath::Square(AttackRange)
			|| !EnemyAttack->HasAttackPathForAI(Boss, TargetActor))
		{
			return false;
		}
	}

	return AbilityCDO->CanActivateAbility(AbilitySpec->Handle, BossASC->AbilityActorInfo.Get());
}

// 首次成为相关节点时缓存真实条件，避免第一次 Tick 产生无意义的重复搜索。
void UBTDecorator_ArenaBossCanActivateAbility::OnBecomeRelevant(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory)
{
	Super::OnBecomeRelevant(OwnerComp, NodeMemory);
	bLastConditionValue = CalculateRawConditionValue(OwnerComp, NodeMemory);
	SetNextTickTime(NodeMemory, FMath::Max(ConditionCheckInterval, 0.05f));
}

// 条件从不可攻击变为可攻击时中断 Chase，从可攻击变为不可攻击时阻止错误进入攻击分支。
void UBTDecorator_ArenaBossCanActivateAbility::TickNode(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory,
	float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
	const bool bCurrentConditionValue = CalculateRawConditionValue(OwnerComp, NodeMemory);
	if (bCurrentConditionValue != bLastConditionValue)
	{
		bLastConditionValue = bCurrentConditionValue;
		OwnerComp.RequestExecution(this);
	}
	SetNextTickTime(NodeMemory, FMath::Max(ConditionCheckInterval, 0.05f));
}
