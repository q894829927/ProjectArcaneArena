#pragma once

#include "CoreMinimal.h"
#include "Core/ArenaGameState.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "ArenaPlayerController.generated.h"

class UArenaPlayerHUDWidget;
class UArenaUpgradeSelectionWidget;
class AArenaBossCharacter;
class AArenaGameState;
class AArenaPlayerState;

UCLASS()
class PROJECTARCANEARENA_API AArenaPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AArenaPlayerController();

	// 切换本地鼠标捕获和第三人称准星，不复制任何相机表现状态。
	void SetThirdPersonInputMode(bool bEnableThirdPerson);

	// 客户端只提交候选 ID，服务器 GameMode 会重新验证阶段、候选和堆叠资格。
	UFUNCTION(Server, Reliable)
	void ServerSelectUpgrade(FName UpgradeID);

protected:
	// 初始化本地输入模式，确保第一次鼠标点击不会被视口捕获吞掉。
	virtual void BeginPlay() override;
	virtual void OnRep_PlayerState() override;

	// Pawn 切换后重试 HUD 绑定，兼容未来重生流程。
	virtual void OnPossess(APawn* InPawn) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// 创建本地玩家 HUD，Dedicated Server 和非本地 Controller 不创建 UI。
	void CreatePlayerHUD();

	// 从 PlayerState 获取 ASC/AttributeSet 并绑定到 HUD，未就绪时短时间重试。
	void TryBindPlayerHUD();
	// 绑定复制 GameState 委托，HUD 只观察阶段与波次数据。
	void BindGameStateHUD();
	void UnbindGameStateHUD();
	void CreateUpgradeSelectionWidget();
	void BindUpgradeState();
	void UnbindUpgradeState();
	void RefreshUpgradeSelectionUI();
	void SetUpgradeInputMode(bool bEnabled);

	UFUNCTION()
	void HandleGamePhaseChanged(EArenaGamePhase OldPhase, EArenaGamePhase NewPhase);
	UFUNCTION()
	void HandleWaveIndexChanged(int32 OldValue, int32 NewValue);
	UFUNCTION()
	void HandleRemainingEnemyCountChanged(int32 OldValue, int32 NewValue);
	UFUNCTION()
	void HandleUpgradeRandomSeedChanged(int32 OldValue, int32 NewValue);
	// ActiveBoss 复制变化时只重绑本地 HUD，不影响 Boss 玩法生命周期。
	UFUNCTION()
	void HandleActiveBossChanged(AArenaBossCharacter* OldBoss, AArenaBossCharacter* NewBoss);

	UFUNCTION()
	void HandleUpgradeStateChanged();

	UFUNCTION()
	void HandleUpgradeChosen(FName UpgradeID);

	// PlayerState 或 ASC 复制到客户端可能晚于 BeginPlay，需要延迟重试。
	void SchedulePlayerHUDBindingRetry();

	// 绑定成功后停止重试，避免无意义定时器常驻。
	void ClearPlayerHUDBindingRetry();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UArenaPlayerHUDWidget> PlayerHUDWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UArenaPlayerHUDWidget> PlayerHUDWidget;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Upgrade", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UArenaUpgradeSelectionWidget> UpgradeSelectionWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UArenaUpgradeSelectionWidget> UpgradeSelectionWidget;

	TWeakObjectPtr<AArenaGameState> BoundArenaGameState;
	TWeakObjectPtr<AArenaPlayerState> BoundUpgradePlayerState;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|UI", meta = (AllowPrivateAccess = "true", ClampMin = "0.01"))
	float PlayerHUDBindingRetryInterval = 0.1f;

	FTimerHandle PlayerHUDBindingRetryTimerHandle;
	bool bThirdPersonInputMode = false;
	bool bUpgradeInputMode = false;
};
