#include "GAS/ArenaAttributeSet.h"

#include "AbilitySystemComponent.h"
#include "Components/SceneComponent.h"
#include "Core/ArenaGameState.h"
#include "Core/ArenaUpgradeDataAsset.h"
#include "GAS/ArenaAbilitySystemComponent.h"
#include "GAS/ArenaAbilityNetworkDebug.h"
#include "GAS/ArenaDamageFeedbackTypes.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffectExtension.h"
#include "GameplayEffectTypes.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogArenaDamage, Log, All);

namespace
{
	// 优先使用升级 DataAsset 的目标技能标签，再从 Spec 标签、效果类和来源对象解析伤害归属。
	FString ResolveDamageSkillLabel(const FGameplayEffectSpec& EffectSpec)
	{
		FGameplayTagContainer AssetTags;
		EffectSpec.GetAllAssetTags(AssetTags);

		const UObject* SourceObject = EffectSpec.GetEffectContext().GetSourceObject();
		const UArenaUpgradeDataAsset* UpgradeData = Cast<UArenaUpgradeDataAsset>(SourceObject);
		if (UpgradeData && UpgradeData->TargetAbilityTag.IsValid())
		{
			return UpgradeData->TargetAbilityTag.ToString();
		}

		const FString EffectClassName = GetNameSafe(EffectSpec.Def);
		if (EffectClassName.Contains(TEXT("Burning")))
		{
			return ArenaGameplayTags::Status_Burning.GetTag().ToString();
		}

		for (const FGameplayTag& AssetTag : AssetTags)
		{
			const FString TagString = AssetTag.ToString();
			if (TagString.StartsWith(TEXT("Ability.")) || TagString.StartsWith(TEXT("Status.")))
			{
				return TagString;
			}
		}

		const FString SourceClassName = GetNameSafe(SourceObject ? SourceObject->GetClass() : nullptr);
		if (SourceClassName.Contains(TEXT("BasicAttack")))
		{
			return ArenaGameplayTags::Ability_BasicAttack.GetTag().ToString();
		}
		if (SourceClassName.Contains(TEXT("FireballProjectile")))
		{
			return ArenaGameplayTags::Ability_Fireball.GetTag().ToString();
		}
		if (SourceClassName.Contains(TEXT("LightningStormArea")))
		{
			return ArenaGameplayTags::Ability_LightningStorm.GetTag().ToString();
		}
		if (SourceClassName.Contains(TEXT("EnemyMeleeAttack")))
		{
			return ArenaGameplayTags::Ability_Enemy_MeleeAttack.GetTag().ToString();
		}
		if (SourceClassName.Contains(TEXT("EnemyProjectile")))
		{
			return ArenaGameplayTags::Ability_Enemy_RangedAttack.GetTag().ToString();
		}
		if (SourceClassName.Contains(TEXT("Overload")))
		{
			return ArenaGameplayTags::Ability_Passive_Overload.GetTag().ToString();
		}

		return !SourceClassName.IsEmpty() ? SourceClassName : EffectClassName;
	}

