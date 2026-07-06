#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ArenaDamageNumberWidget.generated.h"

class UTextBlock;

UCLASS()
class PROJECTARCANEARENA_API UArenaDamageNumberWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void SetDamageAmount(float InDamageAmount);

	UFUNCTION(BlueprintPure, Category = "Arena|UI")
	float GetDamageAmount() const { return DamageAmount; }

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI")
	TObjectPtr<UTextBlock> DamageText;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	float DamageAmount = 0.0f;
};
