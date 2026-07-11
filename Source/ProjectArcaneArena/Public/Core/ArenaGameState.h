#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "ArenaGameState.generated.h"

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

	// 仅由服务器规则层更新阶段，并通过复制委托驱动客户端表现。
	void SetGamePhase(EArenaGamePhase NewPhase);
	// 仅由服务器波次管理器写入当前波次，索引从 1 开始，0 表示尚未开始。
	void SetCurrentWaveIndex(int32 NewWaveIndex);
	// 仅由服务器写入存活敌人数，客户端 HUD 只观察该复制值。
	void SetRemainingEnemyCount(int32 NewRemainingEnemyCount);

	UPROPERTY(BlueprintAssignable, Category = "Arena|Game State")
	FArenaGamePhaseChangedSignature OnGamePhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Game State")
	FArenaIntegerStateChangedSignature OnCurrentWaveIndexChanged;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Game State")
	FArenaIntegerStateChangedSignature OnRemainingEnemyCountChanged;

protected:
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_GamePhase, Category = "Arena|Game State")
	EArenaGamePhase GamePhase = EArenaGamePhase::Waiting;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentWaveIndex, Category = "Arena|Game State")
	int32 CurrentWaveIndex = 0;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RemainingEnemyCount, Category = "Arena|Game State")
	int32 RemainingEnemyCount = 0;

	UFUNCTION()
	void OnRep_GamePhase(EArenaGamePhase OldPhase);

	UFUNCTION()
	void OnRep_CurrentWaveIndex(int32 OldWaveIndex);

	UFUNCTION()
	void OnRep_RemainingEnemyCount(int32 OldRemainingEnemyCount);
};