	// 仅在权威端记录实际消耗的 Shield 与 Health，避免客户端预测或过量伤害产生误导日志。
	void LogAuthoritativeDamage(
		const FGameplayEffectSpec& EffectSpec,
		const UAbilitySystemComponent* TargetASC,
		float AppliedDamage)
	{
		if (!TargetASC || !TargetASC->IsOwnerActorAuthoritative() || AppliedDamage <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		const FGameplayEffectContextHandle EffectContext = EffectSpec.GetEffectContext();
		const UAbilitySystemComponent* SourceASC = EffectContext.GetInstigatorAbilitySystemComponent();
		const AActor* SourceActor = SourceASC ? SourceASC->GetAvatarActor() : EffectContext.GetOriginalInstigator();
		const AActor* TargetActor = TargetASC->GetAvatarActor();
		if (!SourceActor && SourceASC)
		{
			SourceActor = SourceASC->GetOwnerActor();
		}
		if (!TargetActor)
		{
			TargetActor = TargetASC->GetOwnerActor();
		}

		UE_LOG(LogArenaDamage, Log, TEXT("%s 使用 %s 对 %s 造成 %.2f 点伤害"),
			*GetNameSafe(SourceActor),
			*ResolveDamageSkillLabel(EffectSpec),
			*GetNameSafe(TargetActor),
			AppliedDamage);
	}

	// 优先保存 EffectCauser 的命中时位置，失效时回退到 Instigator 或来源 ASC Avatar。
	bool ResolveDamageSourceLocation(const FGameplayEffectSpec& EffectSpec, FVector& OutLocation)
	{
		const FGameplayEffectContextHandle EffectContext = EffectSpec.GetEffectContext();
		const AActor* SourceActor = EffectContext.GetEffectCauser();
		if (!SourceActor)
		{
			SourceActor = Cast<AActor>(EffectContext.GetSourceObject());
		}
		if (!SourceActor)
		{
			SourceActor = EffectContext.GetOriginalInstigator();
		}
		if (!SourceActor)
		{
			if (const UAbilitySystemComponent* SourceASC = EffectContext.GetInstigatorAbilitySystemComponent())
			{
				SourceActor = SourceASC->GetAvatarActor();
			}
		}

		if (!SourceActor)
		{
			return false;
		}

		OutLocation = SourceActor->GetActorLocation();
		return true;
	}
}

// 构造属性集，设置玩家和敌人可共用的基础默认值。
UArenaAttributeSet::UArenaAttributeSet()
{
	InitMaxHealth(100.0f);
	InitHealth(100.0f);

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

// 注册需要复制的 GameplayAttribute，配合 RepNotify 驱动客户端 UI。
void UArenaAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UArenaAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArenaAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArenaAttributeSet, Shield, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArenaAttributeSet, Energy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArenaAttributeSet, MaxEnergy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArenaAttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArenaAttributeSet, Defense, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArenaAttributeSet, MoveSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArenaAttributeSet, CritChance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UArenaAttributeSet, CritDamage, COND_None, REPNOTIFY_Always);
}

// CurrentValue 改变前做边界限制，处理 Max 属性影响下的即时 clamp。
void UArenaAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	ClampAttribute(Attribute, NewValue);
}

// BaseValue 改变前做边界限制，覆盖 Instant GE 等修改基础值的路径。
void UArenaAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	ClampAttribute(Attribute, NewValue);
}

