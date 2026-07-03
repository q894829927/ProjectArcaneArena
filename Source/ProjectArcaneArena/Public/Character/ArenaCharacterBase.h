#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "ArenaCharacterBase.generated.h"

UCLASS(Abstract)
class PROJECTARCANEARENA_API AArenaCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AArenaCharacterBase();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
};
