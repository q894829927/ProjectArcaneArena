#include "GAS/ArenaGameplayAbility_EnergyOnCrit.h"

#include "AbilitySystemComponent.h"
#include "Character/ArenaEnemyCharacter.h"
#include "Core/ArenaPlayerState.h"
#include "Core/ArenaUpgradeDataAsset.h"
#include "GAS/ArenaGameplayEffect_EnergyRestore.h"
#include "GAS/ArenaGameplayTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogArenaEnergyOnCrit, Log, All);

UArenaGameplayAbility_EnergyOnCrit::UArenaGameplayAbility_EnergyOnCrit()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	EnergyRestoreEffectClass = UArenaGameplayEffect_EnergyRestore::StaticClass();

	SetAssetTags(FGameplayTagContainer(ArenaGameplayTags::Ability_Passive_EnergyOnCrit));
	ActivationBlockedTags.AddTag(ArenaGameplayTags::State_Dead);

	FAbilityTriggerData& CritTrigger = AbilityTriggers.AddDefaulted_GetRef();
	CritTrigger.TriggerTag = ArenaGameplayTags::Trigger_OnCrit;
	CritTrigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
}

// 过滤非权威、非敌人或无升级标签的事件，Secondary 暴击仍按设计允许触发。
bool UArenaGameplayAbility_EnergyOnCrit::ShouldAbilityRespondToEvent(
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
		&& Payload->EventTag.MatchesTagExact(ArenaGameplayTags::Trigger_OnCrit)
		&& Payload->EventMagnitude > KINDA_SMALL_NUMBER
		&& Payload->InstigatorTags.HasTagExact(ArenaGameplayTags::Damage_Critical)
		&& Cast<AArenaEnemyCharacter>(const_cast<AActor*>(Payload->Target.Get()))
		&& SourceASC
		&& SourceASC->HasMatchingGameplayTag(ArenaGameplayTags::Upgrade_Trigger_EnergyOnCrit);
}

// 从 SourceObject 和 PlayerState 读取配置及层数，确保恢复值由升级数据驱动。
void UArenaGameplayAbility_EnergyOnCrit::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	AArenaPlayerState* ArenaPlayerState = ActorInfo ? Cast<AArenaPlayerState>(ActorInfo->OwnerActor.Get()) : nullptr;
	const UArenaUpgradeDataAsset* UpgradeData = Cast<UArenaUpgradeDataAsset>(GetCurrentSourceObject());
	const bool bValidRoute = UpgradeData
		&& !UpgradeData->UpgradeID.IsNone()
		&& UpgradeData->UpgradeTags.HasTagExact(ArenaGameplayTags::Upgrade_Trigger_EnergyOnCrit)
		&& UpgradeData->TargetAbilityTag.MatchesTagExact(ArenaGameplayTags::Ability_Passive_EnergyOnCrit)
		&& UpgradeData->TriggerEventTag.MatchesTagExact(ArenaGameplayTags::Trigger_OnCrit);
	const bool bValidEvent = TriggerEventData
		&& TriggerEventData->EventTag.MatchesTagExact(ArenaGameplayTags::Trigger_OnCrit)
		&& TriggerEventData->EventMagnitude > KINDA_SMALL_NUMBER
		&& TriggerEventData->InstigatorTags.HasTagExact(ArenaGameplayTags::Damage_Critical)
		&& Cast<AArenaEnemyCharacter>(const_cast<AActor*>(TriggerEventData->Target.Get()));

	if (!SourceASC || !ArenaPlayerState || !EnergyRestoreEffectClass || !bValidRoute || !bValidEvent)
	{
		UE_LOG(LogArenaEnergyOnCrit, Warning,
			TEXT("EnergyOnCrit activation on %s has invalid event, upgrade data, PlayerState, or restore effect."),
			*GetNameSafe(SourceASC ? SourceASC->GetOwnerActor() : nullptr));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const int32 StackCount = FMath::Clamp(
		ArenaPlayerState->GetUpgradeStackCount(UpgradeData->UpgradeID),
		0,
		FMath::Max(UpgradeData->MaxStacks, 1));
	const float EnergyRecovery = FMath::Max(UpgradeData->NumericValue, 0.0f) * static_cast<float>(StackCount);
	if (EnergyRecovery <= KINDA_SMALL_NUMBER)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(const_cast<UArenaUpgradeDataAsset*>(UpgradeData));
	FGameplayEffectSpecHandle RestoreSpecHandle = SourceASC->MakeOutgoingSpec(
		EnergyRestoreEffectClass,
		1.0f,
		EffectContext);
	if (!RestoreSpecHandle.IsValid())
	{
		UE_LOG(LogArenaEnergyOnCrit, Warning,
			TEXT("EnergyOnCrit failed to create restore effect for %s."),
			*GetNameSafe(ArenaPlayerState));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	RestoreSpecHandle.Data->SetSetByCallerMagnitude(
		ArenaGameplayTags::SetByCaller_Recovery_Energy,
		EnergyRecovery);
	SourceASC->ApplyGameplayEffectSpecToSelf(*RestoreSpecHandle.Data.Get());
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
