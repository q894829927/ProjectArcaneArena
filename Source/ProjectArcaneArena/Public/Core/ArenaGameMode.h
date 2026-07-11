#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"
#include "ArenaGameMode.generated.h"

class AArenaWaveManager;
class UArenaWaveDataAsset;

UCLASS()
class PROJECTARCANEARENA_API AArenaGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AArenaGameMode();

	// 玩家死亡后由服务器检查所有参战玩家，仅在全员死亡时进入失败阶段。
	void NotifyPlayerDeath();

	UFUNCTION(BlueprintCallable, Category = "Arena|Wave")
	void StartNextWave();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Arena|Wave")
	TSubclassOf<AArenaWaveManager> WaveManagerClass;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Wave")
	TObjectPtr<UArenaWaveDataAsset> WaveData;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Wave", meta = (ClampMin = "0.0"))
	float InitialWaveDelay = 1.0f;

	UPROPERTY(Transient)
	TObjectPtr<AArenaWaveManager> WaveManager;

	FTimerHandle InitialWaveTimerHandle;
};
