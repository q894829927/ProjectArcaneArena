#include "UI/ArenaPlayerHUDWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GAS/ArenaAbilitySystemComponent.h"
#include "GAS/ArenaAttributeSet.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffect.h"

namespace
{
	float CalculatePercent(float CurrentValue, float MaxValue)
	{
		return MaxValue > 0.0f
			? FMath::Clamp(CurrentValue / MaxValue, 0.0f, 1.0f)
			: 0.0f;
	}

	FText MakeAttributeValueText(const FText& Label, float CurrentValue, float MaxValue)
	{
		return FText::Format(
			NSLOCTEXT("ArenaPlayerHUDWidget", "AttributeValueFormat", "{0} {1} / {2}"),
			Label,
			FText::AsNumber(FMath::RoundToInt(CurrentValue)),
			FText::AsNumber(FMath::RoundToInt(MaxValue)));
	}

	FText MakeCooldownSecondsText(float RemainingTime)
	{
		FNumberFormattingOptions NumberFormat;
		NumberFormat.MinimumFractionalDigits = 1;
		NumberFormat.MaximumFractionalDigits = 1;

		return FText::AsNumber(FMath::Max(RemainingTime, 0.0f), &NumberFormat);
	}

	FText MakeBasicAttackFullText(bool bIsCooldownActive, float RemainingTime)
	{
		return bIsCooldownActive
			? FText::Format(
				NSLOCTEXT("ArenaPlayerHUDWidget", "BasicAttackCooldownFormat", "LMB Basic {0}s"),
				MakeCooldownSecondsText(RemainingTime))
			: NSLOCTEXT("ArenaPlayerHUDWidget", "BasicAttackReady", "LMB Basic Ready");
	}

	FText MakeBasicAttackSlotText(bool bIsCooldownActive, float RemainingTime)
	{
		return bIsCooldownActive
			? FText::Format(
				NSLOCTEXT("ArenaPlayerHUDWidget", "BasicAttackSlotCooldownFormat", "{0}s"),
				MakeCooldownSecondsText(RemainingTime))
			: NSLOCTEXT("ArenaPlayerHUDWidget", "BasicAttackSlotReady", "Basic Ready");
	}

