#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ArenaEnemyHealthBarWidget.generated.h"

class UProgressBar;

UCLASS()
class PROJECTARCANEARENA_API UArenaEnemyHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void SetHealthValues(float InHealth, float InMaxHealth);

	UFUNCTION(BlueprintPure, Category = "Arena|UI")
	float GetHealthPercent() const { return HealthPercent; }

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI")
	TObjectPtr<UProgressBar> HealthProgressBar;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	float CurrentHealth = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	float CurrentMaxHealth = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	float HealthPercent = 0.0f;
};
