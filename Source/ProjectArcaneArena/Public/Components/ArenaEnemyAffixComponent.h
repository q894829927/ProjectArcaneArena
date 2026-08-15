#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/ArenaEnemyAffixDataAsset.h"
#include "GameplayEffectTypes.h"
#include "TimerManager.h"
#include "ArenaEnemyAffixComponent.generated.h"

class AArenaEnemyCharacter;
class UArenaAbilitySystemComponent;
class UArenaEnemyAffixDataAsset;
class UGameplayEffect;

UCLASS(ClassGroup = (Arena), meta = (BlueprintSpawnableComponent))
class PROJECTARCANEARENA_API UArenaEnemyAffixComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UArenaEnemyAffixComponent();

	// Deferred Spawn 期间注入本次词缀与全局基础强化，普通生成路径保持空配置。
	void ConfigureBeforeSpawn(
		UArenaEnemyAffixDataAsset* InAffixData,
		const FArenaEliteBaselineConfig& InBaselineConfig,
		TSubclassOf<UGameplayEffect> InBaselineEffectClass);

	// 默认敌人属性完成后按 GAS 路径应用精英强化、标签、Cue 与行为计时。
	bool InitializeAfterDefaultAttributes();

	// 死亡入口先清理常驻行为；Volatile 返回 true 表示延迟最终死亡广播。
	bool BeginOwnerDeath();

	// 终局、销毁或初始化失败时幂等清理 Timer、委托与持续 Cue。
	void CancelAffixRuntime();

	UFUNCTION(BlueprintPure, Category = "Arena|Elite")
	bool IsElite() const { return ActiveAffixData != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Arena|Elite")
	UArenaEnemyAffixDataAsset* GetActiveAffixData() const { return ActiveAffixData; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void OnRep_ActiveAffixData();

	// 应用精英基础差额并同步 Elite/Affix replicated loose tags。
	bool ApplyEliteBaselineAndTags();
	// 添加或移除附着于 Avatar Root 的持续词缀 Cue。
	void AddLivingAffixCue();
	void RemoveLivingAffixCue();
	// 定时为同一 WaveManager 管理的附近友军补足 Shield。
	void ExecuteArcaneWardenPulse();
	// 只在首次穿过阈值时应用 Infinite Frenzy GE。
	void HandleOwnerHealthChanged(const FOnAttributeChangeData& Data);
	// 结算一次带 LOS 的服务器范围伤害，然后交还 Enemy 最终死亡流程。
	void ExecuteVolatileExplosion();
	void CompleteDeferredVolatileDeath();
	bool HasWorldLineOfSightTo(const AActor* TargetActor) const;
	AArenaEnemyCharacter* GetEnemyOwner() const;
	UArenaAbilitySystemComponent* GetOwnerASC() const;

	UPROPERTY(ReplicatedUsing = OnRep_ActiveAffixData, Transient)
	TObjectPtr<UArenaEnemyAffixDataAsset> ActiveAffixData;

	FArenaEliteBaselineConfig BaselineConfig;
	TSubclassOf<UGameplayEffect> BaselineEffectClass;
	FDelegateHandle HealthChangedDelegateHandle;
	FTimerHandle ArcaneWardenTimerHandle;
	FTimerHandle VolatileExplosionTimerHandle;
	FVector VolatileExplosionLocation = FVector::ZeroVector;
	bool bInitialized = false;
	bool bLivingCueActive = false;
	bool bFrenzyTriggered = false;
	bool bVolatileDeathPending = false;
};
