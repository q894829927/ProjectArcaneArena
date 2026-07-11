#include "Core/ArenaPlayerState.h"

#include "GAS/ArenaAbilitySystemComponent.h"
#include "GAS/ArenaAttributeSet.h"

// 构造玩家状态，创建长期存在的 ASC 和 AttributeSet。
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

// 返回标准 GAS 接口需要的 AbilitySystemComponent。
UAbilitySystemComponent* AArenaPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

// 返回项目自定义 ASC，供输入路由和项目扩展调用。
UArenaAbilitySystemComponent* AArenaPlayerState::GetArenaAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

// 返回玩家 AttributeSet，供 HUD 和初始化流程读取属性。
UArenaAttributeSet* AArenaPlayerState::GetArenaAttributeSet() const
{
	return AttributeSet;
}

// 记录启动技能是否已经授予，避免 Possess/复制路径重复授予。
void AArenaPlayerState::SetGrantedStartupAbilities(bool bNewGrantedStartupAbilities)
{
	bGrantedStartupAbilities = bNewGrantedStartupAbilities;
}

// 记录默认属性是否已经应用，避免重生或重复初始化时叠加属性。
void AArenaPlayerState::SetAppliedDefaultAttributes(bool bNewAppliedDefaultAttributes)
{
	bAppliedDefaultAttributes = bNewAppliedDefaultAttributes;
}
