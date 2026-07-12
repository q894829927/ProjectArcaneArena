#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "ArenaWaveManager.generated.h"

class AArenaEnemyCharacter;
class ATargetPoint;
class UArenaWaveDataAsset;

UCLASS()
class PROJECTARCANEARENA_API AArenaWaveManager : public AActor
{
	GENERATED_BODY()

public:
	AArenaWaveManager();

	// GameMode 在服务器创建后注入波次数据，并收集带 EnemySpawn Tag 的 TargetPoint。
	void Initialize(UArenaWaveDataAsset* InWaveData);

	// 首次从 Waiting 开始，之后仅允许从 Upgrade 进入下一波。
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Arena|Wave")
	void StartNextWave();

	// 失败阶段停止尚未执行的生成计时器，避免终局后继续增加敌人。
	void StopForDefeat();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void CollectSpawnPoints();
	bool BuildPendingSpawnList(int32 WaveArrayIndex);
	void SpawnNextEnemy();
	void CheckWaveCompletion();
	void UpdateReplicatedEnemyCount();

	UFUNCTION()
	void HandleEnemyDeath(AArenaEnemyCharacter* Enemy);

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Wave")
	FName SpawnPointActorTag = TEXT("EnemySpawn");

	// 升级系统接入前的原型回退：短暂停留 Upgrade 后自动开始下一波；正式选择界面启用时关闭。
	UPROPERTY(EditDefaultsOnly, Category = "Arena|Wave|Prototype")
	bool bAutoStartNextWaveWithoutUpgradeSystem = true;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Wave|Prototype", meta = (EditCondition = "bAutoStartNextWaveWithoutUpgradeSystem", ClampMin = "0.1"))
	float PrototypeUpgradePhaseDuration = 3.0f;

	UPROPERTY(Transient)
	TObjectPtr<UArenaWaveDataAsset> WaveData;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ATargetPoint>> SpawnPoints;

	UPROPERTY(Transient)
	TSet<TObjectPtr<AArenaEnemyCharacter>> AliveEnemies;

	TArray<TSubclassOf<AArenaEnemyCharacter>> PendingEnemyClasses;
	FTimerHandle SpawnTimerHandle;
	FTimerHandle AutoStartNextWaveTimerHandle;
	int32 CurrentWaveArrayIndex = INDEX_NONE;
	int32 NextPendingSpawnIndex = 0;
	int32 NextSpawnPointIndex = 0;
	bool bSpawnFailureInCurrentWave = false;
};
