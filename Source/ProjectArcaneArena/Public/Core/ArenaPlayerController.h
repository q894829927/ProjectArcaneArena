#pragma once

#include "CoreMinimal.h"
#include "Core/ArenaGameState.h"
#include "GameFramework/PlayerController.h"
#include "GAS/ArenaDamageFeedbackTypes.h"
#include "TimerManager.h"
#include "ArenaPlayerController.generated.h"

class UArenaPlayerHUDWidget;
class UArenaUpgradeSelectionWidget;
class AArenaBossCharacter;
class AArenaGameState;
class AArenaPlayerState;
class ACameraActor;

UCLASS()
class PROJECTARCANEARENA_API AArenaPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AArenaPlayerController();

	// 切换本地鼠标捕获和第三人称准星，不复制任何相机表现状态。
	void SetThirdPersonInputMode(bool bEnableThirdPerson);

	// 本地 Space 输入只提交按住状态，服务器独立计时并验证 BossIntro 跳过资格。
	void SetBossIntroSkipHeld(bool bHeld);
	// 本地 Space 输入提交 Outro 按住状态，服务器独立计时验证跳过资格。
	void SetBossOutroSkipHeld(bool bHeld);

	// 服务器接收按住/松开状态，持续满配置时间后才向 GameMode 请求缩短 Intro。
	UFUNCTION(Server, Reliable)
	void ServerSetBossIntroSkipHeld(bool bHeld);

	UFUNCTION(Server, Reliable)
	void ServerSetBossOutroSkipHeld(bool bHeld);

	// Victory 按钮只提交 Ready/Cancel Ready，最终重开由服务器 GameMode 决定。
	UFUNCTION(Server, Reliable)
	void ServerSetVictoryRestartReady(bool bReady);

	// 客户端只提交候选 ID，服务器 GameMode 会重新验证阶段、候选和堆叠资格。
	UFUNCTION(Server, Reliable)
	void ServerSelectUpgrade(FName UpgradeID);

	// 仅在受害者本地 Controller 上把来源方向和强度转成 HUD 表现数据。
	void ShowLocalDamageFeedback(const FArenaDamageFeedbackData& DamageFeedback, float FeedbackIntensity);

protected:
	// 初始化本地输入模式，确保第一次鼠标点击不会被视口捕获吞掉。
	virtual void BeginPlay() override;
	virtual void OnRep_PlayerState() override;

	// Pawn 切换后重试 HUD 绑定，兼容未来重生流程。
	virtual void OnPossess(APawn* InPawn) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Intro 本地镜头开始事件仅供蓝图扩展动画或音效，不拥有玩法阶段。
	UFUNCTION(BlueprintImplementableEvent, Category = "Arena|Boss Intro")
	void K2_OnBossIntroStarted(AArenaBossCharacter* Boss, float IntroDuration);

	// Intro 本地清理完成事件供蓝图关闭附加表现，原生逻辑始终负责恢复镜头与输入。
	UFUNCTION(BlueprintImplementableEvent, Category = "Arena|Boss Intro")
	void K2_OnBossIntroEnded(bool bWasInterrupted);

	UFUNCTION(BlueprintImplementableEvent, Category = "Arena|Boss Outro")
	void K2_OnBossOutroStarted(AArenaBossCharacter* Boss, float OutroDuration);

	UFUNCTION(BlueprintImplementableEvent, Category = "Arena|Boss Outro")
	void K2_OnBossOutroEnded(bool bWasInterrupted);

