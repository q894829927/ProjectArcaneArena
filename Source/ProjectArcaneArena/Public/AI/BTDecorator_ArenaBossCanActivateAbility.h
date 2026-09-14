#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "GameplayTagContainer.h"
#include "BTDecorator_ArenaBossCanActivateAbility.generated.h"

UCLASS()
class PROJECTARCANEARENA_API UBTDecorator_ArenaBossCanActivateAbility : public UBTDecorator
{
	GENERATED_BODY()

public:
	// 配置按精确 Ability AssetTag 判断 Boss 攻击资格的可中断 Decorator。
	UBTDecorator_ArenaBossCanActivateAbility();

	// 在编辑器节点标题中显示目标 Key 与 AbilityTag，便于排查行为树配置。
	virtual FString GetStaticDescription() const override;

protected:
	// 加载 BehaviorTree 后解析 TargetActor Object Key，错误 Key 保持无效并令条件失败。
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	// 统一检查阶段、状态、目标、距离、路径以及 GAS 的 Cost/Cooldown 激活资格。
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
	// 记录首次条件结果并启动 0.1 秒轮询，供低优先级 Chase 被及时中断。
	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	// 定期重新评估条件，仅在结果变化时请求行为树重新搜索分支。
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "Arena|Boss AI")
	FBlackboardKeySelector TargetActorKey;

	UPROPERTY(EditAnywhere, Category = "Arena|Boss AI")
	FGameplayTag AbilityTag;

	UPROPERTY(EditAnywhere, Category = "Arena|Boss AI", meta = (ClampMin = "0.05"))
	float ConditionCheckInterval = 0.1f;

private:
	bool bLastConditionValue = false;
	mutable bool bLoggedSpecConfigurationError = false;
};
