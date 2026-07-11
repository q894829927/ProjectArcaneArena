#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "GAS/ArenaGameplayAbility.h"
#include "ArenaGameplayAbility_LightningStorm.generated.h"

class AArenaLightningStormArea;
class AGameplayAbilityTargetActor;
class UAbilitySystemComponent;
class UAbilityTask_WaitTargetData;
class UGameplayEffect;

UCLASS(Blueprintable)
class PROJECTARCANEARENA_API UArenaGameplayAbility_LightningStorm : public UArenaGameplayAbility
{
	GENERATED_BODY()

public:
	// 设置 LightningStorm 的输入标签、伤害类型和冷却阻断规则。
	UArenaGameplayAbility_LightningStorm();

protected:
	// 客户端采集鼠标落点，服务端校验距离后生成持续伤害区域。
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	UFUNCTION()
	void OnTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData);

	UFUNCTION()
	void OnTargetDataCancelled(const FGameplayAbilityTargetDataHandle& TargetData);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|LightningStorm")
	TSubclassOf<AArenaLightningStormArea> StormAreaClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Targeting")
	TSubclassOf<AGameplayAbilityTargetActor> TargetActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage")
	FGameplayTag DamageTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage", meta = (ClampMin = "0.0"))
	float BaseDamage = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage", meta = (ClampMin = "0.0"))
	float SkillMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Targeting", meta = (ClampMin = "0.0"))
	float MaxTargetRange = 1800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|LightningStorm", meta = (ClampMin = "0.0"))
	float StormRadius = 450.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|LightningStorm", meta = (ClampMin = "0.01"))
	float StormDuration = 4.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|LightningStorm", meta = (ClampMin = "0.01"))
	float DamageTickInterval = 0.5f;

private:
	bool ExtractTargetLocation(const FGameplayAbilityTargetDataHandle& TargetData, FVector& OutTargetLocation) const;
	bool BuildStormSpawnTransform(AActor* AvatarActor, const FVector& TargetLocation, FTransform& OutSpawnTransform) const;
	void SpawnLightningStormArea(AActor* AvatarActor, UAbilitySystemComponent* SourceASC, const FTransform& SpawnTransform) const;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitTargetData> ActiveTargetDataTask;
};
