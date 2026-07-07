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

	// Ability 输入标签由 Character/ASC 用来把本地输入路由到对应技能。
	const FGameplayTag& GetInputTag() const { return InputTag; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Input")
	FGameplayTag InputTag;
};
