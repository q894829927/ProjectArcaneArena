#include "Character/ArenaCharacterBase.h"

AArenaCharacterBase::AArenaCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(true);
}

UAbilitySystemComponent* AArenaCharacterBase::GetAbilitySystemComponent() const
{
	return nullptr;
}
