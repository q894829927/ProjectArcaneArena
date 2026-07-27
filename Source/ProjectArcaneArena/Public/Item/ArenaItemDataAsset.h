#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ArenaItemDataAsset.generated.h"

class AArenaInventoryPickupActor;
class UGameplayEffect;
class UTexture2D;

UCLASS(BlueprintType)
class PROJECTARCANEARENA_API UArenaItemDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Item")
	FGameplayTag ItemTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Item")
	FGameplayTagContainer ItemTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Item")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Item", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Item")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Item", meta = (ClampMin = "1"))
	int32 MaxStackSize = 10;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Item|Use")
	TSubclassOf<UGameplayEffect> UseGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Item|Use")
	FGameplayTag SetByCallerMagnitudeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Item|Use", meta = (ClampMin = "0.0"))
	float UseMagnitude = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Item|Use")
	TSubclassOf<UGameplayEffect> CooldownGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Item|World")
	TSubclassOf<AArenaInventoryPickupActor> WorldPickupClass;
};
