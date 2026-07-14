#pragma once

#include "CoreMinimal.h"
#include "GAS/ArenaGameplayAbility.h"
#include "ArenaGameplayAbility_Overload.generated.h"

class AArenaEnemyCharacter;
class UArenaUpgradeDataAsset;
class UAbilitySystemComponent;
class UGameplayEffect;

UCLASS(Blueprintable)
class PROJECTARCANEARENA_API UArenaGameplayAbility_Overload : public UArenaGameplayAbility
{
	GENERATED_BODY()

public:
	// 配置服务器被动触发标签、默认爆炸范围和来源独立限频效果。
	UArenaGameplayAbility_Overload();

protected:
	// 只响应 Lightning 实际伤害事件中的命中前 Burning 目标，并拒绝 Secondary 递归。
	virtual bool ShouldAbilityRespondToEvent(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayEventData* Payload) const override;

	// 服务端应用目标限频后，在命中位置通过 GE_Damage 结算范围闪电伤害。
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Overload")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Overload")
	TSubclassOf<UGameplayEffect> OverloadLockoutEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Overload", meta = (ClampMin = "0.0"))
	float ExplosionRadius = 300.0f;

private:
	// 统一验证事件来源、升级标签、目标类型、Burning 快照和 Secondary 过滤。
	bool ResolveTriggerActors(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayEventData* Payload,
		UAbilitySystemComponent*& OutSourceASC,
		AArenaEnemyCharacter*& OutTriggerTarget,
		UAbilitySystemComponent*& OutTargetASC) const;

	// 检查目标上的 Overload Lockout 是否由当前来源 ASC 创建。
	bool HasSourceLockout(
		const UAbilitySystemComponent* TargetASC,
		const UAbilitySystemComponent* SourceASC) const;

	// 为存活触发目标写入来源独立的限频 GE，配置错误时拒绝本次爆炸。
	bool ApplySourceLockout(
		UAbilitySystemComponent* SourceASC,
		UAbilitySystemComponent* TargetASC,
		AActor* SourceAvatar,
		const UArenaUpgradeDataAsset* UpgradeData) const;

	// 收集范围内有效敌人并通过现有 GE_Damage 路径结算 Secondary Lightning 伤害。
	void ApplyExplosionDamage(
		UAbilitySystemComponent* SourceASC,
		AActor* SourceAvatar,
		const FVector& ExplosionLocation,
		const UArenaUpgradeDataAsset* UpgradeData,
		float BaseDamage) const;
};
