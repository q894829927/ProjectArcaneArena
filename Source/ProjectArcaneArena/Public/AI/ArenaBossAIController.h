#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Core/ArenaGameState.h"
#include "GameplayTagContainer.h"
#include "ArenaBossAIController.generated.h"

class AArenaBossCharacter;
class UBehaviorTree;

DECLARE_LOG_CATEGORY_EXTERN(LogArenaBossAI, Log, All);

UCLASS(Blueprintable)
class PROJECTARCANEARENA_API AArenaBossAIController : public AAIController
{
	GENERATED_BODY()

public:
	// 创建只由服务器运行的 Boss Controller，具体 BehaviorTree 由蓝图默认值配置。
	AArenaBossAIController();

	// Blackboard 只使用一个 Actor Key 同时驱动追击与 Ability CombatTarget。
	static const FName TargetActorKeyName;

	// 开场缓冲结束前只允许行为树选敌和追击，所有技能分支统一保持不可激活。
	bool CanActivateBossAbilities() const;

protected:
	// 绑定 Boss 状态和 GameState 阶段，并在 Combat 中启动专用 BehaviorTree。
	virtual void OnPossess(APawn* InPawn) override;
	// 停止行为树、解绑委托并清除本次 Boss 战斗引用。
	virtual void OnUnPossess() override;
	// 世界销毁时兜底解绑 GameState 与 ASC 委托，避免回调旧 Controller。
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss AI")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

	// Boss 生成后的首次施法缓冲，不占用 Ability 自身冷却并为后续 Intro 留出接入点。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss AI", meta = (ClampMin = "0.0"))
	float InitialAbilityDelay = 3.0f;

private:
	// 根据 Combat 阶段及 Dead/Stunned 标签统一启动或停止 Boss Brain。
	void RefreshBossLogicState();
	// 验证 BehaviorTree/Blackboard 后启动服务器决策，失败时保持停止并记录配置错误。
	bool StartBossLogic();
	// 停止 Brain、寻路、Focus、CombatTarget 和当前主攻击。
	void StopBossLogic(const FString& Reason);
	// 绑定 Boss Dead/Stunned 标签与服务器 GameState 阶段变化。
	void BindBossDelegates();
	// 对称解除当前 Boss 与 GameState 委托，支持 UnPossess 和世界切换。
	void UnbindBossDelegates();

	// Boss 死亡或眩晕状态变化时立即刷新行为树运行状态。
	void HandleBossStateTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

	// Combat 之外停止所有决策，重新进入 Combat 时仅在 Boss 存活可行动时恢复。
	UFUNCTION()
	void HandleGamePhaseChanged(EArenaGamePhase OldPhase, EArenaGamePhase NewPhase);

	TWeakObjectPtr<AArenaBossCharacter> ControlledBoss;
	TWeakObjectPtr<AArenaGameState> BoundGameState;
	FDelegateHandle DeadTagDelegateHandle;
	FDelegateHandle StunnedTagDelegateHandle;
	float AbilityActivationAllowedTime = 0.0f;
};
