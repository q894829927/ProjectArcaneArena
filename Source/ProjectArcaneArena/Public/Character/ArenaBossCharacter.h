#pragma once

#include "CoreMinimal.h"
#include "Character/ArenaEnemyCharacter.h"
#include "Core/ArenaGameState.h"
#include "GameplayEffectTypes.h"
#include "ArenaBossCharacter.generated.h"

class UGameplayEffect;

UCLASS()
class PROJECTARCANEARENA_API AArenaBossCharacter : public AArenaEnemyCharacter
{
	GENERATED_BODY()

public:
	// 设置 Boss 专属 Controller、路径朝向和显示默认值，战斗能力与死亡流程继续复用敌人基类。
	AArenaBossCharacter();

	// 返回 Boss HUD 使用的本地化名称，UI 不自行保存玩法身份。
	UFUNCTION(BlueprintPure, Category = "Arena|Boss")
	FText GetBossDisplayName() const { return BossDisplayName; }

	// 从 Boss ASC 当前标签解析阶段，避免复制第二份阶段枚举或让 UI 持有玩法状态。
	UFUNCTION(BlueprintPure, Category = "Arena|Boss")
	FGameplayTag GetCurrentBossPhaseTag() const;

protected:
	// 在敌人 GAS 初始化完成后绑定服务器阶段委托，并按当前 Combat 状态初始化 Phase 1。
	virtual void BeginPlay() override;

	// 销毁前对称解除阶段委托并移除 Enrage GE、Cue 与阶段标签。
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss")
	FText BossDisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Phase", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PhaseTwoHealthRatio = 0.70f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Phase", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PhaseThreeHealthRatio = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Phase")
	TSubclassOf<UGameplayEffect> EnrageEffectClass;

private:
	// 注册 Health、死亡和 GameState 阶段委托，所有阶段写入仅在服务器执行。
	void BindBossPhaseDelegates();

	// 对称解除阶段相关委托，支持关卡切换和 Actor 销毁。
	void UnbindBossPhaseDelegates();

	// 为当前 Combat 生命周期添加初始 Phase 1，不播放转换 Cue。
	void InitializeBossPhaseState();

	// 根据最新 Health 比例单向推进到目标阶段，跨阈值时只提交最终阶段。
	void EvaluateBossPhase(float NewHealth);

	// 先添加新阶段标签再移除旧标签，并按需应用 Enrage 与转换 Cue。
	void AdvanceToBossPhase(const FGameplayTag& NewPhaseTag, int32 NewPhaseNumber);

	// 应用一次配置的 Infinite Enrage GE，并保存 Handle 用于确定性清理。
	void ApplyEnrageEffect();

	// 移除阶段标签和 Enrage GE，供死亡、终局和销毁路径复用。
	void CleanupBossPhaseState();

	// 在服务器执行一次阶段转换 Cue，RawMagnitude 携带目标阶段编号。
	void ExecutePhaseTransitionCue(int32 NewPhaseNumber);

	// 把阶段叶标签映射为单调递增序号，用于阻止治疗导致阶段回退。
	static int32 GetBossPhaseNumber(const FGameplayTag& PhaseTag);

	// Health Attribute 变化时在服务器重新计算阶段，零生命不会触发临死狂暴。
	void HandleBossPhaseHealthChanged(const FOnAttributeChangeData& Data);

	// Boss 死亡广播到达时立即清除阶段状态和持续表现。
	UFUNCTION()
	void HandleBossPhaseDeath(AArenaEnemyCharacter* Enemy);

	// GameState 离开 Combat 时清理阶段；重新进入时仅为仍存活 Boss 初始化 Phase 1。
	UFUNCTION()
	void HandleBossGamePhaseChanged(EArenaGamePhase OldPhase, EArenaGamePhase NewPhase);

	TWeakObjectPtr<AArenaGameState> BoundBossGameState;
	FDelegateHandle BossPhaseHealthChangedDelegateHandle;
	FActiveGameplayEffectHandle EnrageEffectHandle;
};
