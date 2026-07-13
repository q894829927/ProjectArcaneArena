#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/ArenaGameState.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "ArenaPlayerHUDWidget.generated.h"

class UArenaAbilitySystemComponent;
class UArenaAttributeSet;
class UProgressBar;
class UTextBlock;

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

	// 刷新生命显示，数值来自 GAS Attribute delegate。
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void SetHealthValues(float InHealth, float InMaxHealth);

	// 刷新护盾显示，数值来自 GAS Attribute delegate。
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void SetShieldValues(float InShield);

	// 刷新能源显示，数值来自 GAS Attribute delegate。
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void SetEnergyValues(float InEnergy, float InMaxEnergy);

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
	// Widget 销毁时解绑 GAS 委托，避免回调悬挂到已销毁 UI。
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
	// 解绑当前 GAS 数据源，支持 PlayerState 重绑或 Widget 销毁。
	void UnbindFromAbilitySystem();

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

	TWeakObjectPtr<UArenaAbilitySystemComponent> BoundAbilitySystemComponent;
	TWeakObjectPtr<UArenaAttributeSet> BoundAttributeSet;

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

	FTimerHandle BasicAttackCooldownTimerHandle;
	FTimerHandle FireballCooldownTimerHandle;
	FTimerHandle DashCooldownTimerHandle;
	FTimerHandle ShieldCooldownTimerHandle;
	FTimerHandle LightningStormCooldownTimerHandle;
};
