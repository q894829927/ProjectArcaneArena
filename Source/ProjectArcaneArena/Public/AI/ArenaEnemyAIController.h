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
	AArenaEnemyAIController();

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	// 在服务器 PlayerArray 中选择最近的存活玩家，避免客户端参与 AI 决策。
	AActor* FindNearestLivingPlayer() const;
	bool IsValidCombatTarget(const AActor* Candidate) const;
	void StopCombatMovement();

	TWeakObjectPtr<AArenaEnemyCharacter> ControlledEnemy;
	TWeakObjectPtr<AActor> CurrentTarget;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|AI", meta = (ClampMin = "0.05"))
	float DecisionInterval = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|AI", meta = (ClampMin = "0.0"))
	float MoveAcceptancePadding = 20.0f;
};
