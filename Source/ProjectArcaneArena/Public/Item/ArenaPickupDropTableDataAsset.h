#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ArenaPickupDropTableDataAsset.generated.h"

class AArenaPickupActor;

USTRUCT(BlueprintType)
struct PROJECTARCANEARENA_API FArenaPickupDropEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Pickup", meta = (AllowAbstract = "false"))
	TSubclassOf<AArenaPickupActor> PickupClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Pickup", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;
};

UCLASS(BlueprintType)
class PROJECTARCANEARENA_API UArenaPickupDropTableDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Pickup", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DropChance = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Pickup")
	TArray<FArenaPickupDropEntry> Entries;
};