private:
	// 创建本地玩家 HUD，Dedicated Server 和非本地 Controller 不创建 UI。
	void CreatePlayerHUD();

	// 从 PlayerState 获取 ASC/AttributeSet 并绑定到 HUD，未就绪时短时间重试。
	void TryBindPlayerHUD();
	// 本地端绑定 HUD、Boss 演出与 Victory 数据，Authority 端同时监听阶段以清理服务器 Hold Timer。
	void BindGameStateHUD();
	void UnbindGameStateHUD();
	void CreateUpgradeSelectionWidget();
	void BindUpgradeState();
	void UnbindUpgradeState();
	void RefreshUpgradeSelectionUI();
	void SetUpgradeInputMode(bool bEnabled);
	// 根据复制阶段、ActiveBoss 和时序快照开始、更新或结束本地 Intro 表现。
	void RefreshBossIntroPresentation();
	// 启动本地相机切换、输入冻结和 HUD 倒计时，不修改任何权威玩法状态。
	void StartBossIntroPresentation(AArenaBossCharacter* Boss, const FArenaBossIntroTiming& Timing);
	// 每个本地刷新周期使用服务器同步时间更新倒计时、跳过进度和镜头回切窗口。
	void UpdateBossIntroPresentation();
	// 提前进入尾段时把 ViewTarget 平滑切回当前 Pawn，并保持控制冻结直到 Combat。
	void BeginBossIntroCameraBlendOut(float BlendOutDuration);
	// 所有正常与异常退出路径恢复原视角输入、HUD 和本地 ViewTarget。
	void FinishBossIntroPresentation(bool bWasInterrupted);
	// 选择可选的关卡 BossIntroCamera 覆盖；动态镜头由启动流程优先创建。
	ACameraActor* FindBossIntroCamera(const AArenaBossCharacter* Boss) const;
	// 根据 Boss 碰撞包围盒计算偏向下半身的注视高度，把全身抬离底部 HUD 遮挡区域。
	float CalculateBossIntroLookAtHeight(const AArenaBossCharacter* Boss) const;
	// 服务器 Hold Timer 完成后把请求交给 GameMode 重新验证，成功与否都清理本次按住状态。
	void CompleteBossIntroSkipHold();
	// 松开、阶段结束或 Controller 销毁时对称清理服务器 Hold Timer。
	void ClearBossIntroSkipHold();
	// 根据复制阶段与 Outro Timing 启动、更新或结束本地死亡演出。
	void RefreshBossOutroPresentation();
	void StartBossOutroPresentation(AArenaBossCharacter* Boss, const FArenaBossOutroTiming& Timing);
	void UpdateBossOutroPresentation();
	void BeginBossOutroCameraBlendOut(float BlendOutDuration);
	void FinishBossOutroPresentation(bool bWasInterrupted);
	// 选择可选的关卡 BossVictoryCamera 覆盖；动态镜头由启动流程优先创建。
	ACameraActor* FindBossOutroCamera(const FVector& BossDeathLocation) const;
	// 为 Boss Intro/Outro 复用同一套本地动态构图与多方向球形避障逻辑。
	ACameraActor* CreateDynamicBossPresentationCamera(
		const AArenaBossCharacter* Boss,
		const FVector& FocusLocation,
		const FVector& PreferredCameraDirection,
		float CameraDistance,
		float CameraHeight,
		float LookAtHeight,
		float FieldOfView,
		float CollisionRadius,
		float MinimumDistance);
	// 回切完成后延迟销毁指定的本地临时镜头，避免 ViewTarget Blend 途中失去相机。
	void ReleaseDynamicBossPresentationCamera(
		TWeakObjectPtr<ACameraActor>& DynamicCamera,
		float DelaySeconds);
	void CompleteBossOutroSkipHold();
	void ClearBossOutroSkipHold();
	// 根据 Victory 复制状态更新 UIOnly 输入与 Ready 面板。
	void RefreshVictoryPresentation();
	void SetVictoryInputMode(bool bEnabled);

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
	// Intro 截止时间被自然设置或跳过缩短时立即刷新本地镜头与 HUD。
	UFUNCTION()
	void HandleBossIntroTimingChanged(FArenaBossIntroTiming OldTiming, FArenaBossIntroTiming NewTiming);
	UFUNCTION()
	void HandleBossOutroTimingChanged(FArenaBossOutroTiming OldTiming, FArenaBossOutroTiming NewTiming);
	UFUNCTION()
	void HandleVictoryRestartCountChanged(int32 OldValue, int32 NewValue);

	UFUNCTION()
	void HandleUpgradeStateChanged();
	UFUNCTION()
	void HandleVictoryRestartReadyChanged(bool bIsReady);
	UFUNCTION()
	void HandleVictoryRestartRequested();

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

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Boss Intro")
	FName BossIntroCameraActorTag = TEXT("BossIntroCamera");

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Boss Intro")
	bool bUsePlacedBossIntroCameraOverride = false;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Boss Intro", meta = (ClampMin = "0.0"))
	float BossIntroCameraBlendDuration = 0.6f;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Boss Intro", meta = (ClampMin = "0.1"))
	float BossIntroSkipHoldDuration = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Boss Intro", meta = (ClampMin = "0.01"))
	float BossIntroPresentationTickInterval = 0.05f;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Boss Intro|Dynamic Camera", meta = (ClampMin = "100.0"))
	float BossIntroDynamicCameraDistance = 900.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Boss Intro|Dynamic Camera", meta = (ClampMin = "0.0"))
	float BossIntroDynamicCameraHeight = 300.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Boss Intro|Dynamic Camera", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BossIntroDynamicCameraVerticalFramingBias = 0.65f;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Boss Intro|Dynamic Camera")
	float BossIntroDynamicCameraLookAtHeightOffset = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Boss Intro|Dynamic Camera", meta = (ClampMin = "5.0", ClampMax = "170.0"))
	float BossIntroDynamicCameraFieldOfView = 45.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Boss Intro|Dynamic Camera", meta = (ClampMin = "0.0"))
	float BossIntroDynamicCameraCollisionRadius = 24.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Boss Intro|Dynamic Camera", meta = (ClampMin = "0.0"))
	float BossIntroDynamicCameraMinimumDistance = 300.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Boss Outro")
	FName BossVictoryCameraActorTag = TEXT("BossVictoryCamera");

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Boss Outro")
	bool bUsePlacedBossVictoryCameraOverride = false;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Boss Outro", meta = (ClampMin = "0.0"))
	float BossOutroCameraBlendDuration = 0.6f;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Boss Outro", meta = (ClampMin = "0.1"))
	float BossOutroSkipHoldDuration = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Boss Outro", meta = (ClampMin = "0.01"))
	float BossOutroPresentationTickInterval = 0.05f;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Boss Outro|Dynamic Camera", meta = (ClampMin = "100.0"))
	float BossOutroDynamicCameraDistance = 800.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Boss Outro|Dynamic Camera", meta = (ClampMin = "0.0"))
	float BossOutroDynamicCameraHeight = 280.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Boss Outro|Dynamic Camera")
	float BossOutroDynamicCameraLookAtHeight = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Boss Outro|Dynamic Camera", meta = (ClampMin = "5.0", ClampMax = "170.0"))
	float BossOutroDynamicCameraFieldOfView = 45.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Boss Outro|Dynamic Camera", meta = (ClampMin = "0.0"))
	float BossOutroDynamicCameraCollisionRadius = 24.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Boss Outro|Dynamic Camera", meta = (ClampMin = "0.0"))
	float BossOutroDynamicCameraMinimumDistance = 300.0f;

	FTimerHandle PlayerHUDBindingRetryTimerHandle;
	FTimerHandle BossIntroPresentationTimerHandle;
	FTimerHandle BossIntroSkipHoldTimerHandle;
	FTimerHandle BossOutroPresentationTimerHandle;
	FTimerHandle BossOutroSkipHoldTimerHandle;
	TWeakObjectPtr<ACameraActor> ActiveBossIntroCamera;
	TWeakObjectPtr<ACameraActor> ActiveBossOutroCamera;
	TWeakObjectPtr<ACameraActor> DynamicBossIntroCamera;
	TWeakObjectPtr<ACameraActor> DynamicBossOutroCamera;
	float LocalBossIntroSkipHoldStartTime = 0.0f;
	float LocalBossOutroSkipHoldStartTime = 0.0f;
	bool bThirdPersonInputMode = false;
	bool bUpgradeInputMode = false;
	bool bBossIntroInputMode = false;
	bool bBossOutroInputMode = false;
	bool bVictoryInputMode = false;
	bool bBossIntroCameraBlendingOut = false;
	bool bBossIntroSkipHeldLocally = false;
	bool bBossIntroSkipHeldOnServer = false;
	bool bBossOutroCameraBlendingOut = false;
	bool bBossOutroSkipHeldLocally = false;
	bool bBossOutroSkipHeldOnServer = false;
	mutable bool bWarnedMissingBossIntroCamera = false;
	mutable bool bWarnedMissingBossOutroCamera = false;
};
