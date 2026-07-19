#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTService_ArenaBossUpdateTarget.generated.h"

UCLASS()
class PROJECTARCANEARENA_API UBTService_ArenaBossUpdateTarget : public UBTService
{
	GENERATED_BODY()

public:
	// 配置 TargetActor Object Key、0.2 秒更新和 150 单位切换滞回。
	UBTService_ArenaBossUpdateTarget();

protected:
	// 解析 BehaviorTree 使用的 Blackboard Key，资产类型错误时保持 InvalidKey。
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	// 在服务器选择稳定的最近存活玩家；目标失效时先取消攻击，再同步 Blackboard 与 CombatTarget。
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	// 使用独立可调周期调度 Service，不引入 Controller Tick。
	virtual void ScheduleNextTick(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	UPROPERTY(EditAnywhere, Category = "Arena|Boss AI")
	FBlackboardKeySelector TargetActorKey;

	UPROPERTY(EditAnywhere, Category = "Arena|Boss AI", meta = (ClampMin = "0.05"))
	float TargetUpdateInterval = 0.2f;

	UPROPERTY(EditAnywhere, Category = "Arena|Boss AI", meta = (ClampMin = "0.0"))
	float TargetSwitchDistanceAdvantage = 150.0f;
};
