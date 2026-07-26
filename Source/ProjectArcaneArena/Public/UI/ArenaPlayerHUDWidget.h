#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/ArenaGameState.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "GAS/ArenaDamageFeedbackTypes.h"
#include "TimerManager.h"
#include "ArenaPlayerHUDWidget.generated.h"

class UArenaAbilitySystemComponent;
class UArenaAttributeSet;
class AArenaBossCharacter;
class UProgressBar;
class UTextBlock;
class UWidget;
class UButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FArenaVictoryRestartRequestedSignature);

UCLASS()
class PROJECTARCANEARENA_API UArenaPlayerHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 仅切换第三人称中心准星的表现可见性，不参与目标或伤害判定。
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void SetThirdPersonReticleVisible(bool bVisible);

	// 显示复制的游戏阶段；Defeat 文本只属于表现层。
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void SetGamePhase(EArenaGamePhase NewPhase);

	// 显示复制的当前波次和剩余敌人数，不在 UI 内计算波次状态。
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void SetWaveState(int32 CurrentWaveIndex, int32 RemainingEnemyCount);

	// 在右上角显示服务器复制的本局升级随机种子，仅用于玩家查看和复现调试。
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void SetUpgradeRandomSeed(int32 UpgradeRandomSeed);

	// 绑定玩家 PlayerState 上的 GAS 数据源，HUD 只监听变化，不拥有玩法状态。
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void BindToAbilitySystem(UArenaAbilitySystemComponent* InAbilitySystemComponent, UArenaAttributeSet* InAttributeSet);

	// 绑定 GameState 复制的 Boss ASC；传入空值时解绑并隐藏 Boss HUD。
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void BindToBoss(AArenaBossCharacter* InBoss);

	// 刷新 Boss 名称与生命显示，所有数值只来自 Boss GAS 属性。
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void SetBossHealthValues(const FText& InBossName, float InHealth, float InMaxHealth);

	// 刷新本地 Boss Intro 名称、服务器倒计时与 Space 长按进度，不拥有演出时序。
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void SetBossIntroPresentation(
		bool bVisible,
		const FText& InBossName,
		float RemainingTime,
		float SkipProgress);

	// 刷新本地 Boss Outro 标题、服务器剩余时间和 Space 长按进度。
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void SetBossOutroPresentation(bool bVisible, float RemainingTime, float SkipProgress);

	// 刷新 Victory 面板和全员重开确认计数，按钮只提交本地意图。
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void SetVictoryPresentation(bool bVisible, bool bLocalReady, int32 ReadyCount, int32 RequiredCount);

	// 返回可聚焦的重开按钮，Controller 切换 UIOnly 时不聚焦不可交互容器。
	UButton* GetVictoryRestartButton() const { return VictoryRestartButton; }

	UPROPERTY(BlueprintAssignable, Category = "Arena|UI|Victory")
	FArenaVictoryRestartRequestedSignature OnVictoryRestartRequested;

	// 刷新生命显示，数值来自 GAS Attribute delegate。
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void SetHealthValues(float InHealth, float InMaxHealth);

	// 刷新护盾显示，数值来自 GAS Attribute delegate。
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void SetShieldValues(float InShield);

	// 刷新能源显示，数值来自 GAS Attribute delegate。
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void SetEnergyValues(float InEnergy, float InMaxEnergy);

	// 刷新复用的本地受击提示，不创建新的方向 Widget 或持有玩法状态。
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void ShowDamageFeedback(
		float DirectionAngleDegrees,
		bool bHasDirection,
		float Intensity,
		EArenaDamageFeedbackType FeedbackType);

	// 根据冷却标签有无刷新普攻状态，兼容没有倒计时数据的蓝图调用。
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void SetBasicAttackCooldownActive(bool bInCooldownActive);

	// 刷新普攻冷却秒数和进度，显示数据只来自 ASC 上的 Active GameplayEffect。
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void SetBasicAttackCooldownValues(bool bInCooldownActive, float InRemainingTime, float InDuration);

	// 刷新 Fireball 冷却秒数和进度，显示数据只来自 ASC 上的 Active GameplayEffect。
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void SetFireballCooldownValues(bool bInCooldownActive, float InRemainingTime, float InDuration);

	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void SetDashCooldownValues(bool bInCooldownActive, float InRemainingTime, float InDuration);

	// 刷新 Shield 冷却秒数和进度，显示数据只来自 ASC 上的 Active GameplayEffect。
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void SetShieldCooldownValues(bool bInCooldownActive, float InRemainingTime, float InDuration);

	// 刷新 LightningStorm 冷却状态，未授予技能时显示锁定。
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void SetLightningStormCooldownValues(bool bInCooldownActive, float InRemainingTime, float InDuration);

