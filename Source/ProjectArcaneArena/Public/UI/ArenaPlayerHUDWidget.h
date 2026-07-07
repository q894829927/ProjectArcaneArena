#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
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
	// 绑定玩家 PlayerState 上的 GAS 数据源，HUD 只监听变化，不拥有玩法状态。
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void BindToAbilitySystem(UArenaAbilitySystemComponent* InAbilitySystemComponent, UArenaAttributeSet* InAttributeSet);

	// 刷新生命显示，数值来自 GAS Attribute delegate。
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void SetHealthValues(float InHealth, float InMaxHealth);

	// 刷新护盾显示，数值来自 GAS Attribute delegate。
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void SetShieldValues(float InShield, float InMaxShield);

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

protected:
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
	TObjectPtr<UProgressBar> BasicAttackCooldownProgressBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI")
	TObjectPtr<UProgressBar> FireballCooldownProgressBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI")
	TObjectPtr<UProgressBar> DashCooldownProgressBar;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	float CurrentHealth = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	float CurrentMaxHealth = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	float HealthPercent = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	float CurrentShield = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	float CurrentMaxShield = 0.0f;

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

	// 冷却期间用定时器刷新秒数，避免把整个 HUD 放进 Tick。
	void StartBasicAttackCooldownTimer();

	// Fireball 冷却期间单独刷新秒数，避免互相影响。
	void StartFireballCooldownTimer();

	void StartDashCooldownTimer();

	// 冷却结束或 Widget 销毁时停止刷新定时器。
	void StopBasicAttackCooldownTimer();

	// 冷却结束或 Widget 销毁时停止刷新定时器。
	void StopFireballCooldownTimer();

	void StopDashCooldownTimer();

	// 查询拥有 Cooldown.BasicAttack 标签的 ActiveGE，返回最长剩余时间。
	bool GetBasicAttackCooldownTime(float& OutRemainingTime, float& OutDuration) const;

	// 查询指定 Cooldown 标签的 ActiveGE，返回最长剩余时间。
	bool GetCooldownTimeForTag(const FGameplayTag& CooldownTag, float& OutRemainingTime, float& OutDuration) const;

	// 根据 AbilitySpec 输入标签判断技能是否已经授予，用于技能槽占位显示。
	bool HasGrantedAbilityForInputTag(const FGameplayTag& InputTag) const;

	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void HandleMaxHealthChanged(const FOnAttributeChangeData& Data);
	void HandleShieldChanged(const FOnAttributeChangeData& Data);
	void HandleMaxShieldChanged(const FOnAttributeChangeData& Data);
	void HandleEnergyChanged(const FOnAttributeChangeData& Data);
	void HandleMaxEnergyChanged(const FOnAttributeChangeData& Data);
	void HandleBasicAttackCooldownChanged(const FGameplayTag CallbackTag, int32 NewCount);
	void HandleFireballCooldownChanged(const FGameplayTag CallbackTag, int32 NewCount);
	void HandleDashCooldownChanged(const FGameplayTag CallbackTag, int32 NewCount);

	TWeakObjectPtr<UArenaAbilitySystemComponent> BoundAbilitySystemComponent;
	TWeakObjectPtr<UArenaAttributeSet> BoundAttributeSet;

	FDelegateHandle HealthChangedDelegateHandle;
	FDelegateHandle MaxHealthChangedDelegateHandle;
	FDelegateHandle ShieldChangedDelegateHandle;
	FDelegateHandle MaxShieldChangedDelegateHandle;
	FDelegateHandle EnergyChangedDelegateHandle;
	FDelegateHandle MaxEnergyChangedDelegateHandle;
	FDelegateHandle BasicAttackCooldownTagDelegateHandle;
	FDelegateHandle FireballCooldownTagDelegateHandle;
	FDelegateHandle DashCooldownTagDelegateHandle;

	FTimerHandle BasicAttackCooldownTimerHandle;
	FTimerHandle FireballCooldownTimerHandle;
	FTimerHandle DashCooldownTimerHandle;
};
