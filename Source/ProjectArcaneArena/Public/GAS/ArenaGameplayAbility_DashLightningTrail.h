#pragma once

#include "CoreMinimal.h"
#include "GAS/ArenaGameplayAbility.h"
#include "ArenaGameplayAbility_DashLightningTrail.generated.h"

class AArenaDashTrailArea;
class UArenaUpgradeDataAsset;
class UGameplayEffect;

UCLASS(Blueprintable)
class PROJECTARCANEARENA_API UArenaGameplayAbility_DashLightningTrail : public UArenaGameplayAbility
{
	GENERATED_BODY()

public:
	// 配置服务器 OnDashEnd 事件触发、默认路径 Area 和死亡阻断规则。
	UArenaGameplayAbility_DashLightningTrail();

protected:
	// 只响应拥有对应升级的玩家自身正常 Dash 完成事件。
	virtual bool ShouldAbilityRespondToEvent(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayEventData* Payload) const override;

	// 从事件 TargetData 读取实际起终点，并在服务器生成唯一 Trail Area。
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Dash Lightning Trail")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Dash Lightning Trail")
	TSubclassOf<AArenaDashTrailArea> TrailAreaClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Dash Lightning Trail", meta = (ClampMin = "0.0"))
	float TrailRadius = 120.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Dash Lightning Trail", meta = (ClampMin = "0.01"))
	float TrailDuration = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Dash Lightning Trail", meta = (ClampMin = "0.01"))
	float DamageTickInterval = 0.5f;

private:
	// 验证内置 Location TargetData，并提取服务器记录的实际 Dash 起终点。
	bool ExtractDashPath(
		const FGameplayEventData* TriggerEventData,
		FVector& OutTrailStart,
		FVector& OutTrailEnd) const;
};
