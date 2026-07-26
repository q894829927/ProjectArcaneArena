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
	Defeat,
	BossIntro,
	BossOutro
};

USTRUCT(BlueprintType)
struct PROJECTARCANEARENA_API FArenaBossIntroTiming
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Boss Intro")
	float EndServerTimeSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Boss Intro")
	float BlendOutDuration = 0.0f;

	// 使用近似比较判断复制时序是否发生有效变化，避免浮点微差重复广播。
	bool operator==(const FArenaBossIntroTiming& Other) const
	{
		return FMath::IsNearlyEqual(EndServerTimeSeconds, Other.EndServerTimeSeconds)
			&& FMath::IsNearlyEqual(BlendOutDuration, Other.BlendOutDuration);
	}

	// 复用相等比较提供服务器 Setter 的幂等更新判断。
	bool operator!=(const FArenaBossIntroTiming& Other) const
	{
		return !(*this == Other);
	}
};

USTRUCT(BlueprintType)
struct PROJECTARCANEARENA_API FArenaBossOutroTiming
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Boss Outro")
	float EndServerTimeSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Boss Outro")
	float BlendOutDuration = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Boss Outro")
	FVector BossDeathLocation = FVector::ZeroVector;

	// 使用近似比较判断复制时序是否发生有效变化，避免浮点微差重复广播。
	bool operator==(const FArenaBossOutroTiming& Other) const
	{
		return FMath::IsNearlyEqual(EndServerTimeSeconds, Other.EndServerTimeSeconds)
			&& FMath::IsNearlyEqual(BlendOutDuration, Other.BlendOutDuration)
			&& BossDeathLocation.Equals(Other.BossDeathLocation);
	}

	// 复用相等比较提供服务器 Setter 的幂等更新判断。
	bool operator!=(const FArenaBossOutroTiming& Other) const
	{
		return !(*this == Other);
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FArenaGamePhaseChangedSignature, EArenaGamePhase, OldPhase, EArenaGamePhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FArenaIntegerStateChangedSignature, int32, OldValue, int32, NewValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FArenaActiveBossChangedSignature, AArenaBossCharacter*, OldBoss, AArenaBossCharacter*, NewBoss);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FArenaBossIntroTimingChangedSignature,
	FArenaBossIntroTiming,
	OldTiming,
	FArenaBossIntroTiming,
	NewTiming);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FArenaBossOutroTimingChangedSignature,
	FArenaBossOutroTiming,
	OldTiming,
	FArenaBossOutroTiming,
	NewTiming);

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

	// 返回服务器复制的 Boss Intro 结束时间与镜头回切时长。
	UFUNCTION(BlueprintPure, Category = "Arena|Boss Intro")
	FArenaBossIntroTiming GetBossIntroTiming() const { return BossIntroTiming; }

	// 使用 GameState 同步服务器时钟计算本地剩余演出时间。
	UFUNCTION(BlueprintPure, Category = "Arena|Boss Intro")
	float GetBossIntroRemainingTime() const;

	// 返回服务器复制的 Boss Outro 结束时间、镜头回切时长和死亡位置。
	UFUNCTION(BlueprintPure, Category = "Arena|Boss Outro")
	FArenaBossOutroTiming GetBossOutroTiming() const { return BossOutroTiming; }

	// 使用 GameState 同步服务器时钟计算本地 Outro 剩余时间。
	UFUNCTION(BlueprintPure, Category = "Arena|Boss Outro")
	float GetBossOutroRemainingTime() const;

	UFUNCTION(BlueprintPure, Category = "Arena|Victory")
	int32 GetVictoryRestartReadyCount() const { return VictoryRestartReadyCount; }

	UFUNCTION(BlueprintPure, Category = "Arena|Victory")
	int32 GetVictoryRestartRequiredCount() const { return VictoryRestartRequiredCount; }

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
	// 仅由服务器写入 Boss Intro 时序，客户端相机与 HUD 共享同一服务器截止时间。
	void SetBossIntroTiming(const FArenaBossIntroTiming& NewTiming);
	// 仅由服务器写入 Boss Outro 时序，客户端镜头和 HUD 共用同一服务器截止时间。
	void SetBossOutroTiming(const FArenaBossOutroTiming& NewTiming);
	// 仅由服务器汇总 Victory 重开确认人数，客户端只负责展示。
	void SetVictoryRestartCounts(int32 NewReadyCount, int32 NewRequiredCount);

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

	UPROPERTY(BlueprintAssignable, Category = "Arena|Boss Intro")
	FArenaBossIntroTimingChangedSignature OnBossIntroTimingChanged;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Boss Outro")
	FArenaBossOutroTimingChangedSignature OnBossOutroTimingChanged;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Victory")
	FArenaIntegerStateChangedSignature OnVictoryRestartReadyCountChanged;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Victory")
	FArenaIntegerStateChangedSignature OnVictoryRestartRequiredCountChanged;

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

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_BossIntroTiming, Category = "Arena|Boss Intro")
	FArenaBossIntroTiming BossIntroTiming;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_BossOutroTiming, Category = "Arena|Boss Outro")
	FArenaBossOutroTiming BossOutroTiming;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_VictoryRestartReadyCount, Category = "Arena|Victory")
	int32 VictoryRestartReadyCount = 0;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_VictoryRestartRequiredCount, Category = "Arena|Victory")
	int32 VictoryRestartRequiredCount = 0;

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

	UFUNCTION()
	void OnRep_BossIntroTiming(FArenaBossIntroTiming OldTiming);

	UFUNCTION()
	void OnRep_BossOutroTiming(FArenaBossOutroTiming OldTiming);

	UFUNCTION()
	void OnRep_VictoryRestartReadyCount(int32 OldReadyCount);

	UFUNCTION()
	void OnRep_VictoryRestartRequiredCount(int32 OldRequiredCount);
};
