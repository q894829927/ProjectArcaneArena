#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "ArenaGameState.generated.h"

class AArenaBossCharacter;

UENUM(BlueprintType)
enum class EArenaGamePhase : uint8
{
	Waiting,
	Combat,
	Upgrade,
	Victory,
	Defeat
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FArenaGamePhaseChangedSignature, EArenaGamePhase, OldPhase, EArenaGamePhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FArenaIntegerStateChangedSignature, int32, OldValue, int32, NewValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FArenaActiveBossChangedSignature, AArenaBossCharacter*, OldBoss, AArenaBossCharacter*, NewBoss);

UCLASS()
class PROJECTARCANEARENA_API AArenaGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AArenaGameState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Arena|Game State")
	EArenaGamePhase GetGamePhase() const { return GamePhase; }

	UFUNCTION(BlueprintPure, Category = "Arena|Game State")
	int32 GetCurrentWaveIndex() const { return CurrentWaveIndex; }

	UFUNCTION(BlueprintPure, Category = "Arena|Game State")
	int32 GetRemainingEnemyCount() const { return RemainingEnemyCount; }

	// 返回服务器为当前对局生成并复制的升级随机种子。
	UFUNCTION(BlueprintPure, Category = "Arena|Game State")
	int32 GetUpgradeRandomSeed() const { return UpgradeRandomSeed; }

	// 返回服务器复制的当前 Boss，空值表示当前没有 Boss 战。
	UFUNCTION(BlueprintPure, Category = "Arena|Game State")
	AArenaBossCharacter* GetActiveBoss() const { return ActiveBoss; }

	// 仅由服务器规则层更新阶段，并通过复制委托驱动客户端表现。
	void SetGamePhase(EArenaGamePhase NewPhase);
	// 仅由服务器波次管理器写入当前波次，索引从 1 开始，0 表示尚未开始。
	void SetCurrentWaveIndex(int32 NewWaveIndex);
	// 仅由服务器写入存活敌人数，客户端 HUD 只观察该复制值。
	void SetRemainingEnemyCount(int32 NewRemainingEnemyCount);
	// 仅由服务器写入本局升级随机种子，客户端不使用该值生成候选。
	void SetUpgradeRandomSeed(int32 NewUpgradeRandomSeed);
	// 仅由服务器波次管理器设置当前 Boss，HUD 通过复制委托观察生命周期。
	void SetActiveBoss(AArenaBossCharacter* NewActiveBoss);

	UPROPERTY(BlueprintAssignable, Category = "Arena|Game State")
	FArenaGamePhaseChangedSignature OnGamePhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Game State")
	FArenaIntegerStateChangedSignature OnCurrentWaveIndexChanged;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Game State")
	FArenaIntegerStateChangedSignature OnRemainingEnemyCountChanged;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Game State")
	FArenaIntegerStateChangedSignature OnUpgradeRandomSeedChanged;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Game State")
	FArenaActiveBossChangedSignature OnActiveBossChanged;

protected:
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_GamePhase, Category = "Arena|Game State")
	EArenaGamePhase GamePhase = EArenaGamePhase::Waiting;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentWaveIndex, Category = "Arena|Game State")
	int32 CurrentWaveIndex = 0;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RemainingEnemyCount, Category = "Arena|Game State")
	int32 RemainingEnemyCount = 0;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_UpgradeRandomSeed, Category = "Arena|Game State")
	int32 UpgradeRandomSeed = 0;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ActiveBoss, Category = "Arena|Game State")
	TObjectPtr<AArenaBossCharacter> ActiveBoss;

	UFUNCTION()
	void OnRep_GamePhase(EArenaGamePhase OldPhase);

	UFUNCTION()
	void OnRep_CurrentWaveIndex(int32 OldWaveIndex);

	UFUNCTION()
	void OnRep_RemainingEnemyCount(int32 OldRemainingEnemyCount);

	UFUNCTION()
	void OnRep_UpgradeRandomSeed(int32 OldUpgradeRandomSeed);

	UFUNCTION()
	void OnRep_ActiveBoss(AArenaBossCharacter* OldActiveBoss);
};
