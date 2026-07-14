#include "GAS/ArenaGameplayAbility_Overload.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Character/ArenaEnemyCharacter.h"
#include "Core/ArenaUpgradeDataAsset.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GAS/ArenaGameplayEffect_OverloadLockout.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffect.h"

DEFINE_LOG_CATEGORY_STATIC(LogArenaOverload, Log, All);

UArenaGameplayAbility_Overload::UArenaGameplayAbility_Overload()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	OverloadLockoutEffectClass = UArenaGameplayEffect_OverloadLockout::StaticClass();

	SetAssetTags(FGameplayTagContainer(ArenaGameplayTags::Ability_Passive_Overload));
	ActivationBlockedTags.AddTag(ArenaGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(ArenaGameplayTags::State_Stunned);

	FAbilityTriggerData& DamageTrigger = AbilityTriggers.AddDefaulted_GetRef();
	DamageTrigger.TriggerTag = ArenaGameplayTags::Trigger_OnDamageDealt_Lightning;
	DamageTrigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
}

// GAS 在正式激活前调用本函数；这里只做无副作用检查，限频写入保留到 ActivateAbility。
bool UArenaGameplayAbility_Overload::ShouldAbilityRespondToEvent(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* Payload) const
{
	if (!Super::ShouldAbilityRespondToEvent(ActorInfo, Payload))
	{
		return false;
	}

	UAbilitySystemComponent* SourceASC = nullptr;
	UAbilitySystemComponent* TargetASC = nullptr;
	AArenaEnemyCharacter* TriggerTarget = nullptr;
	return ResolveTriggerActors(ActorInfo, Payload, SourceASC, TriggerTarget, TargetASC)
		&& !HasSourceLockout(TargetASC, SourceASC);
}

// 从 AbilitySpec SourceObject 读取传奇升级数值，避免运行时依赖具体 UpgradeID。
void UArenaGameplayAbility_Overload::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UAbilitySystemComponent* SourceASC = nullptr;
	UAbilitySystemComponent* TargetASC = nullptr;
	AArenaEnemyCharacter* TriggerTarget = nullptr;
	if (!ResolveTriggerActors(ActorInfo, TriggerEventData, SourceASC, TriggerTarget, TargetASC)
		|| HasSourceLockout(TargetASC, SourceASC))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const UArenaUpgradeDataAsset* UpgradeData = Cast<UArenaUpgradeDataAsset>(GetCurrentSourceObject());
	// 通过 DataAsset 的 Ability/Damage/Trigger/Upgrade Tags 校验路由，不依赖显示名或 UpgradeID。
	const bool bValidUpgradeRoute = UpgradeData
		&& UpgradeData->UpgradeTags.HasTagExact(ArenaGameplayTags::Upgrade_Combo_Overload)
		&& UpgradeData->TargetAbilityTag.MatchesTagExact(ArenaGameplayTags::Ability_Passive_Overload)
		&& UpgradeData->TriggerEventTag.MatchesTagExact(ArenaGameplayTags::Trigger_OnDamageDealt_Lightning)
		&& UpgradeData->DamageTypeTag.MatchesTagExact(ArenaGameplayTags::Damage_Lightning);
	const float OverloadBaseDamage = bValidUpgradeRoute ? FMath::Max(UpgradeData->NumericValue, 0.0f) : 0.0f;
	AActor* SourceAvatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!bValidUpgradeRoute || OverloadBaseDamage <= 0.0f || !DamageEffectClass || !SourceAvatar || !TriggerTarget)
	{
		UE_LOG(LogArenaOverload, Warning,
			TEXT("Overload activation on %s has invalid routing tags, NumericValue, DamageEffectClass, or Avatar."),
			*GetNameSafe(SourceASC ? SourceASC->GetOwnerActor() : nullptr));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const bool bTriggerTargetDead = TargetASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead);
	if (!bTriggerTargetDead
		&& !ApplySourceLockout(SourceASC, TargetASC, SourceAvatar, UpgradeData))
	{
		// 存活目标无法写入限频时关闭本次触发，避免错误配置产生无限被动链。
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FVector ExplosionLocation = TriggerTarget->GetActorLocation();
	FGameplayCueParameters CueParameters;
	CueParameters.Instigator = SourceAvatar;
	CueParameters.EffectCauser = SourceAvatar;
	CueParameters.Location = ExplosionLocation;
	CueParameters.RawMagnitude = OverloadBaseDamage;
	SourceASC->ExecuteGameplayCue(ArenaGameplayTags::GameplayCue_Combo_Overload, CueParameters);

	ApplyExplosionDamage(
		SourceASC,
		SourceAvatar,
		ExplosionLocation,
		UpgradeData,
		OverloadBaseDamage);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

// TargetTags 来自伤害结算前快照，因此目标在本次 Lightning 中死亡后仍可满足 Burning 条件。
bool UArenaGameplayAbility_Overload::ResolveTriggerActors(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* Payload,
	UAbilitySystemComponent*& OutSourceASC,
	AArenaEnemyCharacter*& OutTriggerTarget,
	UAbilitySystemComponent*& OutTargetASC) const
{
	OutSourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	OutTriggerTarget = Payload ? Cast<AArenaEnemyCharacter>(const_cast<AActor*>(Payload->Target.Get())) : nullptr;
	OutTargetASC = OutTriggerTarget
		? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OutTriggerTarget)
		: nullptr;

	return ActorInfo
		&& ActorInfo->IsNetAuthority()
		&& Payload
		&& Payload->EventTag.MatchesTagExact(ArenaGameplayTags::Trigger_OnDamageDealt_Lightning)
		&& Payload->EventMagnitude > KINDA_SMALL_NUMBER
		&& OutSourceASC
		&& OutSourceASC->HasMatchingGameplayTag(ArenaGameplayTags::Upgrade_Combo_Overload)
		&& !Payload->InstigatorTags.HasTagExact(ArenaGameplayTags::Damage_Secondary)
		&& Payload->TargetTags.HasTag(ArenaGameplayTags::Status_Burning)
		&& OutTriggerTarget
		&& OutTargetASC;
}

