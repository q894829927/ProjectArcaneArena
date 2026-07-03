#include "Core/ArenaPlayerState.h"

#include "GAS/ArenaAbilitySystemComponent.h"
#include "GAS/ArenaAttributeSet.h"

AArenaPlayerState::AArenaPlayerState()
{
	SetNetUpdateFrequency(100.0f);
	SetMinNetUpdateFrequency(33.0f);

	AbilitySystemComponent = CreateDefaultSubobject<UArenaAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UArenaAttributeSet>(TEXT("AttributeSet"));
	AbilitySystemComponent->AddAttributeSetSubobject(AttributeSet.Get());
}

UAbilitySystemComponent* AArenaPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

UArenaAbilitySystemComponent* AArenaPlayerState::GetArenaAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

UArenaAttributeSet* AArenaPlayerState::GetArenaAttributeSet() const
{
	return AttributeSet;
}
