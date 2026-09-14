#include "Character/ArenaCharacterBase.h"

#include "Components/ArenaHitReactionComponent.h"

// 构造角色基类，开启复制和移动复制作为玩家/敌人的共同基础。
AArenaCharacterBase::AArenaCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(true);

	HitReactionComponent = CreateDefaultSubobject<UArenaHitReactionComponent>(TEXT("HitReactionComponent"));
}

// 基类默认没有 ASC，具体角色类型负责返回自己的 AbilitySystemComponent。
UAbilitySystemComponent* AArenaCharacterBase::GetAbilitySystemComponent() const
{
	return nullptr;
}

// 基类没有资产引用，玩家可直接在公共组件上配置伤害数字 Blueprint。
TSubclassOf<AArenaDamageNumberActor> AArenaCharacterBase::GetDamageNumberActorClassForFeedback() const
{
	return nullptr;
}

// 基类使用公共组件的默认偏移，敌人可覆盖以兼容已有蓝图值。
FVector AArenaCharacterBase::GetDamageNumberSpawnOffsetForFeedback(const FVector& ComponentDefault) const
{
	return ComponentDefault;
}
