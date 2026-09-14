#pragma once

#include "CoreMinimal.h"
#include "GAS/ArenaGameplayAbility.h"
#include "ArenaGameplayAbility_ShieldBreakBlast.generated.h"

class UArenaUpgradeDataAsset;
class UAbilitySystemComponent;
class UGameplayEffect;

UCLASS(Blueprintable)
class PROJECTARCANEARENA_API UArenaGameplayAbility_ShieldBreakBlast : public UArenaGameplayAbility
{
	GENERATED_BODY()

public:
	// 配置服务器 OnShieldBreak 事件触发、默认爆发范围和死亡阻断规则。
	UArenaGameplayAbility_ShieldBreakBlast();

protected:
	// 只响应护盾拥有者自身的权威破盾事件，并要求已经拥有对应升级标签。
	virtual bool ShouldAbilityRespondToEvent(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayEventData* Payload) const override;

	// 从升级 SourceObject 读取基础伤害，在玩家位置执行 Cue 并结算范围物理伤害。
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Shield Break Blast")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Shield Break Blast", meta = (ClampMin = "0.0"))
	float ExplosionRadius = 300.0f;

private:
	// 收集范围内有效敌人，并通过 GE_Damage 对每个目标独立结算 Secondary Physical 伤害。
	void ApplyExplosionDamage(
		UAbilitySystemComponent* SourceASC,
		AActor* SourceAvatar,
		const UArenaUpgradeDataAsset* UpgradeData,
		float BaseDamage) const;
};
