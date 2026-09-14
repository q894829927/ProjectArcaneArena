#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "GameplayTagContainer.h"
#include "BTTask_ArenaBossActivateAbility.generated.h"

class UAbilitySystemComponent;

UCLASS()
class PROJECTARCANEARENA_API UBTTask_ArenaBossActivateAbility : public UBTTaskNode
{
	GENERATED_BODY()

public:
	// 创建按精确 AssetTag 激活并等待指定 GAS Ability 结束的实例化 Boss Task。
	UBTTask_ArenaBossActivateAbility();

	// 在编辑器节点标题中显示目标 Key 与 AbilityTag，降低资产误配概率。
	virtual FString GetStaticDescription() const override;

protected:
	// 加载 BehaviorTree 后解析共享 TargetActor Object Key。
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	// 写入 CombatTarget、停止移动和朝向后激活唯一 Spec，并等待该 Spec 的结束委托。
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	// 分支中断时先解绑委托再取消当前 Spec，阻止迟到 GroundSlam 释放。
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	// 任何结束路径都兜底解除 ASC 委托与弱引用，支持 BehaviorTree 重启和 Pawn 销毁。
	virtual void OnTaskFinished(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory,
		EBTNodeResult::Type TaskResult) override;

	UPROPERTY(EditAnywhere, Category = "Arena|Boss AI")
	FBlackboardKeySelector TargetActorKey;

	UPROPERTY(EditAnywhere, Category = "Arena|Boss AI")
	FGameplayTag AbilityTag;

private:
	// 只处理本 Task 激活的 Spec 结束事件；同步结束会延迟到 TryActivateAbility 返回后收口。
	void HandleAbilityEnded(const FAbilityEndedData& EndedData);
	// 对称移除 ASC 委托并重置运行时状态，可选择取消仍活跃的对应 Spec。
	void ResetRuntimeState(bool bCancelActiveAbility);

	TWeakObjectPtr<UAbilitySystemComponent> ActiveAbilitySystemComponent;
	TWeakObjectPtr<UBehaviorTreeComponent> ActiveBehaviorTreeComponent;
	FGameplayAbilitySpecHandle ActiveAbilitySpecHandle;
	FDelegateHandle AbilityEndedDelegateHandle;
	EBTNodeResult::Type DeferredEndResult = EBTNodeResult::Failed;
	bool bActivatingAbility = false;
	bool bAbilityEndedDuringActivation = false;
};
