#pragma once

#include "CoreMinimal.h"
#include "GAS/ArenaGameplayAbility.h"
#include "ArenaGameplayAbility_BasicAttack.generated.h"

class UGameplayEffect;

UCLASS(Blueprintable)
class PROJECTARCANEARENA_API UArenaGameplayAbility_BasicAttack : public UArenaGameplayAbility
{
	GENERATED_BODY()

public:
	UArenaGameplayAbility_BasicAttack();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	void DrawAttackRangeDebug(UWorld* World, const FVector& Start, const FVector& End, bool bHitTarget) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage")
	FGameplayTag DamageTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage", meta = (ClampMin = "0.0"))
	float BaseDamage = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage", meta = (ClampMin = "0.0"))
	float SkillMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Trace", meta = (ClampMin = "0.0"))
	float AttackRange = 275.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Trace", meta = (ClampMin = "0.0"))
	float AttackRadius = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Debug")
	bool bDrawDebugAttackRange = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Debug", meta = (ClampMin = "0.0"))
	float DebugAttackRangeDuration = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Debug")
	FLinearColor DebugAttackRangeHitColor = FLinearColor::Green;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Debug")
	FLinearColor DebugAttackRangeMissColor = FLinearColor::Red;
};
