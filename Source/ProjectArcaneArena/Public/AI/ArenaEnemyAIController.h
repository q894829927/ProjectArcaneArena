#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "ArenaEnemyAIController.generated.h"

class AArenaEnemyCharacter;

UCLASS()
class PROJECTARCANEARENA_API AArenaEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	// 初始化服务器低频决策 Tick；客户端不会运行敌人 AI。
	AArenaEnemyAIController();

protected:
	// 缓存受控敌人并只在 Authority 启用决策循环。
	virtual void OnPossess(APawn* InPawn) override;
	// 停止寻路、清理目标并释放受控敌人引用。
	virtual void OnUnPossess() override;
	// 运行通用 Idle/Chase/PrimaryAttack 决策，并在目标被遮挡时持续沿导航路径追击。
	virtual void Tick(float DeltaSeconds) override;

private:
	// 在服务器 PlayerArray 中选择最近的存活玩家，避免客户端参与 AI 决策。
	AActor* FindNearestLivingPlayer() const;
	// 只接受拥有 ASC 且未进入 State.Dead 的玩家目标。
	bool IsValidCombatTarget(const AActor* Candidate) const;
	// 清理寻路、主攻击、朝向和敌人持有的临时 CombatTarget。
	void StopCombatMovement();

	TWeakObjectPtr<AArenaEnemyCharacter> ControlledEnemy;
	TWeakObjectPtr<AActor> CurrentTarget;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|AI", meta = (ClampMin = "0.05"))
	float DecisionInterval = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|AI", meta = (ClampMin = "0.0"))
	float MoveAcceptancePadding = 20.0f;

	// 目标被墙遮挡时使用极小到达半径，避免攻击范围让 MoveTo 提前判定完成。
	UPROPERTY(EditDefaultsOnly, Category = "Arena|AI", meta = (ClampMin = "0.0"))
	float OccludedMoveAcceptanceRadius = 5.0f;
};
