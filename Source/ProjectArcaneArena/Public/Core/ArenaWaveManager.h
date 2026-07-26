#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "ArenaWaveManager.generated.h"

class AArenaEnemyCharacter;
class AArenaBossCharacter;
class AArenaPlayerController;
class AArenaPickupActor;
class ATargetPoint;
class UArenaPickupDropTableDataAsset;
class UArenaWaveDataAsset;

DECLARE_MULTICAST_DELEGATE(FArenaUpgradePhaseStartedSignature);

UCLASS()
class PROJECTARCANEARENA_API AArenaWaveManager : public AActor
{
	GENERATED_BODY()

public:
	AArenaWaveManager();

	// GameMode 注入波次、全局掉落表和本局种子，并初始化独立的服务器掉落随机流。
	void Initialize(
		UArenaWaveDataAsset* InWaveData,
		UArenaPickupDropTableDataAsset* InPickupDropTable,
		int32 InMatchRandomSeed);

	// 首次从 Waiting 开始，之后仅允许从 Upgrade 进入下一波。
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Arena|Wave")
	void StartNextWave();

	// 失败阶段停止尚未执行的生成计时器，避免终局后继续增加敌人。
	void StopForDefeat();

	// 正式升级系统接管后关闭原型自动跳过，并由 GameMode 监听升级阶段入口。
	void SetUpgradeSystemEnabled(bool bEnabled);

	// 服务器验证参战 Controller 后缩短 Boss Intro，并保留统一的镜头回切窗口。
	bool RequestBossIntroSkip(AArenaPlayerController* RequestingController);

	FArenaUpgradePhaseStartedSignature OnUpgradePhaseStarted;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void CollectSpawnPoints();
	// 验证普通波基础数据及 Boss 波唯一 Boss 约束，错误配置不会进入 Combat。
	bool ValidateWaveConfiguration(int32 WaveArrayIndex) const;
	bool BuildPendingSpawnList(int32 WaveArrayIndex);
	void SpawnNextEnemy();
	// Boss 生成、缩放和 ActiveBoss 发布完成后启动服务器 Intro 时钟。
	void BeginBossIntro();
	// Intro 正常结束或跳过回切完成后进入 Combat，并启动 Boss 阶段与 AI。
	void FinishBossIntro();
	// 终局、销毁和 Boss 死亡路径统一清理 Intro Timer。
	void ClearBossIntroTimer();
	// 统计 Boss 生成瞬间所有拥有有效 ASC 的 ArenaPlayerState，死亡或暂时无 Pawn 的玩家仍计入。
	int32 GetBossScalingPlayerCount() const;
	// 为当前死亡敌人执行一次服务器掉落抽取，失败不会影响波次推进。
	void TrySpawnPickupDrop(const AArenaEnemyCharacter* Enemy);
	// 按掉落表有效正权重抽取一个 Pickup Class。
	TSubclassOf<AArenaPickupActor> DrawWeightedPickupClass();
	void CheckWaveCompletion();
	void UpdateReplicatedEnemyCount();

	// 唯一处理受管理敌人的死亡计数、掉落抽取和波次完成检查。
	UFUNCTION()
	void HandleEnemyDeath(AArenaEnemyCharacter* Enemy);
	// 直接 Destroy 未触发死亡委托时仍清理 Intro、ActiveBoss 和波次计数，但不生成普通掉落。
	UFUNCTION()
	void HandleEnemyDestroyed(AActor* DestroyedActor);

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Wave")
	FName SpawnPointActorTag = TEXT("EnemySpawn");

	// 升级系统接入前的原型回退：短暂停留 Upgrade 后自动开始下一波；正式选择界面启用时关闭。
	UPROPERTY(EditDefaultsOnly, Category = "Arena|Wave|Prototype")
	bool bAutoStartNextWaveWithoutUpgradeSystem = true;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Wave|Prototype", meta = (EditCondition = "bAutoStartNextWaveWithoutUpgradeSystem", ClampMin = "0.1"))
	float PrototypeUpgradePhaseDuration = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Boss Intro", meta = (ClampMin = "0.1"))
	float BossIntroDuration = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Boss Intro", meta = (ClampMin = "0.0"))
	float BossIntroBlendDuration = 0.6f;

	UPROPERTY(Transient)
	TObjectPtr<UArenaWaveDataAsset> WaveData;

	UPROPERTY(Transient)
	TObjectPtr<UArenaPickupDropTableDataAsset> PickupDropTable;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ATargetPoint>> SpawnPoints;

	UPROPERTY(Transient)
	TSet<TObjectPtr<AArenaEnemyCharacter>> AliveEnemies;

	TArray<TSubclassOf<AArenaEnemyCharacter>> PendingEnemyClasses;
	FTimerHandle SpawnTimerHandle;
	FTimerHandle AutoStartNextWaveTimerHandle;
	FTimerHandle BossIntroTimerHandle;
	int32 CurrentWaveArrayIndex = INDEX_NONE;
	int32 NextPendingSpawnIndex = 0;
	int32 NextSpawnPointIndex = 0;
	FRandomStream PickupRandomStream;
	bool bSpawnFailureInCurrentWave = false;
	bool bUpgradeSystemEnabled = false;
	bool bCurrentWaveIsBossWave = false;
};
