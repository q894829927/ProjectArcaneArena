#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"
#include "ArenaGameMode.generated.h"

class AArenaPlayerController;
class AArenaPlayerState;
class AArenaWaveManager;
class UArenaPickupDropTableDataAsset;
class UArenaUpgradeDataAsset;
class UArenaWaveDataAsset;

USTRUCT(BlueprintType)
struct PROJECTARCANEARENA_API FArenaUpgradeRarityWeights
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Upgrade|Rarity", meta = (ClampMin = "1"))
	int32 Common = 100;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Upgrade|Rarity", meta = (ClampMin = "1"))
	int32 Rare = 40;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Upgrade|Rarity", meta = (ClampMin = "1"))
	int32 Epic = 15;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|Upgrade|Rarity", meta = (ClampMin = "1"))
	int32 Legendary = 5;
};

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
	// 玩家完成 Pawn 创建和 GAS 初始化后，编辑器测试模式可按顺序授予起始升级。
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	// Upgrade 阶段迟加入时生成候选，并在空候选自动完成后重新检查全员门槛。
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

private:
	// 由服务器使用可选固定种子或会话随机种子初始化随机流，并同步实际种子供客户端观察。
	void InitializeUpgradeRandomStream();
	// 为所有有效玩家生成本轮独立候选，并在全员无奖励可选时继续检查推进条件。
	void HandleUpgradePhaseStarted();
	// 过滤并抽取候选；空候选无奖励完成并恢复资源，非空候选进入等待选择状态。
	void PrepareUpgradeChoicesForPlayer(AArenaPlayerState* ArenaPlayerState);
	// 从候选数组按稀有度权重抽取一个索引，所有随机数只来自服务器升级随机流。
	int32 DrawWeightedUpgradeIndex(const TArray<UArenaUpgradeDataAsset*>& Candidates);
	// 返回升级资产对应的可配置稀有度权重，并防止无效配置产生零权重池。
	int32 GetUpgradeRarityWeight(const UArenaUpgradeDataAsset* Upgrade) const;
	// 判断候选是否精确匹配玩家当前拥有的火焰、闪电或暴击构筑标签。
	bool IsUpgradeForOwnedBuild(const AArenaPlayerState* ArenaPlayerState, const UArenaUpgradeDataAsset* Upgrade) const;
	// 使用同一服务器随机流打乱最终槽位，避免构筑保底固定出现在首位。
	void ShuffleUpgradeChoices(TArray<UArenaUpgradeDataAsset*>& Choices);
	// 按唯一 ID、资格标签和堆叠上限重新验证候选当前是否仍可选择。
	bool IsUpgradeEligible(const AArenaPlayerState* ArenaPlayerState, const UArenaUpgradeDataAsset* Upgrade) const;
	// 在服务器授予升级 GE/Ability/标签，向 GE 注入 NumericValue，并保存 Ability SourceObject。
	bool ApplyUpgrade(AArenaPlayerState* ArenaPlayerState, const UArenaUpgradeDataAsset* Upgrade) const;
	// 在服务器完成有奖励或无奖励选择后通过 GAS 补满生命和能量，Health 恢复会驱动死亡玩家复活。
	void RestorePlayerResourcesAfterUpgrade(AArenaPlayerState* ArenaPlayerState) const;
#if WITH_EDITOR
	// 编辑器测试模式通过正式升级路径顺序授予 DataAsset，保留标签、层数和 Ability SourceObject。
	void ApplyDebugStartingUpgrades(AArenaPlayerState* ArenaPlayerState) const;
#endif
	bool HaveAllPlayersCompletedUpgradeSelection() const;
	void TryAdvanceAfterUpgradeSelections();

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Wave")
	TSubclassOf<AArenaWaveManager> WaveManagerClass;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Wave")
	TObjectPtr<UArenaWaveDataAsset> WaveData;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Pickup")
	TObjectPtr<UArenaPickupDropTableDataAsset> PickupDropTable;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Wave", meta = (ClampMin = "0.0"))
	float InitialWaveDelay = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Upgrade")
	TArray<TObjectPtr<UArenaUpgradeDataAsset>> UpgradePool;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Upgrade", meta = (ClampMin = "1", ClampMax = "3"))
	int32 UpgradeChoiceCount = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Upgrade|Rarity", meta = (AllowPrivateAccess = "true"))
	FArenaUpgradeRarityWeights UpgradeRarityWeights;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Upgrade|Random", meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	int32 UpgradeRandomSeedOverride = 0;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditDefaultsOnly, Category = "Arena|Debug|Upgrade")
	bool bEnableDebugStartingUpgrades = false;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Debug|Upgrade", meta = (EditCondition = "bEnableDebugStartingUpgrades"))
	TArray<TObjectPtr<UArenaUpgradeDataAsset>> DebugStartingUpgrades;
#endif

	UPROPERTY(Transient)
	TObjectPtr<AArenaWaveManager> WaveManager;

	FTimerHandle InitialWaveTimerHandle;
	int32 UpgradeRandomSeed = 0;
	FRandomStream UpgradeRandomStream;
};