// Granted Tag 对所有来源可见，因此进一步比较 ActiveGE Context 中的来源 ASC。
bool UArenaGameplayAbility_Overload::HasSourceLockout(
	const UAbilitySystemComponent* TargetASC,
	const UAbilitySystemComponent* SourceASC) const
{
	if (!TargetASC || !SourceASC
		|| !TargetASC->HasMatchingGameplayTag(ArenaGameplayTags::Status_Overload_Lockout))
	{
		return false;
	}

	FGameplayTagContainer LockoutTags;
	LockoutTags.AddTag(ArenaGameplayTags::Status_Overload_Lockout);
	const FGameplayEffectQuery LockoutQuery = FGameplayEffectQuery::MakeQuery_MatchAllOwningTags(LockoutTags);
	for (const FActiveGameplayEffectHandle& ActiveEffectHandle : TargetASC->GetActiveEffects(LockoutQuery))
	{
		const FActiveGameplayEffect* ActiveEffect = TargetASC->GetActiveGameplayEffect(ActiveEffectHandle);
		if (ActiveEffect
			&& ActiveEffect->Spec.GetEffectContext().GetInstigatorAbilitySystemComponent() == SourceASC)
		{
			return true;
		}
	}

	return false;
}

bool UArenaGameplayAbility_Overload::ApplySourceLockout(
	UAbilitySystemComponent* SourceASC,
	UAbilitySystemComponent* TargetASC,
	AActor* SourceAvatar,
	const UArenaUpgradeDataAsset* UpgradeData) const
{
	if (!SourceASC || !TargetASC || !SourceAvatar || !UpgradeData || !OverloadLockoutEffectClass)
	{
		UE_LOG(LogArenaOverload, Warning, TEXT("Overload lockout configuration is invalid."));
		return false;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddInstigator(SourceAvatar, SourceAvatar);
	EffectContext.AddSourceObject(const_cast<UArenaUpgradeDataAsset*>(UpgradeData));
	const FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(
		OverloadLockoutEffectClass,
		1.0f,
		EffectContext);
	if (!SpecHandle.IsValid())
	{
		return false;
	}

	return SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC).IsValid();
}

// 爆炸只伤害 ArenaEnemyCharacter；Secondary 仍路由通用事件，但不会再次触发 Overload。
void UArenaGameplayAbility_Overload::ApplyExplosionDamage(
	UAbilitySystemComponent* SourceASC,
	AActor* SourceAvatar,
	const FVector& ExplosionLocation,
	const UArenaUpgradeDataAsset* UpgradeData,
	float BaseDamage) const
{
	UWorld* World = SourceAvatar ? SourceAvatar->GetWorld() : nullptr;
	if (!World || !SourceASC || !DamageEffectClass || BaseDamage <= 0.0f)
	{
		return;
	}

	TArray<FOverlapResult> OverlapResults;
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ArenaOverloadExplosion), false, SourceAvatar);
	QueryParams.AddIgnoredActor(SourceAvatar);
	World->OverlapMultiByObjectType(
		OverlapResults,
		ExplosionLocation,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(ExplosionRadius),
		QueryParams);

	TSet<AActor*> DamagedActors;
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AArenaEnemyCharacter* Enemy = Cast<AArenaEnemyCharacter>(OverlapResult.GetActor());
		if (!Enemy || DamagedActors.Contains(Enemy)
			|| (Enemy->GetActorLocation() - ExplosionLocation).SizeSquared2D() > FMath::Square(ExplosionRadius))
		{
			continue;
		}

		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Enemy);
		if (!TargetASC
			|| TargetASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead)
			|| TargetASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Invincible))
		{
			continue;
		}
		DamagedActors.Add(Enemy);

		FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
		EffectContext.AddInstigator(SourceAvatar, SourceAvatar);
		EffectContext.AddSourceObject(const_cast<UArenaUpgradeDataAsset*>(UpgradeData));
		FGameplayEffectSpecHandle DamageSpecHandle = SourceASC->MakeOutgoingSpec(
			DamageEffectClass,
			1.0f,
			EffectContext);
		if (!DamageSpecHandle.IsValid())
		{
			continue;
		}

		FGameplayEffectSpec* DamageSpec = DamageSpecHandle.Data.Get();
		DamageSpec->SetSetByCallerMagnitude(ArenaGameplayTags::SetByCaller_Damage_Base, BaseDamage);
		DamageSpec->SetSetByCallerMagnitude(ArenaGameplayTags::SetByCaller_Damage_SkillMultiplier, 1.0f);
		DamageSpec->AddDynamicAssetTag(ArenaGameplayTags::Damage_Lightning);
		DamageSpec->AddDynamicAssetTag(ArenaGameplayTags::Damage_Secondary);
		SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpec, TargetASC);
	}
}