	FText MakeFireballSlotText(bool bHasFireballAbility, bool bIsCooldownActive, float RemainingTime)
	{
		if (!bHasFireballAbility)
		{
			return NSLOCTEXT("ArenaPlayerHUDWidget", "FireballSlotLocked", "Locked");
		}

		return bIsCooldownActive
			? FText::Format(
				NSLOCTEXT("ArenaPlayerHUDWidget", "FireballSlotCooldownFormat", "{0}s"),
				MakeCooldownSecondsText(RemainingTime))
			: NSLOCTEXT("ArenaPlayerHUDWidget", "FireballSlotReady", "Fireball Ready");
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
	RefreshSkillSlotPlaceholders();
	RefreshBasicAttackCooldownFromAbilitySystem();
	RefreshFireballCooldownFromAbilitySystem();
	BasicAttackCooldownTagDelegateHandle = InAbilitySystemComponent->RegisterAndCallGameplayTagEvent(
		ArenaGameplayTags::Cooldown_BasicAttack,
		FOnGameplayEffectTagCountChanged::FDelegate::CreateUObject(this, &UArenaPlayerHUDWidget::HandleBasicAttackCooldownChanged),
		EGameplayTagEventType::NewOrRemoved);
	FireballCooldownTagDelegateHandle = InAbilitySystemComponent->RegisterAndCallGameplayTagEvent(
		ArenaGameplayTags::Cooldown_Fireball,
		FOnGameplayEffectTagCountChanged::FDelegate::CreateUObject(this, &UArenaPlayerHUDWidget::HandleFireballCooldownChanged),
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
		HealthText->SetText(MakeAttributeValueText(NSLOCTEXT("ArenaPlayerHUDWidget", "HealthLabel", "HP"), CurrentHealth, CurrentMaxHealth));
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
		ShieldText->SetText(MakeAttributeValueText(NSLOCTEXT("ArenaPlayerHUDWidget", "ShieldLabel", "Shield"), CurrentShield, CurrentMaxShield));
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
		EnergyText->SetText(MakeAttributeValueText(NSLOCTEXT("ArenaPlayerHUDWidget", "EnergyLabel", "Energy"), CurrentEnergy, CurrentMaxEnergy));
	}
}

void UArenaPlayerHUDWidget::SetBasicAttackCooldownActive(bool bInCooldownActive)
{
	SetBasicAttackCooldownValues(bInCooldownActive, 0.0f, 0.0f);
}

void UArenaPlayerHUDWidget::SetBasicAttackCooldownValues(bool bInCooldownActive, float InRemainingTime, float InDuration)
{
	bBasicAttackCooldownActive = bInCooldownActive;
	BasicAttackCooldownRemaining = bBasicAttackCooldownActive ? FMath::Max(InRemainingTime, 0.0f) : 0.0f;
	BasicAttackCooldownDuration = bBasicAttackCooldownActive ? FMath::Max(InDuration, 0.0f) : 0.0f;
	BasicAttackCooldownPercent = BasicAttackCooldownDuration > 0.0f
		? FMath::Clamp(BasicAttackCooldownRemaining / BasicAttackCooldownDuration, 0.0f, 1.0f)
		: 0.0f;

	if (BasicAttackCooldownText)
	{
		BasicAttackCooldownText->SetText(MakeBasicAttackFullText(bBasicAttackCooldownActive, BasicAttackCooldownRemaining));
	}

	if (BasicAttackSlotText)
	{
		BasicAttackSlotText->SetText(MakeBasicAttackSlotText(bBasicAttackCooldownActive, BasicAttackCooldownRemaining));
	}

	if (BasicAttackCooldownProgressBar)
	{
		BasicAttackCooldownProgressBar->SetPercent(BasicAttackCooldownPercent);
	}
}

void UArenaPlayerHUDWidget::SetFireballCooldownValues(bool bInCooldownActive, float InRemainingTime, float InDuration)
{
	const bool bHasFireballAbility = HasGrantedAbilityForInputTag(ArenaGameplayTags::Ability_Fireball);
	bFireballCooldownActive = bHasFireballAbility && bInCooldownActive;
	FireballCooldownRemaining = bFireballCooldownActive ? FMath::Max(InRemainingTime, 0.0f) : 0.0f;
	FireballCooldownDuration = bFireballCooldownActive ? FMath::Max(InDuration, 0.0f) : 0.0f;
	FireballCooldownPercent = FireballCooldownDuration > 0.0f
		? FMath::Clamp(FireballCooldownRemaining / FireballCooldownDuration, 0.0f, 1.0f)
		: 0.0f;

	if (FireballSlotText)
	{
		FireballSlotText->SetText(MakeFireballSlotText(bHasFireballAbility, bFireballCooldownActive, FireballCooldownRemaining));
	}

	if (FireballCooldownProgressBar)
	{
		FireballCooldownProgressBar->SetPercent(FireballCooldownPercent);
	}
}

void UArenaPlayerHUDWidget::NativeDestruct()
{
	UnbindFromAbilitySystem();

	Super::NativeDestruct();
}

void UArenaPlayerHUDWidget::UnbindFromAbilitySystem()
{
	StopBasicAttackCooldownTimer();
	StopFireballCooldownTimer();

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

		if (FireballCooldownTagDelegateHandle.IsValid())
		{
			AbilitySystemComponent->UnregisterGameplayTagEvent(
				FireballCooldownTagDelegateHandle,
				ArenaGameplayTags::Cooldown_Fireball,
				EGameplayTagEventType::NewOrRemoved);
			FireballCooldownTagDelegateHandle.Reset();
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

void UArenaPlayerHUDWidget::RefreshSkillSlotPlaceholders()
{
	if (FireballSlotText)
	{
		const bool bHasFireballAbility = HasGrantedAbilityForInputTag(ArenaGameplayTags::Ability_Fireball);
		FireballSlotText->SetText(MakeFireballSlotText(bHasFireballAbility, false, 0.0f));
	}

	if (DashSlotText)
	{
		DashSlotText->SetText(NSLOCTEXT("ArenaPlayerHUDWidget", "DashSlotLocked", "Locked"));
	}

	if (ShieldSlotText)
	{
		ShieldSlotText->SetText(NSLOCTEXT("ArenaPlayerHUDWidget", "ShieldSlotLocked", "Locked"));
	}

	if (UltimateSlotText)
	{
		UltimateSlotText->SetText(NSLOCTEXT("ArenaPlayerHUDWidget", "UltimateSlotLocked", "Locked"));
	}
}

void UArenaPlayerHUDWidget::RefreshBasicAttackCooldownFromAbilitySystem()
{
	float RemainingTime = 0.0f;
	float Duration = 0.0f;
	const bool bHasCooldownTime = GetBasicAttackCooldownTime(RemainingTime, Duration);

	if (const UArenaAbilitySystemComponent* AbilitySystemComponent = BoundAbilitySystemComponent.Get())
	{
		const bool bHasCooldownTag = AbilitySystemComponent->HasMatchingGameplayTag(ArenaGameplayTags::Cooldown_BasicAttack);
		SetBasicAttackCooldownValues(bHasCooldownTag, bHasCooldownTime ? RemainingTime : 0.0f, bHasCooldownTime ? Duration : 0.0f);

		if (bHasCooldownTag)
		{
			StartBasicAttackCooldownTimer();
		}
		else
		{
			StopBasicAttackCooldownTimer();
		}
	}
	else
	{
		SetBasicAttackCooldownValues(false, 0.0f, 0.0f);
		StopBasicAttackCooldownTimer();
	}
}

void UArenaPlayerHUDWidget::RefreshFireballCooldownFromAbilitySystem()
{
	float RemainingTime = 0.0f;
	float Duration = 0.0f;
	const bool bHasCooldownTime = GetCooldownTimeForTag(ArenaGameplayTags::Cooldown_Fireball, RemainingTime, Duration);

	if (const UArenaAbilitySystemComponent* AbilitySystemComponent = BoundAbilitySystemComponent.Get())
	{
		const bool bHasCooldownTag = AbilitySystemComponent->HasMatchingGameplayTag(ArenaGameplayTags::Cooldown_Fireball);
		SetFireballCooldownValues(bHasCooldownTag, bHasCooldownTime ? RemainingTime : 0.0f, bHasCooldownTime ? Duration : 0.0f);

		if (bHasCooldownTag)
		{
			StartFireballCooldownTimer();
		}
		else
		{
			StopFireballCooldownTimer();
		}
	}
	else
	{
		SetFireballCooldownValues(false, 0.0f, 0.0f);
		StopFireballCooldownTimer();
	}
}

void UArenaPlayerHUDWidget::StartBasicAttackCooldownTimer()
{
	if (!GetWorld() || GetWorld()->GetTimerManager().IsTimerActive(BasicAttackCooldownTimerHandle))
	{
		return;
	}

	// 冷却数字是纯表现数据，0.05 秒刷新足够平滑，也避免每帧 Tick。
	GetWorld()->GetTimerManager().SetTimer(
		BasicAttackCooldownTimerHandle,
		this,
		&UArenaPlayerHUDWidget::RefreshBasicAttackCooldownFromAbilitySystem,
		0.05f,
		true);
}

void UArenaPlayerHUDWidget::StartFireballCooldownTimer()
{
	if (!GetWorld() || GetWorld()->GetTimerManager().IsTimerActive(FireballCooldownTimerHandle))
	{
		return;
	}

	// Fireball 使用独立定时器，后续不同技能冷却刷新频率可以单独调。
	GetWorld()->GetTimerManager().SetTimer(
		FireballCooldownTimerHandle,
		this,
		&UArenaPlayerHUDWidget::RefreshFireballCooldownFromAbilitySystem,
		0.05f,
		true);
}

void UArenaPlayerHUDWidget::StopBasicAttackCooldownTimer()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(BasicAttackCooldownTimerHandle);
	}
}

void UArenaPlayerHUDWidget::StopFireballCooldownTimer()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(FireballCooldownTimerHandle);
	}
}

