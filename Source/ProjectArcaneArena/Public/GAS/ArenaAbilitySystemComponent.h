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

	// 在服务端路由实际伤害，并在目标首次死亡时向来源 ASC 追加 OnKill GameplayEvent。
	void RouteAuthoritativeDamageEvent(
		const FGameplayEffectSpec& DamageSpec,
		UAbilitySystemComponent* TargetAbilitySystemComponent,
		const FGameplayTagContainer& TargetTagsBeforeDamage,
		float AppliedDamage);
};