// GE 执行后消费 Damage/Healing 元属性，演出与 Victory 拒绝伤害，其余路径发送权威结果事件。
void UArenaAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		// Damage 是瞬时 meta attribute：ExecCalc 写入后立刻消费并清零。
		const float LocalDamage = FMath::Max(GetDamage(), 0.0f);
		SetDamage(0.0f);
		UAbilitySystemComponent* TargetASC = GetOwningAbilitySystemComponent();
		const AActor* TargetAvatar = TargetASC ? TargetASC->GetAvatarActor() : nullptr;
		const AArenaGameState* ArenaGameState = TargetAvatar && TargetAvatar->GetWorld()
			? TargetAvatar->GetWorld()->GetGameState<AArenaGameState>()
			: nullptr;
		const EArenaGamePhase GamePhase = ArenaGameState
			? ArenaGameState->GetGamePhase()
			: EArenaGamePhase::Waiting;
		if (ArenaGameState
			&& (GamePhase == EArenaGamePhase::BossIntro
				|| GamePhase == EArenaGamePhase::BossOutro
				|| GamePhase == EArenaGamePhase::Victory))
		{
			// 最终消费入口再次阻断演出与 Victory 伤害，覆盖 Burning 等不经过主 ExecCalc_Damage 的旧效果。
			return;
		}
		FGameplayTagContainer TargetTagsBeforeDamage;
		float AppliedDamage = 0.0f;
		float AppliedShieldDamage = 0.0f;
		float AppliedHealthDamage = 0.0f;
		bool bShieldBrokenByDamage = false;
		float ShieldBeforeDamage = GetShield();

		if (LocalDamage > 0.0f)
		{
			if (TargetASC)
			{
				TargetASC->GetOwnedGameplayTags(TargetTagsBeforeDamage);
			}

			const float HealthBeforeDamage = GetHealth();
			// 伤害先消耗护盾，剩余部分才扣 Health。
			const float ShieldDamage = FMath::Min(ShieldBeforeDamage, LocalDamage);
			const float RemainingDamage = LocalDamage - ShieldDamage;

			SetShield(ShieldBeforeDamage - ShieldDamage);
			SetHealth(HealthBeforeDamage - RemainingDamage);

			// 过量伤害不计入事件数值，只有真正消耗的 Shield + Health 才能触发被动。
			AppliedShieldDamage = FMath::Max(ShieldBeforeDamage - GetShield(), 0.0f);
			AppliedHealthDamage = FMath::Max(HealthBeforeDamage - GetHealth(), 0.0f);
			AppliedDamage = AppliedShieldDamage + AppliedHealthDamage;
			bShieldBrokenByDamage = ShieldBeforeDamage > KINDA_SMALL_NUMBER
				&& AppliedShieldDamage > KINDA_SMALL_NUMBER
				&& GetShield() <= KINDA_SMALL_NUMBER;
		}

		RefreshShieldGameplayCue();
		UpdateDeadTag();
		if (AppliedDamage > KINDA_SMALL_NUMBER)
		{
			LogAuthoritativeDamage(Data.EffectSpec, TargetASC, AppliedDamage);
			QueueDamageFeedback(Data, ShieldBeforeDamage, AppliedShieldDamage, AppliedHealthDamage);
			UAbilitySystemComponent* SourceASC = Data.EffectSpec.GetEffectContext().GetInstigatorAbilitySystemComponent();
			if (UArenaAbilitySystemComponent* ArenaSourceASC = Cast<UArenaAbilitySystemComponent>(SourceASC))
			{
				// 先更新死亡状态，再把实际 Shield/Health 损失交给统计与被动事件统一权威路由。
				ArenaSourceASC->RouteAuthoritativeDamageEvent(
					Data.EffectSpec,
					TargetASC,
					TargetTagsBeforeDamage,
					AppliedShieldDamage,
					AppliedHealthDamage);
			}

			if (bShieldBrokenByDamage)
			{
				if (UArenaAbilitySystemComponent* ArenaTargetASC = Cast<UArenaAbilitySystemComponent>(TargetASC))
				{
					// 来源事件完成后再通知护盾拥有者；致死伤害由目标 ASC 的死亡检查直接拒绝。
					ArenaTargetASC->RouteAuthoritativeShieldBreakEvent(
						Data.EffectSpec,
						SourceASC,
						TargetTagsBeforeDamage,
						AppliedShieldDamage);
				}
			}
		}
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
		RefreshShieldGameplayCue();
		UpdateDeadTag();
	}
	else if (Data.EvaluatedData.Attribute == GetShieldAttribute())
	{
		// Shield 是可叠加的临时吸收量，没有最大护盾属性，只限制不能低于 0。
		SetShield(GetShield());
		RefreshShieldGameplayCue();
	}
	else if (Data.EvaluatedData.Attribute == GetEnergyAttribute()
		|| Data.EvaluatedData.Attribute == GetMaxEnergyAttribute())
	{
		SetEnergy(GetEnergy());
	}
}

