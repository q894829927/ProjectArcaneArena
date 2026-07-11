#include "UI/ArenaPlayerHUDWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GAS/ArenaAbilitySystemComponent.h"
#include "GAS/ArenaAttributeSet.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffect.h"

namespace
{
	// 计算当前值相对最大值的 UI 百分比，最大值无效时显示为空。
	float CalculatePercent(float CurrentValue, float MaxValue)
	{
		return MaxValue > 0.0f
			? FMath::Clamp(CurrentValue / MaxValue, 0.0f, 1.0f)
			: 0.0f;
	}

	// 生成生命/能量这类 Current/Max 属性的显示文本。
	FText MakeAttributeValueText(const FText& Label, float CurrentValue, float MaxValue)
	{
		return FText::Format(
			NSLOCTEXT("ArenaPlayerHUDWidget", "AttributeValueFormat", "{0} {1} / {2}"),
			Label,
			FText::AsNumber(FMath::RoundToInt(CurrentValue)),
			FText::AsNumber(FMath::RoundToInt(MaxValue)));
	}

	// 生成护盾显示文本，护盾当前没有 MaxShield。
	FText MakeShieldValueText(float CurrentValue)
	{
		return FText::Format(
			NSLOCTEXT("ArenaPlayerHUDWidget", "ShieldValueFormat", "Shield {0}"),
			FText::AsNumber(FMath::RoundToInt(CurrentValue)));
	}

	// 将冷却剩余时间格式化为一位小数秒数。
	FText MakeCooldownSecondsText(float RemainingTime)
	{
		FNumberFormattingOptions NumberFormat;
		NumberFormat.MinimumFractionalDigits = 1;
		NumberFormat.MaximumFractionalDigits = 1;

		return FText::AsNumber(FMath::Max(RemainingTime, 0.0f), &NumberFormat);
	}

	// 生成基础攻击完整冷却文本，用于主冷却提示。
	FText MakeBasicAttackFullText(bool bIsCooldownActive, float RemainingTime)
	{
		return bIsCooldownActive
			? FText::Format(
				NSLOCTEXT("ArenaPlayerHUDWidget", "BasicAttackCooldownFormat", "LMB Basic {0}s"),
				MakeCooldownSecondsText(RemainingTime))
			: NSLOCTEXT("ArenaPlayerHUDWidget", "BasicAttackReady", "LMB Basic Ready");
	}

	// 生成基础攻击技能槽文本，用于槽位上的简短状态。
	FText MakeBasicAttackSlotText(bool bIsCooldownActive, float RemainingTime)
	{
		return bIsCooldownActive
			? FText::Format(
				NSLOCTEXT("ArenaPlayerHUDWidget", "BasicAttackSlotCooldownFormat", "{0}s"),
				MakeCooldownSecondsText(RemainingTime))
			: NSLOCTEXT("ArenaPlayerHUDWidget", "BasicAttackSlotReady", "Basic Ready");
	}

	// 生成火球技能槽文本，兼顾未解锁和冷却状态。
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

	// 生成冲刺技能槽文本，兼顾未解锁和冷却状态。
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

	// 生成护盾技能槽文本，兼顾未解锁和冷却状态。
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

	// 生成闪电风暴技能槽文本，显示授予状态和冷却倒计时。
	FText MakeLightningStormSlotText(bool bHasLightningStormAbility, bool bIsCooldownActive, float RemainingTime)
	{
		if (!bHasLightningStormAbility)
		{
			return NSLOCTEXT("ArenaPlayerHUDWidget", "LightningStormSlotLocked", "Locked");
		}

		return bIsCooldownActive
			? FText::Format(
				NSLOCTEXT("ArenaPlayerHUDWidget", "LightningStormSlotCooldownFormat", "{0}s"),
				MakeCooldownSecondsText(RemainingTime))
			: NSLOCTEXT("ArenaPlayerHUDWidget", "LightningStormSlotReady", "Storm Ready");
	}
}

// 绑定玩家 HUD 到 ASC/AttributeSet，并注册属性和冷却标签监听。
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
	RefreshLightningStormCooldownFromAbilitySystem();
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
	LightningStormCooldownTagDelegateHandle = InAbilitySystemComponent->RegisterAndCallGameplayTagEvent(
		ArenaGameplayTags::Cooldown_LightningStorm,
		FOnGameplayEffectTagCountChanged::FDelegate::CreateUObject(this, &UArenaPlayerHUDWidget::HandleLightningStormCooldownChanged),
		EGameplayTagEventType::NewOrRemoved);

	RefreshAttributeValues();
}

// 设置生命条和生命文本显示，UI 不直接修改 Health 属性。
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

// 设置护盾显示值，进度条暂时只表达是否存在护盾。
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

// 设置能量条和能量文本显示，UI 只观察 GAS 属性。
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

