#include "GAS/ArenaGameplayAbility_EnergyOnAbilityCast.h"

#include "AbilitySystemComponent.h"
#include "Core/ArenaPlayerState.h"
#include "Core/ArenaUpgradeDataAsset.h"
#include "GAS/ArenaGameplayEffect_EnergyRestore.h"
#include "GAS/ArenaGameplayTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogArenaEnergyOnAbilityCast, Log, All);

UArenaGameplayAbility_EnergyOnAbilityCast::UArenaGameplayAbility_EnergyOnAbilityCast()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	EnergyRestoreEffectClass = UArenaGameplayEffect_EnergyRestore::StaticClass();

	SetAssetTags(FGameplayTagContainer(ArenaGameplayTags::Ability_Passive_EnergyOnAbilityCast));
	ActivationBlockedTags.AddTag(ArenaGameplayTags::State_Dead);

	FAbilityTriggerData& AbilityCastTrigger = AbilityTriggers.AddDefaulted_GetRef();
	AbilityCastTrigger.TriggerTag = ArenaGameplayTags::Trigger_OnAbilityCast;
	AbilityCastTrigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
}

// 过滤非权威、自身以外或非 EnergySkill 的事件，避免普通主动技能和被动技能获得回能。
bool UArenaGameplayAbility_EnergyOnAbilityCast::ShouldAbilityRespondToEvent(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* Payload) const
{
	if (!Super::ShouldAbilityRespondToEvent(ActorInfo, Payload))
	{
		return false;
	}

	const UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const AActor* PlayerAvatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	return ActorInfo
		&& ActorInfo->IsNetAuthority()
		&& Payload
		&& Payload->EventTag.MatchesTagExact(ArenaGameplayTags::Trigger_OnAbilityCast)
		&& Payload->EventMagnitude > KINDA_SMALL_NUMBER
		&& Payload->Instigator.Get() == PlayerAvatar
		&& Payload->Target.Get() == PlayerAvatar
		&& Payload->InstigatorTags.HasTagExact(ArenaGameplayTags::Ability_Type_PlayerActive)
		&& Payload->InstigatorTags.HasTagExact(ArenaGameplayTags::Ability_Type_EnergySkill)
		&& SourceASC
		&& SourceASC->HasMatchingGameplayTag(ArenaGameplayTags::Upgrade_Trigger_EnergyOnAbilityCast);
}

// 从升级 SourceObject 读取数据并按永久层数恢复 Energy；本被动不 Commit，避免递归施放事件。
void UArenaGameplayAbility_EnergyOnAbilityCast::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	AArenaPlayerState* ArenaPlayerState = ActorInfo ? Cast<AArenaPlayerState>(ActorInfo->OwnerActor.Get()) : nullptr;
	const AActor* PlayerAvatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const UArenaUpgradeDataAsset* UpgradeData = Cast<UArenaUpgradeDataAsset>(GetCurrentSourceObject());
	const bool bValidRoute = UpgradeData
		&& !UpgradeData->UpgradeID.IsNone()
		&& UpgradeData->UpgradeTags.HasTagExact(ArenaGameplayTags::Upgrade_Trigger_EnergyOnAbilityCast)
		&& UpgradeData->TargetAbilityTag.MatchesTagExact(ArenaGameplayTags::Ability_Passive_EnergyOnAbilityCast)
		&& UpgradeData->TriggerEventTag.MatchesTagExact(ArenaGameplayTags::Trigger_OnAbilityCast);
	const bool bValidEvent = TriggerEventData
		&& TriggerEventData->EventTag.MatchesTagExact(ArenaGameplayTags::Trigger_OnAbilityCast)
		&& TriggerEventData->EventMagnitude > KINDA_SMALL_NUMBER
		&& TriggerEventData->Instigator.Get() == PlayerAvatar
		&& TriggerEventData->Target.Get() == PlayerAvatar
		&& TriggerEventData->InstigatorTags.HasTagExact(ArenaGameplayTags::Ability_Type_PlayerActive)
		&& TriggerEventData->InstigatorTags.HasTagExact(ArenaGameplayTags::Ability_Type_EnergySkill);

	if (!SourceASC
		|| !SourceASC->HasMatchingGameplayTag(ArenaGameplayTags::Upgrade_Trigger_EnergyOnAbilityCast)
		|| !ArenaPlayerState
		|| !EnergyRestoreEffectClass
		|| !bValidRoute
		|| !bValidEvent)
	{
		UE_LOG(LogArenaEnergyOnAbilityCast, Warning,
			TEXT("EnergyOnAbilityCast activation on %s has invalid event, upgrade data, PlayerState, or restore effect."),
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
		UE_LOG(LogArenaEnergyOnAbilityCast, Warning,
			TEXT("EnergyOnAbilityCast failed to create restore effect for %s."),
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
