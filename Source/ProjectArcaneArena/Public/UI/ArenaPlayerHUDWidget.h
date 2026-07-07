#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
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

	// 根据冷却标签有无刷新普攻状态，后续可替换为倒计时显示。
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void SetBasicAttackCooldownActive(bool bInCooldownActive);

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

private:
	// 解绑当前 GAS 数据源，支持 PlayerState 重绑或 Widget 销毁。
	void UnbindFromAbilitySystem();

	// 初次绑定后立即用当前 AttributeSet 值刷新 UI，避免等下一次属性变化。
	void RefreshAttributeValues();

	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void HandleMaxHealthChanged(const FOnAttributeChangeData& Data);
	void HandleShieldChanged(const FOnAttributeChangeData& Data);
	void HandleMaxShieldChanged(const FOnAttributeChangeData& Data);
	void HandleEnergyChanged(const FOnAttributeChangeData& Data);
	void HandleMaxEnergyChanged(const FOnAttributeChangeData& Data);
	void HandleBasicAttackCooldownChanged(const FGameplayTag CallbackTag, int32 NewCount);

	TWeakObjectPtr<UArenaAbilitySystemComponent> BoundAbilitySystemComponent;
	TWeakObjectPtr<UArenaAttributeSet> BoundAttributeSet;

	FDelegateHandle HealthChangedDelegateHandle;
	FDelegateHandle MaxHealthChangedDelegateHandle;
	FDelegateHandle ShieldChangedDelegateHandle;
	FDelegateHandle MaxShieldChangedDelegateHandle;
	FDelegateHandle EnergyChangedDelegateHandle;
	FDelegateHandle MaxEnergyChangedDelegateHandle;
	FDelegateHandle BasicAttackCooldownTagDelegateHandle;
};