// 根据属性类型统一限制数值范围，避免各处重复 clamp 规则。
void UArenaAttributeSet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		// 项目规则允许所有属性归零，0 可表示该资源或能力被禁用。
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetShieldAttribute())
	{
		// 护盾池可以被技能和升级持续叠加，只限制不能低于 0。
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetEnergyAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxEnergy());
	}
	else if (Attribute == GetMaxEnergyAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
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
		NewValue = FMath::Max(NewValue, 0.0f);
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

// 服务端根据 Health 更新 State.Dead loose tag，供死亡流程和客户端观察。
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

void UArenaAttributeSet::RefreshShieldGameplayCue()
{
	UAbilitySystemComponent* OwningASC = GetOwningAbilitySystemComponent();
	if (!OwningASC || !OwningASC->IsOwnerActorAuthoritative())
	{
		return;
	}

	const bool bShouldBeActive = GetShield() > KINDA_SMALL_NUMBER && GetHealth() > 0.0f;
	if (bShouldBeActive == bShieldGameplayCueActive)
	{
		return;
	}

	if (bShouldBeActive)
	{
		FGameplayCueParameters CueParameters;
		AActor* CueAvatar = OwningASC->GetAvatarActor();
		CueParameters.Instigator = CueAvatar;
		CueParameters.EffectCauser = CueAvatar;
		// 护盾以 Capsule/Avatar 根组件为中心，避免 Manny Mesh 原点让光环落在头顶或脚下。
		CueParameters.TargetAttachComponent = CueAvatar ? CueAvatar->GetRootComponent() : nullptr;
		OwningASC->AddGameplayCue(ArenaGameplayTags::GameplayCue_Ability_Shield_Active, CueParameters);
	}
	else
	{
		OwningASC->RemoveGameplayCue(ArenaGameplayTags::GameplayCue_Ability_Shield_Active);
	}

	bShieldGameplayCueActive = bShouldBeActive;
	if (ArenaAbilityNetworkDebug::IsAuditEnabled())
	{
		UE_LOG(LogArenaAbilityNet, Log, TEXT("[%llu] ShieldCue %s Avatar=%s Shield=%.2f"),
			ArenaAbilityNetworkDebug::NextServerExecutionSequence(),
			bShouldBeActive ? TEXT("Added") : TEXT("Removed"),
			*GetNameSafe(OwningASC->GetAvatarActor()),
			GetShield());
	}
}

// 根据权威实际资源损失分类，并优先使用真实命中点、否则计算目标表面位置供客户端表现。
void UArenaAttributeSet::QueueDamageFeedback(
	const FGameplayEffectModCallbackData& Data,
	float ShieldBeforeDamage,
	float ActualShieldDamage,
	float ActualHealthDamage) const
{
	UArenaAbilitySystemComponent* TargetASC = Cast<UArenaAbilitySystemComponent>(GetOwningAbilitySystemComponent());
	const float AppliedDamage = ActualShieldDamage + ActualHealthDamage;
	if (!TargetASC || !TargetASC->IsOwnerActorAuthoritative() || AppliedDamage <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const bool bLostShield = ActualShieldDamage > KINDA_SMALL_NUMBER;
	const bool bLostHealth = ActualHealthDamage > KINDA_SMALL_NUMBER;
	const bool bShieldBroken = ShieldBeforeDamage > KINDA_SMALL_NUMBER
		&& bLostShield
		&& GetShield() <= KINDA_SMALL_NUMBER;

	FArenaDamageFeedbackData DamageFeedback;
	if (bShieldBroken && bLostHealth)
	{
		DamageFeedback.FeedbackType = EArenaDamageFeedbackType::ShieldBreakWithHealthDamage;
	}
	else if (bShieldBroken)
	{
		DamageFeedback.FeedbackType = EArenaDamageFeedbackType::ShieldBreak;
	}
	else if (bLostShield)
	{
		DamageFeedback.FeedbackType = EArenaDamageFeedbackType::ShieldOnly;
	}
	else if (bLostHealth)
	{
		DamageFeedback.FeedbackType = EArenaDamageFeedbackType::HealthOnly;
	}
	else
	{
		return;
	}

	FGameplayTagContainer AssetTags;
	Data.EffectSpec.GetAllAssetTags(AssetTags);
	DamageFeedback.ActualShieldDamage = ActualShieldDamage;
	DamageFeedback.ActualHealthDamage = ActualHealthDamage;
	DamageFeedback.HealthDamageRatio = GetMaxHealth() > KINDA_SMALL_NUMBER
		? FMath::Clamp(ActualHealthDamage / GetMaxHealth(), 0.0f, 1.0f)
		: 0.0f;
	FVector DamageSourceLocation = FVector::ZeroVector;
	DamageFeedback.bHasDamageSourceLocation = ResolveDamageSourceLocation(Data.EffectSpec, DamageSourceLocation);
	DamageFeedback.DamageSourceLocation = DamageSourceLocation;

	DamageFeedback.CueParameters = FGameplayCueParameters(Data.EffectSpec.GetEffectContext());
	DamageFeedback.CueParameters.RawMagnitude = AppliedDamage;
	DamageFeedback.CueParameters.NormalizedMagnitude = DamageFeedback.HealthDamageRatio;
	DamageFeedback.CueParameters.EffectContext = Data.EffectSpec.GetEffectContext();
	DamageFeedback.CueParameters.AggregatedSourceTags.AppendTags(AssetTags);
	if (const FHitResult* HitResult = DamageFeedback.CueParameters.EffectContext.GetHitResult())
	{
		DamageFeedback.CueParameters.Location = HitResult->ImpactPoint;
		DamageFeedback.CueParameters.Normal = HitResult->ImpactNormal;
	}
	else if (const AActor* TargetAvatar = TargetASC->GetAvatarActor())
	{
		FVector BoundsOrigin = FVector::ZeroVector;
		FVector BoundsExtent = FVector::ZeroVector;
		TargetAvatar->GetActorBounds(true, BoundsOrigin, BoundsExtent);
		FVector SurfaceNormal = DamageFeedback.bHasDamageSourceLocation
			? (FVector(DamageFeedback.DamageSourceLocation) - BoundsOrigin).GetSafeNormal()
			: FVector::UpVector;
		if (SurfaceNormal.IsNearlyZero())
		{
			SurfaceNormal = FVector::UpVector;
		}
		// 按伤害方向投影包围盒尺寸，避免用角色高度作为水平半径而把 Cue 推离目标。
		const float SurfaceDistance = FMath::Max(
			FVector::DotProduct(BoundsExtent, SurfaceNormal.GetAbs()),
			1.0f);
		DamageFeedback.CueParameters.Location = BoundsOrigin + SurfaceNormal * SurfaceDistance;
		DamageFeedback.CueParameters.Normal = SurfaceNormal;
	}

	TargetASC->QueueAuthoritativeDamageFeedback(DamageFeedback);
	if (ArenaAbilityNetworkDebug::IsAuditEnabled())
	{
		UE_LOG(LogArenaAbilityNet, Log,
			TEXT("[%llu] DamageFeedback Target=%s Type=%d Shield=%.2f Health=%.2f Total=%.2f"),
			ArenaAbilityNetworkDebug::NextServerExecutionSequence(),
			*GetNameSafe(TargetASC->GetAvatarActor()),
			static_cast<int32>(DamageFeedback.FeedbackType),
			ActualShieldDamage,
			ActualHealthDamage,
			AppliedDamage);
	}
}

// Health 复制回调，通知 GAS 属性变化委托和 UI。
void UArenaAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArenaAttributeSet, Health, OldValue);
}

