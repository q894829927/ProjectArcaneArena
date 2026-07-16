#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "ArenaAbilitySystemComponent.generated.h"

struct FGameplayEffectSpec;

UCLASS()
class PROJECTARCANEARENA_API UArenaAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UArenaAbilitySystemComponent();

	// 根据输入标签查找对应 AbilitySpec，并交给 GAS 标准激活流程处理。
	void AbilityInputTagPressed(const FGameplayTag& InputTag);

	// 在服务端按伤害、暴击、首次击杀顺序向来源 ASC 路由 GameplayEvent。
	void RouteAuthoritativeDamageEvent(
		const FGameplayEffectSpec& DamageSpec,
		UAbilitySystemComponent* TargetAbilitySystemComponent,
		const FGameplayTagContainer& TargetTagsBeforeDamage,
		float AppliedDamage);

	// 在服务端向护盾拥有者路由一次伤害驱动的破盾事件，供自身被动 Ability 响应。
	void RouteAuthoritativeShieldBreakEvent(
		const FGameplayEffectSpec& DamageSpec,
		UAbilitySystemComponent* SourceAbilitySystemComponent,
		const FGameplayTagContainer& TargetTagsBeforeDamage,
		float AppliedShieldDamage);
};
