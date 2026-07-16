#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "ArenaDashTrailArea.generated.h"

class UArenaUpgradeDataAsset;
class UAbilitySystemComponent;
class UGameplayEffect;
class USceneComponent;

UCLASS(Blueprintable)
class PROJECTARCANEARENA_API AArenaDashTrailArea : public AActor
{
	GENERATED_BODY()

public:
	// 创建不使用 Tick 的复制路径区域，伤害仅由服务器计时器结算。
	AArenaDashTrailArea();

	// 由服务器被动技能注入实际 Dash 路径、伤害数据和区域生命周期。
	void InitializeTrail(
		UAbilitySystemComponent* InSourceASC,
		AActor* InSourceActor,
		const UArenaUpgradeDataAsset* InUpgradeData,
		TSubclassOf<UGameplayEffect> InDamageEffectClass,
		float InBaseDamage,
		const FVector& InTrailStart,
		const FVector& InTrailEnd,
		float InTrailRadius,
		float InTrailDuration,
		float InDamageTickInterval);

protected:
	// 刷新路径变换；各端为本 Area 启动独立 Cue，仅服务器启动伤害计时。
	virtual void BeginPlay() override;

	// 清理伤害计时器并在当前世界成对移除本 Area 的持续 Cue。
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 复制路径几何和生命周期参数，供客户端蓝图表现读取。
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena|Dash Trail")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_TrailGeometry, Category = "Arena|Dash Trail")
	FVector TrailStart = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_TrailGeometry, Category = "Arena|Dash Trail")
	FVector TrailEnd = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_TrailGeometry, Category = "Arena|Dash Trail", meta = (ClampMin = "0.0"))
	float TrailRadius = 120.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Replicated, Category = "Arena|Dash Trail", meta = (ClampMin = "0.01"))
	float TrailDuration = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Replicated, Category = "Arena|Dash Trail", meta = (ClampMin = "0.01"))
	float DamageTickInterval = 0.5f;

	// 蓝图可按复制的起终点、半径和持续时间组合更精确的路径 Niagara。
	UFUNCTION(BlueprintImplementableEvent, Category = "Arena|Dash Trail")
	void K2_OnTrailGeometryChanged(FVector StartLocation, FVector EndLocation, float Radius, float Duration);

private:
	// 任一复制几何字段变化时重建世界变换并通知蓝图表现。
	UFUNCTION()
	void OnRep_TrailGeometry();

	// 服务端单次收集路径附近敌人，并保证每个目标每 Tick 最多结算一次。
	void ApplyDamageTick();
	// 使用二维点到线段距离执行严格路径判定，排除死亡和无敌目标。
	bool CanDamageTarget(AActor* TargetActor, UAbilitySystemComponent* TargetASC) const;
	// 为单个目标创建独立 Lightning Damage Spec，并继续走标准伤害管线。
	void ApplyDamageToTarget(UAbilitySystemComponent* TargetASC) const;
	// 根据路径中点与方向刷新 Actor 变换，供复制和 GameplayCue 共用。
	void RefreshTrailTransform();
	// 向蓝图广播当前复制路径参数，蓝图只负责表现。
	void NotifyTrailGeometryChanged();
	// 以本复制 Area 为 Cue Target 启动独立本地持续表现，避免同 Tag 路径互相移除。
	void AddTrailGameplayCue();
	// 只结束与本 Area Target 关联的本地持续表现。
	void RemoveTrailGameplayCue();

	FTimerHandle DamageTickTimerHandle;
	TWeakObjectPtr<UAbilitySystemComponent> SourceAbilitySystemComponent;
	TWeakObjectPtr<AActor> SourceActor;
	TWeakObjectPtr<UArenaUpgradeDataAsset> UpgradeData;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	float BaseDamage = 6.0f;
	int32 MaxDamageTicks = 0;
	int32 DamageTicksApplied = 0;
	bool bAddedTrailGameplayCue = false;
};