protected:
	// 蓝图未提供准星控件时创建一个轻量居中占位，保证第三人称可直接使用。
	virtual void NativeConstruct() override;
	// Widget 销毁时隐藏演出/终局面板并解绑 GAS 委托，避免回调悬挂到已销毁 UI。
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI")
	TObjectPtr<UProgressBar> HealthProgressBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI")
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI")
	TObjectPtr<UProgressBar> ShieldProgressBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI")
	TObjectPtr<UTextBlock> ShieldText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI")
	TObjectPtr<UProgressBar> EnergyProgressBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI")
	TObjectPtr<UTextBlock> EnergyText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI")
	TObjectPtr<UTextBlock> BasicAttackCooldownText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI")
	TObjectPtr<UTextBlock> BasicAttackSlotText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI")
	TObjectPtr<UTextBlock> FireballSlotText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI")
	TObjectPtr<UTextBlock> DashSlotText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI")
	TObjectPtr<UTextBlock> ShieldSlotText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI")
	TObjectPtr<UTextBlock> UltimateSlotText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI")
	TObjectPtr<UTextBlock> AimReticleText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI")
	TObjectPtr<UTextBlock> PhaseText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI")
	TObjectPtr<UTextBlock> WaveText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI")
	TObjectPtr<UTextBlock> RemainingEnemiesText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI")
	TObjectPtr<UTextBlock> RandomSeedText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI")
	TObjectPtr<UTextBlock> DefeatText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI")
	TObjectPtr<UWidget> BossPanel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI")
	TObjectPtr<UTextBlock> BossNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI")
	TObjectPtr<UTextBlock> BossPhaseText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI")
	TObjectPtr<UProgressBar> BossHealthProgressBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI")
	TObjectPtr<UTextBlock> BossHealthText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI|Boss Intro")
	TObjectPtr<UWidget> BossIntroPanel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI|Boss Intro")
	TObjectPtr<UTextBlock> BossIntroText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI|Boss Intro")
	TObjectPtr<UTextBlock> BossIntroCountdownText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI|Boss Intro")
	TObjectPtr<UTextBlock> BossIntroSkipText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI|Boss Intro")
	TObjectPtr<UProgressBar> BossIntroSkipProgressBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI|Boss Outro")
	TObjectPtr<UWidget> BossOutroPanel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI|Boss Outro")
	TObjectPtr<UTextBlock> BossDefeatedText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI|Boss Outro")
	TObjectPtr<UTextBlock> BossOutroSkipText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI|Boss Outro")
	TObjectPtr<UProgressBar> BossOutroSkipProgressBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI|Victory")
	TObjectPtr<UWidget> VictoryPanel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI|Victory")
	TObjectPtr<UTextBlock> VictoryText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI|Victory")
	TObjectPtr<UButton> VictoryRestartButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI|Victory")
	TObjectPtr<UTextBlock> VictoryRestartButtonText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI|Victory")
	TObjectPtr<UTextBlock> VictoryRestartStatusText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI|Damage Feedback")
	TObjectPtr<UWidget> DamageDirectionIndicator;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI|Damage Feedback")
	TObjectPtr<UTextBlock> ShieldBreakText;

	// 蓝图可在同一个常驻 HUD 上播放更完整动画，不能据此修改 Shield 或 Health。
	UFUNCTION(BlueprintImplementableEvent, Category = "Arena|UI|Damage Feedback", meta = (DisplayName = "On Damage Feedback"))
	void K2_OnDamageFeedback(
		float DirectionAngleDegrees,
		bool bHasDirection,
		float Intensity,
		EArenaDamageFeedbackType FeedbackType);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|UI|Damage Feedback", meta = (ClampMin = "0.0"))
	float DamageDirectionDuration = 0.45f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|UI|Damage Feedback", meta = (ClampMin = "0.0"))
	float ShieldBreakMessageDuration = 0.7f;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI")
	TObjectPtr<UProgressBar> BasicAttackCooldownProgressBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI")
	TObjectPtr<UProgressBar> FireballCooldownProgressBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI")
	TObjectPtr<UProgressBar> DashCooldownProgressBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI")
	TObjectPtr<UProgressBar> ShieldCooldownProgressBar;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	float CurrentHealth = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	float CurrentMaxHealth = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	float HealthPercent = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	float CurrentShield = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	float ShieldPercent = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	float CurrentEnergy = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	float CurrentMaxEnergy = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	float EnergyPercent = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	bool bBasicAttackCooldownActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	float BasicAttackCooldownRemaining = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	float BasicAttackCooldownDuration = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	float BasicAttackCooldownPercent = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	bool bFireballCooldownActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	float FireballCooldownRemaining = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	float FireballCooldownDuration = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	float FireballCooldownPercent = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	bool bDashCooldownActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	float DashCooldownRemaining = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	float DashCooldownDuration = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	float DashCooldownPercent = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	bool bShieldCooldownActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	float ShieldCooldownRemaining = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	float ShieldCooldownDuration = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	float ShieldCooldownPercent = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	bool bLightningStormCooldownActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	float LightningStormCooldownRemaining = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	float LightningStormCooldownDuration = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	float LightningStormCooldownPercent = 0.0f;

