#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ArenaEnemyAffixDataAsset.generated.h"

class UGameplayEffect;

UENUM(BlueprintType)
enum class EArenaEnemyAffixBehavior : uint8
{
	Volatile,
	ArcaneWarden,
	Frenzy
};

USTRUCT(BlueprintType)
struct PROJECTARCANEARENA_API FArenaEliteBaselineConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Elite", meta = (ClampMin = "1.0"))
	float MaxHealthMultiplier = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Elite", meta = (ClampMin = "1.0"))
	float AttackPowerMultiplier = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Elite", meta = (ClampMin = "0.0"))
	float DefenseBonus = 5.0f;
};

USTRUCT(BlueprintType)
struct PROJECTARCANEARENA_API FArenaVolatileAffixConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Elite|Volatile", meta = (ClampMin = "0.0"))
	float TelegraphDuration = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Elite|Volatile", meta = (ClampMin = "0.0"))
	float ExplosionRadius = 325.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Elite|Volatile", meta = (ClampMin = "0.0"))
	float BaseDamage = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Elite|Volatile")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Elite|Volatile")
	FGameplayTag TelegraphGameplayCueTag;
};

USTRUCT(BlueprintType)
struct PROJECTARCANEARENA_API FArenaArcaneWardenAffixConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Elite|Arcane Warden", meta = (ClampMin = "0.0"))
	float InitialDelay = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Elite|Arcane Warden", meta = (ClampMin = "0.1"))
	float PulseInterval = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Elite|Arcane Warden", meta = (ClampMin = "0.0"))
	float Radius = 550.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Elite|Arcane Warden", meta = (ClampMin = "0.0"))
	float ShieldCap = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Elite|Arcane Warden")
	TSubclassOf<UGameplayEffect> ShieldEffectClass;
};

USTRUCT(BlueprintType)
struct PROJECTARCANEARENA_API FArenaFrenzyAffixConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Elite|Frenzy", meta = (ClampMin = "0.01", ClampMax = "0.99"))
	float HealthThreshold = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Elite|Frenzy")
	TSubclassOf<UGameplayEffect> FrenzyEffectClass;
};

UCLASS(BlueprintType)
class PROJECTARCANEARENA_API UArenaEnemyAffixDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// 校验词缀身份、Tag、Cue、GE 和行为数值，服务器会在开波前复用该检查。
	bool IsRuntimeDefinitionValid(FText* OutError = nullptr) const;

#if WITH_EDITOR
	// 在 Content Validation 阶段拒绝无法安全进入服务器词缀流程的配置。
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Elite")
	FName AffixID;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Elite")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Elite", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Elite")
	FLinearColor AccentColor = FLinearColor(0.8f, 0.15f, 0.1f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Elite")
	EArenaEnemyAffixBehavior Behavior = EArenaEnemyAffixBehavior::Volatile;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Elite")
	FGameplayTag AffixTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Elite")
	FGameplayTag ActiveGameplayCueTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Elite")
	FGameplayTag TriggerGameplayCueTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Elite")
	FArenaVolatileAffixConfig Volatile;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Elite")
	FArenaArcaneWardenAffixConfig ArcaneWarden;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Elite")
	FArenaFrenzyAffixConfig Frenzy;
};
