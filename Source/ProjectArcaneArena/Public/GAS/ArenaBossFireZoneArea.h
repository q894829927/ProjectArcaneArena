#pragma once

#include "CoreMinimal.h"
#include "Core/ArenaGameState.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "ArenaBossFireZoneArea.generated.h"

class AArenaPlayerCharacter;
class UAbilitySystemComponent;
class UGameplayEffect;
class USceneComponent;

UCLASS(Blueprintable)
class PROJECTARCANEARENA_API AArenaBossFireZoneArea : public AActor
{
	GENERATED_BODY()

public:
	// 创建不使用 Tick 的复制火区；客户端只显示范围，服务器运行周期伤害。
	AArenaBossFireZoneArea();

	// 由服务器 Ability 在 FinishSpawning 前注入伤害、范围和生命周期快照。
	void InitializeFireZone(
		UAbilitySystemComponent* InSourceASC,
		AActor* InSourceActor,
		TSubclassOf<UGameplayEffect> InDamageEffectClass,
		float InBaseDamage,
		float InSkillMultiplier,
		float InZoneRadius,
		float InDamageHalfHeight,
		float InZoneDuration,
		float InDamageTickInterval);

protected:
	// 各端启动本 Area 的独立 Active Cue；服务器额外绑定清理条件和伤害计时器。
	virtual void BeginPlay() override;
	// 清理 Timer、来源委托、阶段委托和本 Area 的持续 Cue。
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// 复制客户端表现所需的半径、持续时间和 Tick 间隔。
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena|Boss|Fire Zone")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_ZonePresentation, Category = "Arena|Boss|Fire Zone", meta = (ClampMin = "0.0"))
	float ZoneRadius = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_ZonePresentation, Category = "Arena|Boss|Fire Zone", meta = (ClampMin = "0.01"))
	float ZoneDuration = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Replicated, Category = "Arena|Boss|Fire Zone", meta = (ClampMin = "0.01"))
	float DamageTickInterval = 0.5f;

	// 蓝图可使用复制参数补充非 GameplayCue 表现，但不得承担伤害或生命周期。
	UFUNCTION(BlueprintImplementableEvent, Category = "Arena|Boss|Fire Zone")
	void K2_OnZonePresentationChanged(float Radius, float Duration);

private:
	// 复制表现参数变化时刷新本 Area 的 GameplayCue 半径和蓝图扩展点。
	UFUNCTION()
	void OnRep_ZonePresentation();
	// 来源 Actor 被销毁时立即终止火区，避免孤立伤害区域继续运行。
	UFUNCTION()
	void HandleSourceActorDestroyed(AActor* DestroyedActor);
	// 游戏离开 Combat 时销毁火区，覆盖 Victory、Defeat 和异常阶段切换。
	UFUNCTION()
	void HandleGamePhaseChanged(EArenaGamePhase OldPhase, EArenaGamePhase NewPhase);

	// 监听来源死亡、来源销毁和 GameState 阶段，Boss Stun 不清除已生成火区。
	void BindServerCleanupDelegates();
	// 对称解绑来源和阶段委托，支持寿命结束、死亡与世界切换。
	void UnbindServerCleanupDelegates();
	// 来源死亡标签出现时立即销毁本火区。
	void HandleSourceDeadTagChanged(const FGameplayTag CallbackTag, int32 NewCount);
	// 服务器先验证伤害来源，再执行一次火区结算并保证每个目标在当前 Tick 最多处理一次。
	void ApplyDamageTick();
	// 使用严格二维半径和垂直高度过滤存活玩家。
	bool CanDamagePlayer(const AArenaPlayerCharacter* PlayerCharacter, UAbilitySystemComponent* TargetASC) const;
	// 为单个玩家创建独立 Fire Damage Spec，继续走标准 Shield 与伤害管线。
	void ApplyDamageToTarget(UAbilitySystemComponent* TargetASC);
	// 以本复制 Area 为 Cue Target 启动独立本地表现，允许多个同 Tag 火区共存。
	void AddZoneGameplayCue();
	// 只移除与本 Area Target 关联的持续表现。
	void RemoveZoneGameplayCue();
	// 通知蓝图当前最终复制参数。
	void NotifyZonePresentationChanged();

	FTimerHandle DamageTickTimerHandle;
	TWeakObjectPtr<UAbilitySystemComponent> SourceAbilitySystemComponent;
	TWeakObjectPtr<AActor> SourceActor;
	TWeakObjectPtr<AArenaGameState> BoundGameState;
	FDelegateHandle SourceDeadTagDelegateHandle;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	float BaseDamage = 5.0f;
	float SkillMultiplier = 1.0f;
	float DamageHalfHeight = 180.0f;
	int32 MaxDamageTicks = 0;
	int32 DamageTicksApplied = 0;
	bool bAddedZoneGameplayCue = false;
};
