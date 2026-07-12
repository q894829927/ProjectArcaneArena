#include "Character/ArenaCharacterBase.h"

// 构造角色基类，开启复制和移动复制作为玩家/敌人的共同基础。
AArenaCharacterBase::AArenaCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(true);
}

// 基类默认没有 ASC，具体角色类型负责返回自己的 AbilitySystemComponent。
UAbilitySystemComponent* AArenaCharacterBase::GetAbilitySystemComponent() const
{
	return nullptr;
}
