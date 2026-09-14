#pragma once

#include "CoreMinimal.h"
#include "Core/ArenaEnemyAffixDataAsset.h"
#include "Engine/DataAsset.h"
#include "ArenaWaveDataAsset.generated.h"

class AArenaEnemyCharacter;
class UArenaEnemyAffixDataAsset;
class UGameplayEffect;

USTRUCT(BlueprintType)
struct PROJECTARCANEARENA_API FArenaWaveEnemyEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Wave")
	TSubclassOf<AArenaEnemyCharacter> EnemyClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Wave", meta = (ClampMin = "1"))
	int32 Count = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Wave|Elite", meta = (ClampMin = "0"))
	int32 EliteCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Wave|Elite", meta = (EditCondition = "EliteCount > 0"))
	TArray<TObjectPtr<UArenaEnemyAffixDataAsset>> EliteAffixPool;
};

USTRUCT(BlueprintType)
struct PROJECTARCANEARENA_API FArenaWaveConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Wave")
	TArray<FArenaWaveEnemyEntry> Enemies;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Wave", meta = (ClampMin = "0.01"))
	float SpawnInterval = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Wave")
	bool bBossWave = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Wave", meta = (ClampMin = "0"))
	int32 RewardCount = 3;
};

UCLASS(BlueprintType)
class PROJECTARCANEARENA_API UArenaWaveDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UArenaWaveDataAsset();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Wave")
	TArray<FArenaWaveConfig> Waves;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Wave|Elite")
	FArenaEliteBaselineConfig EliteBaseline;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Wave|Elite")
	TSubclassOf<UGameplayEffect> EliteBaselineEffectClass;
};
