#include "GAS/ArenaGameplayAbility_ShieldBreakBlast.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Character/ArenaEnemyCharacter.h"
#include "Core/ArenaUpgradeDataAsset.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffect.h"

DEFINE_LOG_CATEGORY_STATIC(LogArenaShieldBreakBlast, Log, All);

UArenaGameplayAbility_ShieldBreakBlast::UArenaGameplayAbility_ShieldBreakBlast()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	SetAssetTags(FGameplayTagContainer(ArenaGameplayTags::Ability_Passive_ShieldBreakBlast));
	ActivationBlockedTags.AddTag(ArenaGameplayTags::State_Dead);

	FAbilityTriggerData& ShieldBreakTrigger = AbilityTriggers.AddDefaulted_GetRef();
	ShieldBreakTrigger.TriggerTag = ArenaGameplayTags::Trigger_OnShieldBreak;
	ShieldBreakTrigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
}

// GAS 激活前仅做无副作用检查，确保事件属于当前护盾拥有者且来源于实际破盾结算。
bool UArenaGameplayAbility_ShieldBreakBlast::ShouldAbilityRespondToEvent(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* Payload) const
{
	if (!Super::ShouldAbilityRespondToEvent(ActorInfo, Payload))
	{
		return false;
	}

	const UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	return ActorInfo
		&& ActorInfo->IsNetAuthority()
		&& Payload
		&& Payload->EventTag.MatchesTagExact(ArenaGameplayTags::Trigger_OnShieldBreak)
		&& Payload->EventMagnitude > KINDA_SMALL_NUMBER
		&& Payload->Target.Get() == ActorInfo->AvatarActor.Get()
		&& SourceASC
		&& SourceASC->HasMatchingGameplayTag(ArenaGameplayTags::Upgrade_Trigger_ShieldBreakBlast);
}

// 破盾爆发不提交 Cost/Cooldown，所有伤害只在服务器通过现有 GE_Damage 路径结算。
void UArenaGameplayAbility_ShieldBreakBlast::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	AActor* SourceAvatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const UArenaUpgradeDataAsset* UpgradeData = Cast<UArenaUpgradeDataAsset>(GetCurrentSourceObject());
	const bool bValidUpgradeRoute = UpgradeData
		&& UpgradeData->UpgradeTags.HasTagExact(ArenaGameplayTags::Upgrade_Trigger_ShieldBreakBlast)
		&& UpgradeData->TargetAbilityTag.MatchesTagExact(ArenaGameplayTags::Ability_Passive_ShieldBreakBlast)
		&& UpgradeData->TriggerEventTag.MatchesTagExact(ArenaGameplayTags::Trigger_OnShieldBreak)
		&& UpgradeData->DamageTypeTag.MatchesTagExact(ArenaGameplayTags::Damage_Physical);
	const bool bValidEvent = TriggerEventData
		&& TriggerEventData->EventTag.MatchesTagExact(ArenaGameplayTags::Trigger_OnShieldBreak)
		&& TriggerEventData->EventMagnitude > KINDA_SMALL_NUMBER
		&& TriggerEventData->Target.Get() == SourceAvatar;
	const float BaseDamage = bValidUpgradeRoute ? FMath::Max(UpgradeData->NumericValue, 0.0f) : 0.0f;

	if (!ActorInfo || !ActorInfo->IsNetAuthority() || !SourceASC || !SourceAvatar
		|| !DamageEffectClass || !bValidEvent || BaseDamage <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogArenaShieldBreakBlast, Warning,
			TEXT("ShieldBreakBlast activation on %s has invalid event, upgrade data, DamageEffectClass, or Avatar."),
			*GetNameSafe(SourceASC ? SourceASC->GetOwnerActor() : nullptr));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FGameplayCueParameters CueParameters;
	CueParameters.Instigator = SourceAvatar;
	CueParameters.EffectCauser = SourceAvatar;
	CueParameters.SourceObject = const_cast<UArenaUpgradeDataAsset*>(UpgradeData);
	CueParameters.Location = SourceAvatar->GetActorLocation();
	CueParameters.RawMagnitude = BaseDamage;
	SourceASC->ExecuteGameplayCue(ArenaGameplayTags::GameplayCue_Ability_Shield_Break, CueParameters);

	ApplyExplosionDamage(SourceASC, SourceAvatar, UpgradeData, BaseDamage);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

// 爆发按二维半径过滤敌人，避免胶囊高度或多个组件重叠造成重复伤害。
void UArenaGameplayAbility_ShieldBreakBlast::ApplyExplosionDamage(
	UAbilitySystemComponent* SourceASC,
	AActor* SourceAvatar,
	const UArenaUpgradeDataAsset* UpgradeData,
	float BaseDamage) const
{
	UWorld* World = SourceAvatar ? SourceAvatar->GetWorld() : nullptr;
	if (!World || !SourceASC || !DamageEffectClass || !UpgradeData
		|| BaseDamage <= KINDA_SMALL_NUMBER || ExplosionRadius <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector ExplosionLocation = SourceAvatar->GetActorLocation();
	TArray<FOverlapResult> OverlapResults;
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ArenaShieldBreakBlast), false, SourceAvatar);
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
		DamageSpec->AddDynamicAssetTag(ArenaGameplayTags::Damage_Physical);
		DamageSpec->AddDynamicAssetTag(ArenaGameplayTags::Damage_Secondary);
		SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpec, TargetASC);
	}
}