// MaxHealth 复制回调，通知 GAS 属性变化委托和 UI。
void UArenaAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArenaAttributeSet, MaxHealth, OldValue);
}

// Shield 复制回调，通知 GAS 属性变化委托和 UI。
void UArenaAttributeSet::OnRep_Shield(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArenaAttributeSet, Shield, OldValue);
}

// Energy 复制回调，通知 GAS 属性变化委托和 UI。
void UArenaAttributeSet::OnRep_Energy(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArenaAttributeSet, Energy, OldValue);
}

// MaxEnergy 复制回调，通知 GAS 属性变化委托和 UI。
void UArenaAttributeSet::OnRep_MaxEnergy(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArenaAttributeSet, MaxEnergy, OldValue);
}

// AttackPower 复制回调，通知 GAS 属性变化委托。
void UArenaAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArenaAttributeSet, AttackPower, OldValue);
}

// Defense 复制回调，通知 GAS 属性变化委托。
void UArenaAttributeSet::OnRep_Defense(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArenaAttributeSet, Defense, OldValue);
}

// MoveSpeed 复制回调，通知 GAS 属性变化委托。
void UArenaAttributeSet::OnRep_MoveSpeed(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArenaAttributeSet, MoveSpeed, OldValue);
}

// CritChance 复制回调，通知 GAS 属性变化委托。
void UArenaAttributeSet::OnRep_CritChance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArenaAttributeSet, CritChance, OldValue);
}

// CritDamage 复制回调，通知 GAS 属性变化委托。
void UArenaAttributeSet::OnRep_CritDamage(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UArenaAttributeSet, CritDamage, OldValue);
}
