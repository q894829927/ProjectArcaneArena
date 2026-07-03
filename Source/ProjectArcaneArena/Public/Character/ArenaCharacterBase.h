#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ArenaCharacterBase.generated.h"

UCLASS(Abstract)
class PROJECTARCANEARENA_API AArenaCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	AArenaCharacterBase();
};