// 快速设置基础攻击冷却激活状态，供蓝图或简单状态刷新调用。
void UArenaPlayerHUDWidget::SetBasicAttackCooldownActive(bool bInCooldownActive)
{
	SetBasicAttackCooldownValues(bInCooldownActive, 0.0f, 0.0f);
}

// 设置基础攻击冷却显示，包括文本和进度条。
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

// 设置火球冷却显示，并在未授予技能时显示锁定状态。
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

// 设置冲刺冷却显示，并在未授予技能时显示锁定状态。
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

// 设置护盾冷却显示，并在未授予技能时显示锁定状态。
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

// 设置闪电风暴冷却显示，并在 AbilitySpec 尚未授予时显示锁定状态。
void UArenaPlayerHUDWidget::SetLightningStormCooldownValues(bool bInCooldownActive, float InRemainingTime, float InDuration)
{
	const bool bHasLightningStormAbility = HasGrantedAbilityForInputTag(ArenaGameplayTags::Ability_LightningStorm);
	bLightningStormCooldownActive = bHasLightningStormAbility && bInCooldownActive;
	LightningStormCooldownRemaining = bLightningStormCooldownActive ? FMath::Max(InRemainingTime, 0.0f) : 0.0f;
	LightningStormCooldownDuration = bLightningStormCooldownActive ? FMath::Max(InDuration, 0.0f) : 0.0f;
	LightningStormCooldownPercent = LightningStormCooldownDuration > 0.0f
		? FMath::Clamp(LightningStormCooldownRemaining / LightningStormCooldownDuration, 0.0f, 1.0f)
		: 0.0f;

	if (UltimateSlotText)
	{
		UltimateSlotText->SetText(MakeLightningStormSlotText(
			bHasLightningStormAbility,
			bLightningStormCooldownActive,
			LightningStormCooldownRemaining));
	}
}

// Widget 销毁时解绑 GAS 委托，避免 ASC 回调悬挂对象。
void UArenaPlayerHUDWidget::NativeDestruct()
{
	UnbindFromAbilitySystem();

	Super::NativeDestruct();
}

// 解绑所有属性/标签委托并停止冷却刷新定时器。
void UArenaPlayerHUDWidget::UnbindFromAbilitySystem()
{
	StopBasicAttackCooldownTimer();
	StopFireballCooldownTimer();
	StopDashCooldownTimer();
	StopShieldCooldownTimer();
	StopLightningStormCooldownTimer();

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

		if (LightningStormCooldownTagDelegateHandle.IsValid())
		{
			AbilitySystemComponent->UnregisterGameplayTagEvent(
				LightningStormCooldownTagDelegateHandle,
				ArenaGameplayTags::Cooldown_LightningStorm,
				EGameplayTagEventType::NewOrRemoved);
			LightningStormCooldownTagDelegateHandle.Reset();
		}
	}

	BoundAbilitySystemComponent.Reset();
	BoundAttributeSet.Reset();
}

// 从当前绑定的 AttributeSet 拉取一次属性快照刷新 HUD。
void UArenaPlayerHUDWidget::RefreshAttributeValues()
{
	if (const UArenaAttributeSet* AttributeSet = BoundAttributeSet.Get())
	{
		SetHealthValues(AttributeSet->GetHealth(), AttributeSet->GetMaxHealth());
		SetShieldValues(AttributeSet->GetShield());
		SetEnergyValues(AttributeSet->GetEnergy(), AttributeSet->GetMaxEnergy());
	}
}

// 刷新技能槽基础文本，显示已授予技能或锁定状态。
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
		const bool bHasLightningStormAbility = HasGrantedAbilityForInputTag(ArenaGameplayTags::Ability_LightningStorm);
		UltimateSlotText->SetText(MakeLightningStormSlotText(bHasLightningStormAbility, false, 0.0f));
	}
}

// 从 ASC 查询基础攻击冷却剩余时间，并维护对应刷新定时器。
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

// 从 ASC 查询火球冷却剩余时间，并维护对应刷新定时器。
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

// 从 ASC 查询冲刺冷却剩余时间，并维护对应刷新定时器。
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

// 从 ASC 查询护盾冷却剩余时间，并维护对应刷新定时器。
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

