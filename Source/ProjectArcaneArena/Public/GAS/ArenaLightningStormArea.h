#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "ArenaLightningStormArea.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class USphereComponent;

UCLASS(Blueprintable)
class PROJECTARCANEARENA_API AArenaLightningStormArea : public AActor
{
	GENERATED_BODY()

public:
	AArenaLightningStormArea();

	// 由服务端技能注入伤害、Shocked 构筑快照和范围参数，客户端只接收复制后的表现 Actor。
	void InitializeStorm(
		UAbilitySystemComponent* InSourceASC,
		AActor* InSourceActor,
		TSubclassOf<UGameplayEffect> InDamageEffectClass,
		FGameplayTag InDamageTypeTag,
		float InBaseDamage,
		float InSkillMultiplier,
		TSubclassOf<UGameplayEffect> InShockedEffectClass,
		bool bInShockedUnlocked,
		float InShockedLightningDamageBonus,
		float InStormRadius,
		float InStormDuration,
		float InDamageTickInterval);

protected:
	// 服务端启动周期伤害计时，客户端保留 Actor 用于后续挂接表现。
	virtual void BeginPlay() override;

	// Area 生命周期结束时由服务器移除持续 Cue，避免客户端残留循环特效。
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 复制运行时半径，确保蓝图表现可以读到技能配置后的范围。
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena|LightningStorm")
	TObjectPtr<USphereComponent> AreaComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_StormRadius, Category = "Arena|LightningStorm", meta = (ClampMin = "0.0"))
	float StormRadius = 450.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Replicated, Category = "Arena|LightningStorm", meta = (ClampMin = "0.01"))
	float StormDuration = 4.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Replicated, Category = "Arena|LightningStorm", meta = (ClampMin = "0.01"))
	float DamageTickInterval = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|LightningStorm|Debug")
	bool bDrawDebugRadius = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|LightningStorm|Debug")
	FColor DebugRadiusColor = FColor::Cyan;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|LightningStorm|Debug", meta = (ClampMin = "0.0"))
	float DebugRadiusThickness = 4.0f;

private:
	UFUNCTION()
	void OnRep_StormRadius();

	// 单次服务端结算范围内目标，所有数值仍通过 GE_Damage 和 ExecCalc_Damage 处理。
	void ApplyDamageTick();
	// 过滤自己、死亡和无敌目标，确保无敌期间不会刷新负面状态。
	bool CanDamageTarget(AActor* TargetActor, UAbilitySystemComponent* TargetASC) const;
	// 通过 GE_Damage 结算单个目标，本函数不直接修改属性。
	void ApplyDamageToTarget(UAbilitySystemComponent* TargetASC);
	// 在本次伤害结算后为存活目标应用或刷新服务器权威 Shocked GE。
	void ApplyShockedToTarget(UAbilitySystemComponent* TargetASC);
	void RefreshAreaRadius() const;
	void DrawDebugDamageRadius() const;

	FTimerHandle DamageTickTimerHandle;

	TWeakObjectPtr<UAbilitySystemComponent> SourceAbilitySystemComponent;
	TWeakObjectPtr<AActor> SourceActor;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> ShockedEffectClass;

	FGameplayTag DamageTypeTag;
	float BaseDamage = 8.0f;
	float SkillMultiplier = 1.0f;
	float ShockedLightningDamageBonus = 0.0f;
	int32 MaxDamageTicks = 0;
	int32 DamageTicksApplied = 0;
	bool bShockedUnlocked = false;
	bool bAddedActiveGameplayCue = false;
};