bool UArenaPlayerHUDWidget::GetBasicAttackCooldownTime(float& OutRemainingTime, float& OutDuration) const
{
	return GetCooldownTimeForTag(ArenaGameplayTags::Cooldown_BasicAttack, OutRemainingTime, OutDuration);
}

bool UArenaPlayerHUDWidget::GetCooldownTimeForTag(const FGameplayTag& CooldownTag, float& OutRemainingTime, float& OutDuration) const
{
	OutRemainingTime = 0.0f;
	OutDuration = 0.0f;

	const UArenaAbilitySystemComponent* AbilitySystemComponent = BoundAbilitySystemComponent.Get();
	if (!AbilitySystemComponent || !CooldownTag.IsValid())
	{
		return false;
	}

	FGameplayTagContainer CooldownTags;
	CooldownTags.AddTag(CooldownTag);
	const FGameplayEffectQuery CooldownQuery = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(CooldownTags);
	const TArray<TPair<float, float>> DurationAndTimeRemaining = AbilitySystemComponent->GetActiveEffectsTimeRemainingAndDuration(CooldownQuery);
	if (DurationAndTimeRemaining.Num() == 0)
	{
		return false;
	}

	// 与 GameplayAbility 默认冷却查询一致：多个冷却效果存在时显示剩余时间最长的一个。
	for (const TPair<float, float>& CooldownTime : DurationAndTimeRemaining)
	{
		if (CooldownTime.Key > OutRemainingTime)
		{
			OutRemainingTime = CooldownTime.Key;
			OutDuration = CooldownTime.Value;
		}
	}

	return true;
}

bool UArenaPlayerHUDWidget::HasGrantedAbilityForInputTag(const FGameplayTag& InputTag) const
{
	const UArenaAbilitySystemComponent* AbilitySystemComponent = BoundAbilitySystemComponent.Get();
	if (!AbilitySystemComponent || !InputTag.IsValid())
	{
		return false;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : AbilitySystemComponent->GetActivatableAbilities())
	{
		if (AbilitySpec.Ability && AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			return true;
		}
	}

	return false;
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
	if (CallbackTag == ArenaGameplayTags::Cooldown_BasicAttack && NewCount <= 0)
	{
		SetBasicAttackCooldownValues(false, 0.0f, 0.0f);
		StopBasicAttackCooldownTimer();
		return;
	}

	RefreshBasicAttackCooldownFromAbilitySystem();
}

void UArenaPlayerHUDWidget::HandleFireballCooldownChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	if (CallbackTag == ArenaGameplayTags::Cooldown_Fireball && NewCount <= 0)
	{
		SetFireballCooldownValues(false, 0.0f, 0.0f);
		StopFireballCooldownTimer();
		return;
	}

	RefreshFireballCooldownFromAbilitySystem();
}
