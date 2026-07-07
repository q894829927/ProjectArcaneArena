#include "GAS/ArenaAttributeSet.h"

#include "AbilitySystemComponent.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UArenaAttributeSet::UArenaAttributeSet()
{
	InitMaxHealth(100.0f);
	InitHealth(100.0f);

	InitMaxShield(50.0f);
	InitShield(0.0f);

	InitMaxEnergy(100.0f);
	InitEnergy(100.0f);

	InitAttackPower(10.0f);
	InitDefense(0.0f);
	InitMoveSpeed(600.0f);
	InitCritChance(0.05f);
	InitCritDamage(2.0f);
	InitDamage(0.0f);
	InitHealing(0.0f);
}

void UArenaAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UArenaAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArenaAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArenaAttributeSet, Shield, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArenaAttributeSet, MaxShield, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArenaAttributeSet, Energy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArenaAttributeSet, MaxEnergy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArenaAttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArenaAttributeSet, Defense, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArenaAttributeSet, MoveSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArenaAttributeSet, CritChance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArenaAttributeSet, CritDamage, COND_None, REPNOTIFY_Always);
}

void UArenaAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	ClampAttribute(Attribute, NewValue);
}

void UArenaAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	ClampAttribute(Attribute, NewValue);
}

void UArenaAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		// Damage 是瞬时 meta attribute：ExecCalc 写入后立刻消费并清零。
		const float LocalDamage = FMath::Max(GetDamage(), 0.0f);
		SetDamage(0.0f);

		if (LocalDamage > 0.0f)
		{
			// 伤害先消耗护盾，剩余部分才扣 Health。
			const float ShieldDamage = FMath::Min(GetShield(), LocalDamage);
			const float RemainingDamage = LocalDamage - ShieldDamage;

			SetShield(GetShield() - ShieldDamage);
			SetHealth(GetHealth() - RemainingDamage);
		}

		UpdateDeadTag();
	}
	else if (Data.EvaluatedData.Attribute == GetHealingAttribute())
	{
		// Healing 也是瞬时 meta attribute，避免复制和长期保存临时治疗量。
		const float LocalHealing = FMath::Max(GetHealing(), 0.0f);
		SetHealing(0.0f);

		if (LocalHealing > 0.0f)
		{
			SetHealth(GetHealth() + LocalHealing);
		}

		UpdateDeadTag();
	}
	else if (Data.EvaluatedData.Attribute == GetHealthAttribute()
		|| Data.EvaluatedData.Attribute == GetMaxHealthAttribute())
	{
		// 通过 setter 重新走 clamp，确保 MaxHealth 改变后 Health 仍合法。
		SetHealth(GetHealth());
		UpdateDeadTag();
	}
	else if (Data.EvaluatedData.Attribute == GetShieldAttribute()
		|| Data.EvaluatedData.Attribute == GetMaxShieldAttribute())
	{
		SetShield(GetShield());
	}
	else if (Data.EvaluatedData.Attribute == GetEnergyAttribute()
		|| Data.EvaluatedData.Attribute == GetMaxEnergyAttribute())
	{
		SetEnergy(GetEnergy());
	}
}

void UArenaAttributeSet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
	else if (Attribute == GetShieldAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxShield());
	}
	else if (Attribute == GetMaxShieldAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetEnergyAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxEnergy());
	}
	else if (Attribute == GetMaxEnergyAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
	else if (Attribute == GetAttackPowerAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetDefenseAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetMoveSpeedAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetCritChanceAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, 1.0f);
	}
	else if (Attribute == GetCritDamageAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
	else if (Attribute == GetDamageAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetHealingAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
}

void UArenaAttributeSet::UpdateDeadTag() const
{
	UAbilitySystemComponent* OwningASC = GetOwningAbilitySystemComponent();
	if (!OwningASC)
	{
		return;
	}

	const AActor* OwningActor = OwningASC->GetOwnerActor();
	if (!OwningActor || !OwningActor->HasAuthority())
	{
		return;
	}

	const int32 DeadTagCount = GetHealth() <= 0.0f ? 1 : 0;
	// 本地 loose tag 供服务端立即判断，replicated loose tag 供客户端稳定观察死亡状态。
	OwningASC->SetLooseGameplayTagCount(ArenaGameplayTags::State_Dead, DeadTagCount);
	OwningASC->SetReplicatedLooseGameplayTagCount(ArenaGameplayTags::State_Dead, DeadTagCount);
}

void UArenaAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArenaAttributeSet, Health, OldValue);
}

void UArenaAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArenaAttributeSet, MaxHealth, OldValue);
}

void UArenaAttributeSet::OnRep_Shield(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArenaAttributeSet, Shield, OldValue);
}

void UArenaAttributeSet::OnRep_MaxShield(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArenaAttributeSet, MaxShield, OldValue);
}

void UArenaAttributeSet::OnRep_Energy(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArenaAttributeSet, Energy, OldValue);
}

void UArenaAttributeSet::OnRep_MaxEnergy(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArenaAttributeSet, MaxEnergy, OldValue);
}

void UArenaAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArenaAttributeSet, AttackPower, OldValue);
}

void UArenaAttributeSet::OnRep_Defense(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArenaAttributeSet, Defense, OldValue);
}

void UArenaAttributeSet::OnRep_MoveSpeed(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArenaAttributeSet, MoveSpeed, OldValue);
}

void UArenaAttributeSet::OnRep_CritChance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArenaAttributeSet, CritChance, OldValue);
}

void UArenaAttributeSet::OnRep_CritDamage(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArenaAttributeSet, CritDamage, OldValue);
}
