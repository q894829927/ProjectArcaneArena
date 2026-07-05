#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "ArenaAbilitySystemComponent.generated.h"

UCLASS()
class PROJECTARCANEARENA_API UArenaAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UArenaAbilitySystemComponent();

	void AbilityInputTagPressed(const FGameplayTag& InputTag);
};
