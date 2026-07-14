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

	// 在服务端把实际生效的伤害转换为来源 ASC 上的类型化 GameplayEvent。
	void RouteAuthoritativeDamageEvent(
		const FGameplayEffectSpec& DamageSpec,
		UAbilitySystemComponent* TargetAbilitySystemComponent,
		const FGameplayTagContainer& TargetTagsBeforeDamage,
		float AppliedDamage);
};
