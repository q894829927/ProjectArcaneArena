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

	FText MakeShieldValueText(float CurrentValue)
	{
		return FText::Format(
			NSLOCTEXT("ArenaPlayerHUDWidget", "ShieldValueFormat", "Shield {0}"),
			FText::AsNumber(FMath::RoundToInt(CurrentValue)));
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

	FText MakeDashSlotText(bool bHasDashAbility, bool bIsCooldownActive, float RemainingTime)
	{
		if (!bHasDashAbility)
		{
			return NSLOCTEXT("ArenaPlayerHUDWidget", "DashSlotLocked", "Locked");
		}

		return bIsCooldownActive
			? FText::Format(
				NSLOCTEXT("ArenaPlayerHUDWidget", "DashSlotCooldownFormat", "{0}s"),
				MakeCooldownSecondsText(RemainingTime))
			: NSLOCTEXT("ArenaPlayerHUDWidget", "DashSlotReady", "Dash Ready");
	}

	FText MakeShieldSlotText(bool bHasShieldAbility, bool bIsCooldownActive, float RemainingTime)
	{
		if (!bHasShieldAbility)
		{
			return NSLOCTEXT("ArenaPlayerHUDWidget", "ShieldSlotLocked", "Locked");
		}

		return bIsCooldownActive
			? FText::Format(
				NSLOCTEXT("ArenaPlayerHUDWidget", "ShieldSlotCooldownFormat", "{0}s"),
				MakeCooldownSecondsText(RemainingTime))
			: NSLOCTEXT("ArenaPlayerHUDWidget", "ShieldSlotReady", "Shield Ready");
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
	EnergyChangedDelegateHandle = InAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UArenaAttributeSet::GetEnergyAttribute()).AddUObject(this, &UArenaPlayerHUDWidget::HandleEnergyChanged);
	MaxEnergyChangedDelegateHandle = InAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UArenaAttributeSet::GetMaxEnergyAttribute()).AddUObject(this, &UArenaPlayerHUDWidget::HandleMaxEnergyChanged);

	// RegisterAndCall 只会在标签 count > 0 时立即回调，所以这里先主动刷新一次可用状态。
	RefreshSkillSlotPlaceholders();
	RefreshBasicAttackCooldownFromAbilitySystem();
	RefreshFireballCooldownFromAbilitySystem();
	RefreshDashCooldownFromAbilitySystem();
	RefreshShieldCooldownFromAbilitySystem();
	BasicAttackCooldownTagDelegateHandle = InAbilitySystemComponent->RegisterAndCallGameplayTagEvent(
		ArenaGameplayTags::Cooldown_BasicAttack,
		FOnGameplayEffectTagCountChanged::FDelegate::CreateUObject(this, &UArenaPlayerHUDWidget::HandleBasicAttackCooldownChanged),
		EGameplayTagEventType::NewOrRemoved);
	FireballCooldownTagDelegateHandle = InAbilitySystemComponent->RegisterAndCallGameplayTagEvent(
		ArenaGameplayTags::Cooldown_Fireball,
		FOnGameplayEffectTagCountChanged::FDelegate::CreateUObject(this, &UArenaPlayerHUDWidget::HandleFireballCooldownChanged),
		EGameplayTagEventType::NewOrRemoved);
	DashCooldownTagDelegateHandle = InAbilitySystemComponent->RegisterAndCallGameplayTagEvent(
		ArenaGameplayTags::Cooldown_Dash,
		FOnGameplayEffectTagCountChanged::FDelegate::CreateUObject(this, &UArenaPlayerHUDWidget::HandleDashCooldownChanged),
		EGameplayTagEventType::NewOrRemoved);
	ShieldCooldownTagDelegateHandle = InAbilitySystemComponent->RegisterAndCallGameplayTagEvent(
		ArenaGameplayTags::Cooldown_Shield,
		FOnGameplayEffectTagCountChanged::FDelegate::CreateUObject(this, &UArenaPlayerHUDWidget::HandleShieldCooldownChanged),
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

void UArenaPlayerHUDWidget::SetShieldValues(float InShield)
{
	CurrentShield = FMath::Max(InShield, 0.0f);
	ShieldPercent = CurrentShield > 0.0f ? 1.0f : 0.0f;

	if (ShieldProgressBar)
	{
		// Shield 没有最大值，进度条暂时只表示是否存在护盾。
		ShieldProgressBar->SetPercent(ShieldPercent);
	}

	if (ShieldText)
	{
		ShieldText->SetText(MakeShieldValueText(CurrentShield));
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

void UArenaPlayerHUDWidget::SetDashCooldownValues(bool bInCooldownActive, float InRemainingTime, float InDuration)
{
	const bool bHasDashAbility = HasGrantedAbilityForInputTag(ArenaGameplayTags::Ability_Dash);
	bDashCooldownActive = bHasDashAbility && bInCooldownActive;
	DashCooldownRemaining = bDashCooldownActive ? FMath::Max(InRemainingTime, 0.0f) : 0.0f;
	DashCooldownDuration = bDashCooldownActive ? FMath::Max(InDuration, 0.0f) : 0.0f;
	DashCooldownPercent = DashCooldownDuration > 0.0f
		? FMath::Clamp(DashCooldownRemaining / DashCooldownDuration, 0.0f, 1.0f)
		: 0.0f;

	if (DashSlotText)
	{
		DashSlotText->SetText(MakeDashSlotText(bHasDashAbility, bDashCooldownActive, DashCooldownRemaining));
	}

	if (DashCooldownProgressBar)
	{
		DashCooldownProgressBar->SetPercent(DashCooldownPercent);
	}
}

void UArenaPlayerHUDWidget::SetShieldCooldownValues(bool bInCooldownActive, float InRemainingTime, float InDuration)
{
	const bool bHasShieldAbility = HasGrantedAbilityForInputTag(ArenaGameplayTags::Ability_Shield);
	bShieldCooldownActive = bHasShieldAbility && bInCooldownActive;
	ShieldCooldownRemaining = bShieldCooldownActive ? FMath::Max(InRemainingTime, 0.0f) : 0.0f;
	ShieldCooldownDuration = bShieldCooldownActive ? FMath::Max(InDuration, 0.0f) : 0.0f;
	ShieldCooldownPercent = ShieldCooldownDuration > 0.0f
		? FMath::Clamp(ShieldCooldownRemaining / ShieldCooldownDuration, 0.0f, 1.0f)
		: 0.0f;

	if (ShieldSlotText)
	{
		ShieldSlotText->SetText(MakeShieldSlotText(bHasShieldAbility, bShieldCooldownActive, ShieldCooldownRemaining));
	}

	if (ShieldCooldownProgressBar)
	{
		ShieldCooldownProgressBar->SetPercent(ShieldCooldownPercent);
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
	StopDashCooldownTimer();
	StopShieldCooldownTimer();

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

		if (DashCooldownTagDelegateHandle.IsValid())
		{
			AbilitySystemComponent->UnregisterGameplayTagEvent(
				DashCooldownTagDelegateHandle,
				ArenaGameplayTags::Cooldown_Dash,
				EGameplayTagEventType::NewOrRemoved);
			DashCooldownTagDelegateHandle.Reset();
		}

		if (ShieldCooldownTagDelegateHandle.IsValid())
		{
			AbilitySystemComponent->UnregisterGameplayTagEvent(
				ShieldCooldownTagDelegateHandle,
				ArenaGameplayTags::Cooldown_Shield,
				EGameplayTagEventType::NewOrRemoved);
			ShieldCooldownTagDelegateHandle.Reset();
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
		SetShieldValues(AttributeSet->GetShield());
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
		const bool bHasDashAbility = HasGrantedAbilityForInputTag(ArenaGameplayTags::Ability_Dash);
		DashSlotText->SetText(MakeDashSlotText(bHasDashAbility, false, 0.0f));
	}

	if (ShieldSlotText)
	{
		const bool bHasShieldAbility = HasGrantedAbilityForInputTag(ArenaGameplayTags::Ability_Shield);
		ShieldSlotText->SetText(MakeShieldSlotText(bHasShieldAbility, false, 0.0f));
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

void UArenaPlayerHUDWidget::RefreshDashCooldownFromAbilitySystem()
{
	float RemainingTime = 0.0f;
	float Duration = 0.0f;
	const bool bHasCooldownTime = GetCooldownTimeForTag(ArenaGameplayTags::Cooldown_Dash, RemainingTime, Duration);

	if (const UArenaAbilitySystemComponent* AbilitySystemComponent = BoundAbilitySystemComponent.Get())
	{
		const bool bHasCooldownTag = AbilitySystemComponent->HasMatchingGameplayTag(ArenaGameplayTags::Cooldown_Dash);
		SetDashCooldownValues(bHasCooldownTag, bHasCooldownTime ? RemainingTime : 0.0f, bHasCooldownTime ? Duration : 0.0f);

		if (bHasCooldownTag)
		{
			StartDashCooldownTimer();
		}
		else
		{
			StopDashCooldownTimer();
		}
	}
	else
	{
		SetDashCooldownValues(false, 0.0f, 0.0f);
		StopDashCooldownTimer();
	}
}

void UArenaPlayerHUDWidget::RefreshShieldCooldownFromAbilitySystem()
{
	float RemainingTime = 0.0f;
	float Duration = 0.0f;
	const bool bHasCooldownTime = GetCooldownTimeForTag(ArenaGameplayTags::Cooldown_Shield, RemainingTime, Duration);

	if (const UArenaAbilitySystemComponent* AbilitySystemComponent = BoundAbilitySystemComponent.Get())
	{
		const bool bHasCooldownTag = AbilitySystemComponent->HasMatchingGameplayTag(ArenaGameplayTags::Cooldown_Shield);
		SetShieldCooldownValues(bHasCooldownTag, bHasCooldownTime ? RemainingTime : 0.0f, bHasCooldownTime ? Duration : 0.0f);

		if (bHasCooldownTag)
		{
			StartShieldCooldownTimer();
		}
		else
		{
			StopShieldCooldownTimer();
		}
	}
	else
	{
		SetShieldCooldownValues(false, 0.0f, 0.0f);
		StopShieldCooldownTimer();
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

void UArenaPlayerHUDWidget::StartDashCooldownTimer()
{
	if (!GetWorld() || GetWorld()->GetTimerManager().IsTimerActive(DashCooldownTimerHandle))
	{
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(
		DashCooldownTimerHandle,
		this,
		&UArenaPlayerHUDWidget::RefreshDashCooldownFromAbilitySystem,
		0.05f,
		true);
}

void UArenaPlayerHUDWidget::StartShieldCooldownTimer()
{
	if (!GetWorld() || GetWorld()->GetTimerManager().IsTimerActive(ShieldCooldownTimerHandle))
	{
		return;
	}

	// Shield 使用独立定时器，后续若加入护盾表现或音效可单独调整刷新策略。
	GetWorld()->GetTimerManager().SetTimer(
		ShieldCooldownTimerHandle,
		this,
		&UArenaPlayerHUDWidget::RefreshShieldCooldownFromAbilitySystem,
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

void UArenaPlayerHUDWidget::StopDashCooldownTimer()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(DashCooldownTimerHandle);
	}
}

void UArenaPlayerHUDWidget::StopShieldCooldownTimer()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ShieldCooldownTimerHandle);
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
	SetShieldValues(Data.NewValue);
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

void UArenaPlayerHUDWidget::HandleDashCooldownChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	if (CallbackTag == ArenaGameplayTags::Cooldown_Dash && NewCount <= 0)
	{
		SetDashCooldownValues(false, 0.0f, 0.0f);
		StopDashCooldownTimer();
		return;
	}

	RefreshDashCooldownFromAbilitySystem();
}

void UArenaPlayerHUDWidget::HandleShieldCooldownChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	if (CallbackTag == ArenaGameplayTags::Cooldown_Shield && NewCount <= 0)
	{
		SetShieldCooldownValues(false, 0.0f, 0.0f);
		StopShieldCooldownTimer();
		return;
	}

	RefreshShieldCooldownFromAbilitySystem();
}
