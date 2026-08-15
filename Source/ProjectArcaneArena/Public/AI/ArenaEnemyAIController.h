#pragma once

#include "CoreMinimal.h"
#include "DetourCrowdAIController.h"
#include "ArenaEnemyAIController.generated.h"

class AArenaEnemyCharacter;

UCLASS()
class PROJECTARCANEARENA_API AArenaEnemyAIController : public ADetourCrowdAIController
{
	GENERATED_BODY()

public:
	// 初始化服务器低频决策 Tick 与 Detour Crowd PathFollowing；客户端不会运行敌人 AI。
	AArenaEnemyAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

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
	// 将蓝图可调的分离参数写入 Detour Crowd，避免多个敌人沿同一路径互相顶死。
	void ConfigureCrowdFollowing();
	// 检测追击停滞并短暂移动到 NavMesh 侧向点，为被前排堵住的敌人释放移动空间。
	bool TryRunCrowdRecovery(
		AArenaEnemyCharacter* Enemy,
		const AActor* Target,
		float DeltaSeconds);
	// 清理侧移计时并以当前位置重新开始停滞采样，避免攻击或换目标后误触发。
	void ResetCrowdRecovery(const FVector& CurrentLocation = FVector::ZeroVector);

	TWeakObjectPtr<AArenaEnemyCharacter> ControlledEnemy;
	TWeakObjectPtr<AActor> CurrentTarget;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|AI", meta = (ClampMin = "0.05"))
	float DecisionInterval = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|AI", meta = (ClampMin = "0.0"))
	float MoveAcceptancePadding = 20.0f;

	// 提高相邻 Crowd Agent 的横向分离意愿，同时保留 NavMesh 走廊约束。
	UPROPERTY(EditDefaultsOnly, Category = "Arena|AI|Crowd", meta = (ClampMin = "0.0"))
	float CrowdSeparationWeight = 4.0f;

	// 控制 Detour Crowd 查询附近动态代理的范围，覆盖普通敌人胶囊排队距离。
	UPROPERTY(EditDefaultsOnly, Category = "Arena|AI|Crowd", meta = (ClampMin = "0.0"))
	float CrowdCollisionQueryRange = 400.0f;

	// 连续低于该水平速度才计入停滞时间，正常追击减速不会立刻触发侧移。
	UPROPERTY(EditDefaultsOnly, Category = "Arena|AI|Crowd Recovery", meta = (ClampMin = "0.0"))
	float StuckSpeedThreshold = 10.0f;

	// 达到该停滞时间后请求一次侧向脱困，低频 AI Tick 会持续累计实际 DeltaSeconds。
	UPROPERTY(EditDefaultsOnly, Category = "Arena|AI|Crowd Recovery", meta = (ClampMin = "0.1"))
	float StuckDetectionDuration = 0.8f;

	// 侧向脱困点距离；目标点仍投影到 NavMesh，避免把敌人推出可导航区域。
	UPROPERTY(EditDefaultsOnly, Category = "Arena|AI|Crowd Recovery", meta = (ClampMin = "10.0"))
	float StuckSidestepDistance = 140.0f;

	// 单次侧移最多占用的时间，超时后恢复正式 Chase 决策。
	UPROPERTY(EditDefaultsOnly, Category = "Arena|AI|Crowd Recovery", meta = (ClampMin = "0.1"))
	float StuckSidestepDuration = 0.7f;

	// 目标被墙遮挡时使用极小到达半径，避免攻击范围让 MoveTo 提前判定完成。
	UPROPERTY(EditDefaultsOnly, Category = "Arena|AI", meta = (ClampMin = "0.0"))
	float OccludedMoveAcceptanceRadius = 5.0f;

	FVector LastCrowdSampleLocation = FVector::ZeroVector;
	FVector CrowdRecoveryLocation = FVector::ZeroVector;
	float StuckAccumulatedTime = 0.0f;
	double CrowdRecoveryEndTime = 0.0;
	bool bHasCrowdSample = false;
	bool bCrowdRecoveryActive = false;
	bool bNextRecoveryUsesRightSide = false;
};
