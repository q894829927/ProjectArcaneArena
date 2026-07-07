#include "UI/ArenaPlayerHUDWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GAS/ArenaAbilitySystemComponent.h"
#include "GAS/ArenaAttributeSet.h"
#include "GAS/ArenaGameplayTags.h"

namespace
{
	float CalculatePercent(float CurrentValue, float MaxValue)
	{
		return MaxValue > 0.0f
			? FMath::Clamp(CurrentValue / MaxValue, 0.0f, 1.0f)
			: 0.0f;
	}

	FText MakeAttributeValueText(float CurrentValue, float MaxValue)
	{
		return FText::Format(
			NSLOCTEXT("ArenaPlayerHUDWidget", "AttributeValueFormat", "{0} / {1}"),
			FText::AsNumber(FMath::RoundToInt(CurrentValue)),
			FText::AsNumber(FMath::RoundToInt(MaxValue)));
	}
}

void UArenaPlayerHUDWidget::BindToAbilitySystem(UArenaAbilitySystemComponent* InAbilitySystemComponent, UArenaAttributeSet* InAttributeSet)
{
	UnbindFromAbilitySystem();

	if (!InAbilitySystemComponent || !InAttributeSet)
	{
		return;
	}

	BoundAbilitySystemComponent = InAbilitySystemComponent;
	BoundAttributeSet = InAttributeSet;

	// 玩家 HUD 只观察 PlayerState 上的 ASC/AttributeSet，保持 UI 与玩法状态分离。
	HealthChangedDelegateHandle = InAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UArenaAttributeSet::GetHealthAttribute()).AddUObject(this, &UArenaPlayerHUDWidget::HandleHealthChanged);
	MaxHealthChangedDelegateHandle = InAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UArenaAttributeSet::GetMaxHealthAttribute()).AddUObject(this, &UArenaPlayerHUDWidget::HandleMaxHealthChanged);
	ShieldChangedDelegateHandle = InAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UArenaAttributeSet::GetShieldAttribute()).AddUObject(this, &UArenaPlayerHUDWidget::HandleShieldChanged);
	MaxShieldChangedDelegateHandle = InAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UArenaAttributeSet::GetMaxShieldAttribute()).AddUObject(this, &UArenaPlayerHUDWidget::HandleMaxShieldChanged);
	EnergyChangedDelegateHandle = InAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UArenaAttributeSet::GetEnergyAttribute()).AddUObject(this, &UArenaPlayerHUDWidget::HandleEnergyChanged);
	MaxEnergyChangedDelegateHandle = InAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UArenaAttributeSet::GetMaxEnergyAttribute()).AddUObject(this, &UArenaPlayerHUDWidget::HandleMaxEnergyChanged);

	// RegisterAndCall 只会在标签 count > 0 时立即回调，所以这里先主动刷新一次可用状态。
	SetBasicAttackCooldownActive(InAbilitySystemComponent->HasMatchingGameplayTag(ArenaGameplayTags::Cooldown_BasicAttack));
	BasicAttackCooldownTagDelegateHandle = InAbilitySystemComponent->RegisterAndCallGameplayTagEvent(
		ArenaGameplayTags::Cooldown_BasicAttack,
		FOnGameplayEffectTagCountChanged::FDelegate::CreateUObject(this, &UArenaPlayerHUDWidget::HandleBasicAttackCooldownChanged),
		EGameplayTagEventType::NewOrRemoved);

	RefreshAttributeValues();
}

void UArenaPlayerHUDWidget::SetHealthValues(float InHealth, float InMaxHealth)
{
	CurrentHealth = FMath::Max(InHealth, 0.0f);
	CurrentMaxHealth = FMath::Max(InMaxHealth, 0.0f);
	HealthPercent = CalculatePercent(CurrentHealth, CurrentMaxHealth);

	if (HealthProgressBar)
	{
		HealthProgressBar->SetPercent(HealthPercent);
	}

	if (HealthText)
	{
		HealthText->SetText(MakeAttributeValueText(CurrentHealth, CurrentMaxHealth));
	}
}

void UArenaPlayerHUDWidget::SetShieldValues(float InShield, float InMaxShield)
{
	CurrentShield = FMath::Max(InShield, 0.0f);
	CurrentMaxShield = FMath::Max(InMaxShield, 0.0f);
	ShieldPercent = CalculatePercent(CurrentShield, CurrentMaxShield);

	if (ShieldProgressBar)
	{
		ShieldProgressBar->SetPercent(ShieldPercent);
	}

	if (ShieldText)
	{
		ShieldText->SetText(MakeAttributeValueText(CurrentShield, CurrentMaxShield));
	}
}

void UArenaPlayerHUDWidget::SetEnergyValues(float InEnergy, float InMaxEnergy)
{
	CurrentEnergy = FMath::Max(InEnergy, 0.0f);
	CurrentMaxEnergy = FMath::Max(InMaxEnergy, 0.0f);
	EnergyPercent = CalculatePercent(CurrentEnergy, CurrentMaxEnergy);

	if (EnergyProgressBar)
	{
		EnergyProgressBar->SetPercent(EnergyPercent);
	}

	if (EnergyText)
	{
		EnergyText->SetText(MakeAttributeValueText(CurrentEnergy, CurrentMaxEnergy));
	}
}

