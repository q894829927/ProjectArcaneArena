#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "ArenaPlayerState.generated.h"

class UArenaAbilitySystemComponent;
class UArenaAttributeSet;
class UAbilitySystemComponent;

UCLASS()
class PROJECTARCANEARENA_API AArenaPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AArenaPlayerState();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "Arena|GAS")
	UArenaAbilitySystemComponent* GetArenaAbilitySystemComponent() const;

	UFUNCTION(BlueprintPure, Category = "Arena|GAS")
	UArenaAttributeSet* GetArenaAttributeSet() const;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UArenaAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UArenaAttributeSet> AttributeSet;
};
