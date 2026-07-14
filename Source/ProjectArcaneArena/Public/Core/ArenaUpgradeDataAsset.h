#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ArenaUpgradeDataAsset.generated.h"

class UGameplayAbility;
class UGameplayEffect;
class UTexture2D;

UENUM(BlueprintType)
enum class EArenaUpgradeRarity : uint8
{
	Common,
	Rare,
	Epic,
	Legendary
};

UCLASS(BlueprintType)
class PROJECTARCANEARENA_API UArenaUpgradeDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Upgrade")
	FName UpgradeID;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Upgrade")
	FText UpgradeName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Upgrade", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Upgrade")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Upgrade")
	EArenaUpgradeRarity Rarity = EArenaUpgradeRarity::Common;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Upgrade|Tags")
	FGameplayTagContainer UpgradeTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Upgrade|Tags")
	FGameplayTagContainer RequiredTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Upgrade|Tags")
	FGameplayTagContainer BlockedTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Upgrade|Grant")
	TSubclassOf<UGameplayEffect> GrantedGameplayEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Upgrade|Grant")
	TSubclassOf<UGameplayAbility> GrantedAbility;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Upgrade|Routing")
	FGameplayTag TargetAbilityTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Upgrade|Routing")
	FGameplayTag TriggerEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Upgrade|Routing")
	FGameplayTag DamageTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Upgrade|Value")
	float NumericValue = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Upgrade|Stack", meta = (ClampMin = "1"))
	int32 MaxStacks = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Upgrade|Stack")
	bool bStackable = false;
};