void UArenaPlayerHUDWidget::SetBasicAttackCooldownActive(bool bInCooldownActive)
{
	bBasicAttackCooldownActive = bInCooldownActive;

	if (BasicAttackCooldownText)
	{
		BasicAttackCooldownText->SetText(bBasicAttackCooldownActive
			? NSLOCTEXT("ArenaPlayerHUDWidget", "BasicAttackCooldown", "Basic Attack: Cooldown")
			: NSLOCTEXT("ArenaPlayerHUDWidget", "BasicAttackReady", "Basic Attack: Ready"));
	}
}

void UArenaPlayerHUDWidget::NativeDestruct()
{
	UnbindFromAbilitySystem();

	Super::NativeDestruct();
}

void UArenaPlayerHUDWidget::UnbindFromAbilitySystem()
{
	if (UArenaAbilitySystemComponent* AbilitySystemComponent = BoundAbilitySystemComponent.Get())
	{
		// Attribute 和 Tag 委托都挂在 ASC 上，Widget 重建或销毁时必须移除。
		if (HealthChangedDelegateHandle.IsValid())
		{
			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UArenaAttributeSet::GetHealthAttribute()).Remove(HealthChangedDelegateHandle);
			HealthChangedDelegateHandle.Reset();
		}

		if (MaxHealthChangedDelegateHandle.IsValid())
		{
			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UArenaAttributeSet::GetMaxHealthAttribute()).Remove(MaxHealthChangedDelegateHandle);
			MaxHealthChangedDelegateHandle.Reset();
		}

		if (ShieldChangedDelegateHandle.IsValid())
		{
			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UArenaAttributeSet::GetShieldAttribute()).Remove(ShieldChangedDelegateHandle);
			ShieldChangedDelegateHandle.Reset();
		}

		if (MaxShieldChangedDelegateHandle.IsValid())
		{
			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UArenaAttributeSet::GetMaxShieldAttribute()).Remove(MaxShieldChangedDelegateHandle);
			MaxShieldChangedDelegateHandle.Reset();
		}

		if (EnergyChangedDelegateHandle.IsValid())
		{
			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UArenaAttributeSet::GetEnergyAttribute()).Remove(EnergyChangedDelegateHandle);
			EnergyChangedDelegateHandle.Reset();
		}

		if (MaxEnergyChangedDelegateHandle.IsValid())
		{
			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UArenaAttributeSet::GetMaxEnergyAttribute()).Remove(MaxEnergyChangedDelegateHandle);
			MaxEnergyChangedDelegateHandle.Reset();
		}

		if (BasicAttackCooldownTagDelegateHandle.IsValid())
		{
			AbilitySystemComponent->UnregisterGameplayTagEvent(
				BasicAttackCooldownTagDelegateHandle,
				ArenaGameplayTags::Cooldown_BasicAttack,
				EGameplayTagEventType::NewOrRemoved);
			BasicAttackCooldownTagDelegateHandle.Reset();
		}
	}

	BoundAbilitySystemComponent.Reset();
	BoundAttributeSet.Reset();
}

void UArenaPlayerHUDWidget::RefreshAttributeValues()
{
	if (const UArenaAttributeSet* AttributeSet = BoundAttributeSet.Get())
	{
		SetHealthValues(AttributeSet->GetHealth(), AttributeSet->GetMaxHealth());
		SetShieldValues(AttributeSet->GetShield(), AttributeSet->GetMaxShield());
		SetEnergyValues(AttributeSet->GetEnergy(), AttributeSet->GetMaxEnergy());
	}
}

void UArenaPlayerHUDWidget::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	SetHealthValues(Data.NewValue, CurrentMaxHealth);
}

void UArenaPlayerHUDWidget::HandleMaxHealthChanged(const FOnAttributeChangeData& Data)
{
	SetHealthValues(CurrentHealth, Data.NewValue);
}

void UArenaPlayerHUDWidget::HandleShieldChanged(const FOnAttributeChangeData& Data)
{
	SetShieldValues(Data.NewValue, CurrentMaxShield);
}

void UArenaPlayerHUDWidget::HandleMaxShieldChanged(const FOnAttributeChangeData& Data)
{
	SetShieldValues(CurrentShield, Data.NewValue);
}

void UArenaPlayerHUDWidget::HandleEnergyChanged(const FOnAttributeChangeData& Data)
{
	SetEnergyValues(Data.NewValue, CurrentMaxEnergy);
}

void UArenaPlayerHUDWidget::HandleMaxEnergyChanged(const FOnAttributeChangeData& Data)
{
	SetEnergyValues(CurrentEnergy, Data.NewValue);
}

void UArenaPlayerHUDWidget::HandleBasicAttackCooldownChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	SetBasicAttackCooldownActive(NewCount > 0);
}
