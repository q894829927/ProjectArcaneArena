#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "GAS/ArenaGameplayAbility.h"
#include "ArenaGameplayAbility_Fireball.generated.h"

class AArenaFireballProjectile;
class AGameplayAbilityTargetActor;
class UAbilitySystemComponent;
class UAbilityTask_WaitTargetData;
class UGameplayEffect;

UCLASS(Blueprintable)
class PROJECTARCANEARENA_API UArenaGameplayAbility_Fireball : public UArenaGameplayAbility
{
	GENERATED_BODY()

public:
	// 设置 Fireball 的输入标签、伤害类型和状态阻断规则。
	UArenaGameplayAbility_Fireball();

protected:
	// 客户端采集鼠标 TargetData，服务端收到后提交消耗/冷却并生成投射物。
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	// TargetData 就绪后，服务端提交消耗/冷却并生成 Fireball。
	UFUNCTION()
	void OnTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData);

	// 目标选择取消时结束 Ability，避免等待远端 TargetData 悬挂。
	UFUNCTION()
	void OnTargetDataCancelled(const FGameplayAbilityTargetDataHandle& TargetData);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Projectile")
	TSubclassOf<AArenaFireballProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Targeting")
	TSubclassOf<AGameplayAbilityTargetActor> TargetActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage")
	FGameplayTag DamageTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage", meta = (ClampMin = "0.0"))
	float BaseDamage = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage", meta = (ClampMin = "0.0"))
	float SkillMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Targeting", meta = (ClampMin = "0.0"))
	float MaxTargetRange = 2200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Projectile", meta = (ClampMin = "0.0"))
	float SpawnForwardOffset = 90.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Projectile")
	float SpawnHeightOffset = 50.0f;

private:
	// 从 Location TargetData 中取出目标点，服务端会再做距离和方向兜底。
	bool ExtractTargetLocation(const FGameplayAbilityTargetDataHandle& TargetData, FVector& OutTargetLocation) const;

	// 根据角色和目标点计算服务端实际生成 transform。
	bool BuildProjectileSpawnTransform(AActor* AvatarActor, const FVector& TargetLocation, FTransform& OutSpawnTransform) const;

	// 只在服务端生成 replicated projectile，并注入 GE_Damage 所需参数。
	void SpawnFireballProjectile(AActor* AvatarActor, UAbilitySystemComponent* SourceASC, const FTransform& SpawnTransform) const;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitTargetData> ActiveTargetDataTask;

	bool bConsumedTargetData = false;
	bool bServerSpawnConsumed = false;
};
