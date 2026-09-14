#include "AI/BTTask_ArenaBossActivateAbility.h"

#include "AI/ArenaBossAIController.h"
#include "AI/ArenaBossAIHelpers.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/ArenaBossCharacter.h"

// Task 必须实例化，确保每个 Boss 独立保存 ASC、Spec Handle 与结束委托。
UBTTask_ArenaBossActivateAbility::UBTTask_ArenaBossActivateAbility()
{
	NodeName = TEXT("Boss Activate Ability");
	bCreateNodeInstance = true;
	bNotifyTaskFinished = true;
	TargetActorKey.AddObjectFilter(
		this,
		GET_MEMBER_NAME_CHECKED(UBTTask_ArenaBossActivateAbility, TargetActorKey),
		AActor::StaticClass());
	TargetActorKey.SelectedKeyName = AArenaBossAIController::TargetActorKeyName;
}

// 节点描述展示精确配置，便于确认 Decorator 与 Task 使用同一 AbilityTag。
FString UBTTask_ArenaBossActivateAbility::GetStaticDescription() const
{
	return FString::Printf(
		TEXT("%s\nTarget: %s\nAbility: %s"),
		*Super::GetStaticDescription(),
		*TargetActorKey.SelectedKeyName.ToString(),
		*AbilityTag.ToString());
}

// 解析 TargetActor Key，错误 Blackboard 资产会让 ExecuteTask 安全失败。
void UBTTask_ArenaBossActivateAbility::InitializeFromAsset(UBehaviorTree& Asset)
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

// 激活前同步目标和朝向，之后只等待当前 Spec Handle 的结束结果。
EBTNodeResult::Type UBTTask_ArenaBossActivateAbility::ExecuteTask(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory)
{
	ResetRuntimeState(false);
	AArenaBossAIController* BossController = Cast<AArenaBossAIController>(OwnerComp.GetAIOwner());
	AArenaBossCharacter* Boss = BossController ? Cast<AArenaBossCharacter>(BossController->GetPawn()) : nullptr;
	UBlackboardComponent* BlackboardComponent = OwnerComp.GetBlackboardComponent();
	UAbilitySystemComponent* BossASC = Boss ? Boss->GetAbilitySystemComponent() : nullptr;
	AActor* TargetActor = BlackboardComponent
		? Cast<AActor>(BlackboardComponent->GetValueAsObject(TargetActorKey.SelectedKeyName))
		: nullptr;
	if (!BossController || !BossController->HasAuthority() || !Boss || !BossASC
		|| !ArenaBossAI::IsLivingCombatTarget(TargetActor))
	{
		return EBTNodeResult::Failed;
	}

	int32 MatchCount = 0;
	FGameplayAbilitySpec* AbilitySpec = ArenaBossAI::FindUniqueAbilitySpecByTag(BossASC, AbilityTag, MatchCount);
	if (!AbilitySpec)
	{
		UE_LOG(
			LogArenaBossAI,
			Error,
			TEXT("Boss %s expected exactly one AbilitySpec for tag %s but found %d."),
			*GetNameSafe(Boss),
			*AbilityTag.ToString(),
			MatchCount);
		return EBTNodeResult::Failed;
	}

	Boss->SetCombatTarget(TargetActor);
	BossController->StopMovement();
	BossController->SetFocus(TargetActor, EAIFocusPriority::Gameplay);
	FVector HorizontalDirection = TargetActor->GetActorLocation() - Boss->GetActorLocation();
	HorizontalDirection.Z = 0.0f;
	if (!HorizontalDirection.IsNearlyZero())
	{
		Boss->SetActorRotation(HorizontalDirection.Rotation());
	}

	ActiveAbilitySystemComponent = BossASC;
	ActiveBehaviorTreeComponent = &OwnerComp;
	ActiveAbilitySpecHandle = AbilitySpec->Handle;
	AbilityEndedDelegateHandle = BossASC->OnAbilityEnded.AddUObject(
		this,
		&UBTTask_ArenaBossActivateAbility::HandleAbilityEnded);

	bActivatingAbility = true;
	const bool bActivated = BossASC->TryActivateAbility(ActiveAbilitySpecHandle, false);
	bActivatingAbility = false;
	if (bAbilityEndedDuringActivation)
	{
		const EBTNodeResult::Type SynchronousResult = DeferredEndResult;
		ResetRuntimeState(false);
		return SynchronousResult;
	}
	if (!bActivated)
	{
		ResetRuntimeState(false);
		return EBTNodeResult::Failed;
	}

	const FGameplayAbilitySpec* ActiveSpec = BossASC->FindAbilitySpecFromHandle(ActiveAbilitySpecHandle);
	if (!ActiveSpec || !ActiveSpec->IsActive())
	{
		ResetRuntimeState(false);
		return EBTNodeResult::Failed;
	}

	return EBTNodeResult::InProgress;
}