// 从 ASC 刷新闪电风暴冷却，并按标签状态维护倒计时定时器。
void UArenaPlayerHUDWidget::RefreshLightningStormCooldownFromAbilitySystem()
{
	float RemainingTime = 0.0f;
	float Duration = 0.0f;
	const bool bHasCooldownTime = GetCooldownTimeForTag(ArenaGameplayTags::Cooldown_LightningStorm, RemainingTime, Duration);

	if (const UArenaAbilitySystemComponent* AbilitySystemComponent = BoundAbilitySystemComponent.Get())
	{
		const bool bHasCooldownTag = AbilitySystemComponent->HasMatchingGameplayTag(ArenaGameplayTags::Cooldown_LightningStorm);
		SetLightningStormCooldownValues(bHasCooldownTag, bHasCooldownTime ? RemainingTime : 0.0f, bHasCooldownTime ? Duration : 0.0f);

		if (bHasCooldownTag)
		{
			StartLightningStormCooldownTimer();
		}
		else
		{
			StopLightningStormCooldownTimer();
		}
	}
	else
	{
		SetLightningStormCooldownValues(false, 0.0f, 0.0f);
		StopLightningStormCooldownTimer();
	}
}

// 启动基础攻击冷却 UI 刷新定时器。
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

// 启动火球冷却 UI 刷新定时器。
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

// 启动冲刺冷却 UI 刷新定时器。
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

// 启动护盾冷却 UI 刷新定时器。
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

// 启动闪电风暴冷却 UI 刷新定时器。
void UArenaPlayerHUDWidget::StartLightningStormCooldownTimer()
{
	if (!GetWorld() || GetWorld()->GetTimerManager().IsTimerActive(LightningStormCooldownTimerHandle))
	{
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(
		LightningStormCooldownTimerHandle,
		this,
		&UArenaPlayerHUDWidget::RefreshLightningStormCooldownFromAbilitySystem,
		0.05f,
		true);
}

// 停止基础攻击冷却 UI 刷新定时器。
void UArenaPlayerHUDWidget::StopBasicAttackCooldownTimer()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(BasicAttackCooldownTimerHandle);
	}
}

// 停止火球冷却 UI 刷新定时器。
void UArenaPlayerHUDWidget::StopFireballCooldownTimer()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(FireballCooldownTimerHandle);
	}
}

// 停止冲刺冷却 UI 刷新定时器。
void UArenaPlayerHUDWidget::StopDashCooldownTimer()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(DashCooldownTimerHandle);
	}
}

// 停止护盾冷却 UI 刷新定时器。
void UArenaPlayerHUDWidget::StopShieldCooldownTimer()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ShieldCooldownTimerHandle);
	}
}

// 停止闪电风暴冷却 UI 刷新定时器。
void UArenaPlayerHUDWidget::StopLightningStormCooldownTimer()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(LightningStormCooldownTimerHandle);
	}
}

// 查询基础攻击冷却剩余时间和总时长。
bool UArenaPlayerHUDWidget::GetBasicAttackCooldownTime(float& OutRemainingTime, float& OutDuration) const
{
	return GetCooldownTimeForTag(ArenaGameplayTags::Cooldown_BasicAttack, OutRemainingTime, OutDuration);
}

// 根据冷却标签查询当前 ASC 上最长的冷却剩余时间。
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

// 检查玩家是否已获得带有指定输入标签的 Ability。
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

// Health 属性变化回调，刷新生命显示。
void UArenaPlayerHUDWidget::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	SetHealthValues(Data.NewValue, CurrentMaxHealth);
}

// MaxHealth 属性变化回调，刷新生命最大值显示。
void UArenaPlayerHUDWidget::HandleMaxHealthChanged(const FOnAttributeChangeData& Data)
{
	SetHealthValues(CurrentHealth, Data.NewValue);
}

// Shield 属性变化回调，刷新护盾显示。
void UArenaPlayerHUDWidget::HandleShieldChanged(const FOnAttributeChangeData& Data)
{
	SetShieldValues(Data.NewValue);
}

// Energy 属性变化回调，刷新能量显示。
void UArenaPlayerHUDWidget::HandleEnergyChanged(const FOnAttributeChangeData& Data)
{
	SetEnergyValues(Data.NewValue, CurrentMaxEnergy);
}

// MaxEnergy 属性变化回调，刷新能量最大值显示。
void UArenaPlayerHUDWidget::HandleMaxEnergyChanged(const FOnAttributeChangeData& Data)
{
	SetEnergyValues(CurrentEnergy, Data.NewValue);
}

// 基础攻击冷却标签变化回调，启动或停止对应冷却显示。
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

// 火球冷却标签变化回调，启动或停止对应冷却显示。
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

// 冲刺冷却标签变化回调，启动或停止对应冷却显示。
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

// 护盾冷却标签变化回调，启动或停止对应冷却显示。
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

// 闪电风暴冷却标签变化时同步 R 槽状态。
void UArenaPlayerHUDWidget::HandleLightningStormCooldownChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	if (CallbackTag == ArenaGameplayTags::Cooldown_LightningStorm && NewCount <= 0)
	{
		SetLightningStormCooldownValues(false, 0.0f, 0.0f);
		StopLightningStormCooldownTimer();
		return;
	}

	RefreshLightningStormCooldownFromAbilitySystem();
}
