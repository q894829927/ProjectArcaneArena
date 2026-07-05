#pragma once

#include "CoreMinimal.h"
#include "Character/ArenaCharacterBase.h"
#include "ArenaEnemyCharacter.generated.h"

class UArenaAbilitySystemComponent;
class UArenaAttributeSet;
class UGameplayEffect;
class UAbilitySystemComponent;

UCLASS()
class PROJECTARCANEARENA_API AArenaEnemyCharacter : public AArenaCharacterBase
{
	GENERATED_BODY()

public:
	AArenaEnemyCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TSubclassOf<UGameplayEffect> DefaultAttributeEffect;

private:
	void InitializeAbilityActorInfo();
	void ApplyDefaultAttributes();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UArenaAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UArenaAttributeSet> AttributeSet;

	bool bAppliedDefaultAttributes = false;
};