// Abort 先移除回调再取消 Ability，防止 CancelAbilityHandle 的同步结束委托重复完成 Task。
EBTNodeResult::Type UBTTask_ArenaBossActivateAbility::AbortTask(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory)
{
	ResetRuntimeState(true);
	return EBTNodeResult::Aborted;
}

// Task 正常、失败或被中断后都执行幂等清理，不把 Focus 的所有权留给旧攻击节点。
void UBTTask_ArenaBossActivateAbility::OnTaskFinished(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory,
	EBTNodeResult::Type TaskResult)
{
	ResetRuntimeState(false);
	if (AArenaBossAIController* BossController = Cast<AArenaBossAIController>(OwnerComp.GetAIOwner()))
	{
		BossController->ClearFocus(EAIFocusPriority::Gameplay);
	}
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}

// 只接受当前 Spec 的结束事件，取消返回 Failed，正常结束返回 Succeeded。
void UBTTask_ArenaBossActivateAbility::HandleAbilityEnded(const FAbilityEndedData& EndedData)
{
	if (EndedData.AbilitySpecHandle != ActiveAbilitySpecHandle)
	{
		return;
	}

	const EBTNodeResult::Type Result = EndedData.bWasCancelled
		? EBTNodeResult::Failed
		: EBTNodeResult::Succeeded;
	if (bActivatingAbility)
	{
		bAbilityEndedDuringActivation = true;
		DeferredEndResult = Result;
		return;
	}

	UBehaviorTreeComponent* BehaviorTreeComponent = ActiveBehaviorTreeComponent.Get();
	ResetRuntimeState(false);
	if (BehaviorTreeComponent)
	{
		FinishLatentTask(*BehaviorTreeComponent, Result);
	}
}

// 清理顺序保证取消 Ability 时回调已经解绑，并在所有路径重置同步结束保护状态。
void UBTTask_ArenaBossActivateAbility::ResetRuntimeState(bool bCancelActiveAbility)
{
	UAbilitySystemComponent* BossASC = ActiveAbilitySystemComponent.Get();
	const FGameplayAbilitySpecHandle AbilityHandleToCancel = ActiveAbilitySpecHandle;
	if (BossASC && AbilityEndedDelegateHandle.IsValid())
	{
		BossASC->OnAbilityEnded.Remove(AbilityEndedDelegateHandle);
	}

	AbilityEndedDelegateHandle.Reset();
	ActiveAbilitySystemComponent.Reset();
	ActiveBehaviorTreeComponent.Reset();
	ActiveAbilitySpecHandle = FGameplayAbilitySpecHandle();
	bActivatingAbility = false;
	bAbilityEndedDuringActivation = false;
	DeferredEndResult = EBTNodeResult::Failed;

	if (bCancelActiveAbility && BossASC && AbilityHandleToCancel.IsValid())
	{
		if (const FGameplayAbilitySpec* AbilitySpec = BossASC->FindAbilitySpecFromHandle(AbilityHandleToCancel);
			AbilitySpec && AbilitySpec->IsActive())
		{
			BossASC->CancelAbilityHandle(AbilityHandleToCancel);
		}
	}
}
