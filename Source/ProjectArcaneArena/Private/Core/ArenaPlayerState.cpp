#include "Core/ArenaPlayerState.h"

#include "GAS/ArenaAbilitySystemComponent.h"
#include "GAS/ArenaAttributeSet.h"

AArenaPlayerState::AArenaPlayerState()
{
	SetNetUpdateFrequency(100.0f);
	SetMinNetUpdateFrequency(33.0f);

	// 玩家 ASC 放在 PlayerState 上，后续死亡重生时可以保留长期 GAS 状态。
	AbilitySystemComponent = CreateDefaultSubobject<UArenaAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UArenaAttributeSet>(TEXT("AttributeSet"));
	// 显式注册 AttributeSet 子对象，确保 ASC 能发现并复制属性。
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

void AArenaPlayerState::SetGrantedStartupAbilities(bool bNewGrantedStartupAbilities)
{
	bGrantedStartupAbilities = bNewGrantedStartupAbilities;
}

void AArenaPlayerState::SetAppliedDefaultAttributes(bool bNewAppliedDefaultAttributes)
{
	bAppliedDefaultAttributes = bNewAppliedDefaultAttributes;
}