private:
	// 隐藏复用的方向提示和破盾文本，连续受伤只刷新同一个 Timer。
	void ClearDamageFeedbackPresentation();
	// 解绑当前 GAS 数据源，支持 PlayerState 重绑或 Widget 销毁。
	void UnbindFromAbilitySystem();
	// 解绑当前 Boss 属性、死亡和阶段标签委托，避免换 Boss 或切图后残留回调。
	void UnbindFromBoss();
	// 同步设置 Boss 面板及可选独立控件可见性。
	void SetBossPanelVisible(bool bVisible);
	// 从 Boss ASC 阶段标签刷新独立文本或旧 HUD 的名称合并回退。
	void RefreshBossPhasePresentation();

	// 初次绑定后立即用当前 AttributeSet 值刷新 UI，避免等下一次属性变化。
	void RefreshAttributeValues();

	// 初始化未实现技能的占位文本，后续接入 Ability 后再替换成真实状态。
	void RefreshSkillSlotPlaceholders();

	// 从 ASC 查询普攻冷却 ActiveGE 的剩余时间，并刷新 HUD。
	void RefreshBasicAttackCooldownFromAbilitySystem();

	// 从 ASC 查询 Fireball 冷却 ActiveGE 的剩余时间，并刷新 HUD。
	void RefreshFireballCooldownFromAbilitySystem();

	void RefreshDashCooldownFromAbilitySystem();

	// 从 ASC 查询 Shield 冷却 ActiveGE 的剩余时间，并刷新 HUD。
	void RefreshShieldCooldownFromAbilitySystem();

	// 从 ASC 查询 LightningStorm 冷却 ActiveGE 的剩余时间，并刷新 R 槽。
	void RefreshLightningStormCooldownFromAbilitySystem();

	// 冷却期间用定时器刷新秒数，避免把整个 HUD 放进 Tick。
	void StartBasicAttackCooldownTimer();

	// Fireball 冷却期间单独刷新秒数，避免互相影响。
	void StartFireballCooldownTimer();

	void StartDashCooldownTimer();

	// Shield 冷却期间单独刷新秒数，避免依赖 Widget Tick。
	void StartShieldCooldownTimer();

	// LightningStorm 冷却期间单独刷新秒数。
	void StartLightningStormCooldownTimer();

	// 冷却结束或 Widget 销毁时停止刷新定时器。
	void StopBasicAttackCooldownTimer();

	// 冷却结束或 Widget 销毁时停止刷新定时器。
	void StopFireballCooldownTimer();

	void StopDashCooldownTimer();

	// 冷却结束或 Widget 销毁时停止刷新定时器。
	void StopShieldCooldownTimer();

	// 冷却结束或 Widget 销毁时停止 LightningStorm 刷新定时器。
	void StopLightningStormCooldownTimer();

	// 查询拥有 Cooldown.BasicAttack 标签的 ActiveGE，返回最长剩余时间。
	bool GetBasicAttackCooldownTime(float& OutRemainingTime, float& OutDuration) const;

	// 查询指定 Cooldown 标签的 ActiveGE，返回最长剩余时间。
	bool GetCooldownTimeForTag(const FGameplayTag& CooldownTag, float& OutRemainingTime, float& OutDuration) const;

	// 根据 AbilitySpec 输入标签判断技能是否已经授予，用于技能槽占位显示。
	bool HasGrantedAbilityForInputTag(const FGameplayTag& InputTag) const;

	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void HandleMaxHealthChanged(const FOnAttributeChangeData& Data);
	void HandleShieldChanged(const FOnAttributeChangeData& Data);
	void HandleEnergyChanged(const FOnAttributeChangeData& Data);
	void HandleMaxEnergyChanged(const FOnAttributeChangeData& Data);
	void HandleBasicAttackCooldownChanged(const FGameplayTag CallbackTag, int32 NewCount);
	void HandleFireballCooldownChanged(const FGameplayTag CallbackTag, int32 NewCount);
	void HandleDashCooldownChanged(const FGameplayTag CallbackTag, int32 NewCount);
	void HandleShieldCooldownChanged(const FGameplayTag CallbackTag, int32 NewCount);
	void HandleLightningStormCooldownChanged(const FGameplayTag CallbackTag, int32 NewCount);
	// Boss Health 变化只刷新本地 HUD，不参与死亡判断。
	void HandleBossHealthChanged(const FOnAttributeChangeData& Data);
	// Boss MaxHealth 变化时使用最新 Health 重算比例。
	void HandleBossMaxHealthChanged(const FOnAttributeChangeData& Data);
	// 死亡标签把 Boss Health 刷为零并保留 Outro 面板，ActiveBoss 清空后再正式解绑。
	void HandleBossDeadTagChanged(const FGameplayTag CallbackTag, int32 NewCount);
	// 任一 Boss 阶段标签变化时重新解析最终阶段并刷新本地表现。
	void HandleBossPhaseTagChanged(const FGameplayTag CallbackTag, int32 NewCount);
	// 重开按钮点击后只广播本地 UI 意图，由 Controller 发送服务器 RPC。
	UFUNCTION()
	void HandleVictoryRestartButtonClicked();

	TWeakObjectPtr<UArenaAbilitySystemComponent> BoundAbilitySystemComponent;
	TWeakObjectPtr<UArenaAttributeSet> BoundAttributeSet;
	TWeakObjectPtr<AArenaBossCharacter> BoundBoss;
	TWeakObjectPtr<UArenaAbilitySystemComponent> BoundBossAbilitySystemComponent;
	TWeakObjectPtr<UArenaAttributeSet> BoundBossAttributeSet;

	FDelegateHandle HealthChangedDelegateHandle;
	FDelegateHandle MaxHealthChangedDelegateHandle;
	FDelegateHandle ShieldChangedDelegateHandle;
	FDelegateHandle EnergyChangedDelegateHandle;
	FDelegateHandle MaxEnergyChangedDelegateHandle;
	FDelegateHandle BasicAttackCooldownTagDelegateHandle;
	FDelegateHandle FireballCooldownTagDelegateHandle;
	FDelegateHandle DashCooldownTagDelegateHandle;
	FDelegateHandle ShieldCooldownTagDelegateHandle;
	FDelegateHandle LightningStormCooldownTagDelegateHandle;
	FDelegateHandle BossHealthChangedDelegateHandle;
	FDelegateHandle BossMaxHealthChangedDelegateHandle;
	FDelegateHandle BossDeadTagDelegateHandle;
	FDelegateHandle BossPhaseOneTagDelegateHandle;
	FDelegateHandle BossPhaseTwoTagDelegateHandle;
	FDelegateHandle BossPhaseThreeTagDelegateHandle;

	FTimerHandle BasicAttackCooldownTimerHandle;
	FTimerHandle FireballCooldownTimerHandle;
	FTimerHandle DashCooldownTimerHandle;
	FTimerHandle ShieldCooldownTimerHandle;
	FTimerHandle LightningStormCooldownTimerHandle;
	FTimerHandle DamageFeedbackTimerHandle;
	bool bUsesRuntimeDamageDirectionIndicator = false;
};
