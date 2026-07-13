#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"
#include "ArenaGameMode.generated.h"

class AArenaPlayerController;
class AArenaPlayerState;
class AArenaWaveManager;
class UArenaUpgradeDataAsset;
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

	// 接收 Controller 的选择请求，全部规则由服务器重新验证后才应用升级。
	void SubmitUpgradeSelection(AArenaPlayerController* RequestingController, FName UpgradeID);

protected:
	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

private:
	// 由服务器为本局生成一次随机种子，并同步到 GameState 供所有客户端观察。
	void InitializeUpgradeRandomStream();
	void HandleUpgradePhaseStarted();
	void PrepareUpgradeChoicesForPlayer(AArenaPlayerState* ArenaPlayerState);
	bool IsUpgradeEligible(const AArenaPlayerState* ArenaPlayerState, const UArenaUpgradeDataAsset* Upgrade) const;
	bool ApplyUpgrade(AArenaPlayerState* ArenaPlayerState, const UArenaUpgradeDataAsset* Upgrade) const;
	bool HaveAllPlayersCompletedUpgradeSelection() const;
	void TryAdvanceAfterUpgradeSelections();

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Wave")
	TSubclassOf<AArenaWaveManager> WaveManagerClass;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Wave")
	TObjectPtr<UArenaWaveDataAsset> WaveData;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Wave", meta = (ClampMin = "0.0"))
	float InitialWaveDelay = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Upgrade")
	TArray<TObjectPtr<UArenaUpgradeDataAsset>> UpgradePool;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Upgrade", meta = (ClampMin = "1", ClampMax = "3"))
	int32 UpgradeChoiceCount = 3;

	UPROPERTY(Transient)
	TObjectPtr<AArenaWaveManager> WaveManager;

	FTimerHandle InitialWaveTimerHandle;
	int32 UpgradeRandomSeed = 0;
	FRandomStream UpgradeRandomStream;
};
