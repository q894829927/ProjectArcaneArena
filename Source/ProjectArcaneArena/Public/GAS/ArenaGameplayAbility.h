#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "ArenaGameplayAbility.generated.h"

UCLASS(Abstract, Blueprintable)
class PROJECTARCANEARENA_API UArenaGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UArenaGameplayAbility();

	const FGameplayTag& GetInputTag() const { return InputTag; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Input")
	FGameplayTag InputTag;
};
