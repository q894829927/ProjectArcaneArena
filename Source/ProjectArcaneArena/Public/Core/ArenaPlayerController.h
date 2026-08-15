#pragma once

#include "CoreMinimal.h"
#include "Core/ArenaGameState.h"
#include "GameFramework/PlayerController.h"
#include "GAS/ArenaDamageFeedbackTypes.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "ArenaPlayerController.generated.h"

class UArenaInventoryComponent;
class UArenaInventoryWidget;
class UArenaPauseMenuWidget;
class UArenaPlayerHUDWidget;
class UArenaUpgradeSelectionWidget;
class UAbilitySystemComponent;
class AArenaInventoryPickupActor;
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

	// 切换本地鼠标捕获和第三人称准星；仅真实视角切换时按参数把顶视角鼠标归中。
	void SetThirdPersonInputMode(bool bEnableThirdPerson, bool bRecenterTopDownCursor = true);

	// Tab 按复制阶段权限打开或关闭本地背包，并保留当前顶视角或第三人称选择。
	void ToggleInventory();

	// Tab 按下时立即显示背包并记录原开关状态，供轻点切换与长按临时查看共用。
	void HandleInventoryTabPressed();

	// Tab 松开时按按住时长决定保持轻点结果或关闭长按临时背包。
	void HandleInventoryTabReleased();

	// G 在允许操作的阶段选择最近可见 Pickup，再交给服务器重新验证并拾取。
	void RequestInteractWithNearestInventoryPickup();

	// 返回本地背包是否正在占用输入，供 Character 阻止移动、观察和主动技能输入。
	bool IsInventoryOpen() const { return bInventoryInputMode; }

	// 返回本地 ESC 菜单是否正在占用输入；该状态不复制，也不会暂停多人服务器。
	bool IsPauseMenuOpen() const { return bPauseMenuInputMode; }

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

	// 服务器重新验证 Pickup 的距离、视线、保护期和物品数据后执行唯一拾取。
	UFUNCTION(Server, Reliable)
	void ServerInteractWithInventoryPickup(AArenaInventoryPickupActor* PickupActor);

	// 服务器使用稳定 StackId 重新验证并使用一个背包物品。
	UFUNCTION(Server, Reliable)
	void ServerUseInventoryItem(FGuid StackId);

	// 服务器使用稳定 StackId 和合法数量生成世界 Pickup 后扣除堆栈。
	UFUNCTION(Server, Reliable)
	void ServerDropInventoryItem(FGuid StackId, int32 Quantity);

	// 仅在受害者本地 Controller 上把来源方向和强度转成 HUD 表现数据。
	void ShowLocalDamageFeedback(const FArenaDamageFeedbackData& DamageFeedback, float FeedbackIntensity);

protected:
	// 初始化本地输入模式，确保第一次鼠标点击不会被视口捕获吞掉。
	virtual void BeginPlay() override;
	// 在 Controller 输入层绑定 P 和 Escape，使统计与 ESC 菜单不依赖当前 Pawn 或视角。
	virtual void SetupInputComponent() override;
	// 使用平台真实时间汇总本地帧率，并按低频间隔刷新 HUD 与网络 Ping。
	virtual void PlayerTick(float DeltaTime) override;
	virtual void OnRep_PlayerState() override;

	// Pawn 切换后重试 HUD 绑定，兼容未来重生流程。
	virtual void OnPossess(APawn* InPawn) override;
	// 引擎重启 Pawn 清空 IgnoreInput 计数后重建项目 UI 锁，保持本地记账与真实输入状态一致。
	virtual void ResetIgnoreInputFlags() override;
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
	// Escape 在游戏输入模式下切换本地菜单；UIOnly 下由 Widget 把 Escape 转回同一入口。
	void TogglePauseMenu();
	// 创建无需蓝图即可使用的 ESC 菜单，并绑定继续、返回和退出意图。
	void CreatePauseMenuWidget();
	// 切换最高优先级 UIOnly 输入模式，并协调背包、升级、Victory 和双视角状态。
	void SetPauseMenuInputMode(bool bEnabled);
	// 关闭 ESC 菜单后按当前阶段恢复 Upgrade、Victory、Boss 演出或游戏视角输入。
	void RestoreInputModeAfterPauseMenu();

	// 切换左上角本地帧率和网络延迟显示，不复制该偏好或统计值。
	void TogglePerformanceStats();
	// 清空当前采样窗口，避免隐藏期间或关卡切换时间污染下一次 FPS 平均值。
	void ResetPerformanceStatsSample();

	// 正式关卡旅行完成后的首个 Tick 重新把焦点交还游戏视口，避免菜单 Slate 回调覆盖 GameOnly 输入模式。
	void RestoreGameplayInputAfterTravel();

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
	// 切换升级界面的 GameAndUI 输入；转交背包时只释放升级锁，不清除仍在处理的 Tab 手势。
	void SetUpgradeInputMode(bool bEnabled, bool bTransitioningToInventory = false);
	// 创建无需蓝图即可使用的背包 View，并绑定所有 UI 意图委托。
	void CreateInventoryWidget();
	// 绑定当前 PlayerState 的 OwnerOnly InventoryComponent，并立即刷新本地页面。
	void BindInventoryState();
	// 对称解除旧 InventoryComponent 委托并清空本地筛选/分页，避免旅行或重连后残留状态。
	void UnbindInventoryState();
	// 从复制 Model 构建筛选后的二十槽页面及阶段权限快照，View 不直接读取玩法状态。
	void RefreshInventoryUI();
	// 切换 GameAndUI 输入、鼠标和准星，并协调 Upgrade/Victory 的界面输入优先级。
	void SetInventoryInputMode(bool bEnabled);
	// 根据所有本地 UI 与演出模式统一持有一层输入锁；销毁时可强制释放本 Controller 持有的锁。
	void RefreshLocalUIInputLocks(bool bForceRelease = false);
	// 按阶段权限尝试打开背包，成功后统一进入 GameAndUI 输入模式。
	bool TryOpenInventory();
	// 清理本次 Tab 按住快照，供关闭、阶段切换和销毁路径防止迟到松开事件。
	void ResetInventoryTabPressState();
	// 统一检查复制阶段以及 Dead/Stunned，供本地意图发送前做即时反馈。
	bool CanPerformInventoryActions() const;
	// 在基础操作资格上检查共享消耗品冷却，供 Use 按钮和客户端意图即时反馈。
	bool CanUseInventoryItems() const;
	// 本地选择最近且视线可达的 Pickup，服务器仍会执行同一组关键校验。
	AArenaInventoryPickupActor* FindNearestInteractableInventoryPickup() const;
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
	// 继续按钮和 UIOnly Escape 共用同一关闭入口。
	UFUNCTION()
	void HandlePauseMenuResumeRequested();
	// 返回主菜单复用 DirectConnectSubsystem，使 Listen Host 与 Client 正确断开。
	UFUNCTION()
	void HandlePauseMenuReturnToMainMenuRequested();
	// 退出按钮只结束当前本地进程，不发送 gameplay RPC。
	UFUNCTION()
	void HandlePauseMenuQuitGameRequested();

	UFUNCTION()
	void HandleUpgradeChosen(FName UpgradeID);
	// Upgrade Widget 捕获 Tab 按下后，转入统一轻点/长按状态机。
	UFUNCTION()
	void HandleUpgradeInventoryTabPressed();
	// Upgrade Widget 捕获 Tab 松开后，完成轻点切换或长按关闭。
	UFUNCTION()
	void HandleUpgradeInventoryTabReleased();
	// 背包获得焦点后转发 Tab 按下，按键重复不会重复切换。
	UFUNCTION()
	void HandleInventoryTabPressedFromView();
	// 背包获得焦点后转发 Tab 松开，保证 GameAndUI 下能结束长按。
	UFUNCTION()
	void HandleInventoryTabReleasedFromView();
	UFUNCTION()
	void HandleInventoryChanged();
	// Dead、Stunned 或共享冷却 Tag 变化时立即刷新背包按钮资格。
	void HandleInventoryPermissionTagChanged(const FGameplayTag CallbackTag, int32 NewCount);
	UFUNCTION()
	void HandleInventoryStackSelected(FGuid StackId);
	UFUNCTION()
	void HandleInventoryUseRequested(FGuid StackId);
	UFUNCTION()
	void HandleInventoryDropRequested(FGuid StackId, int32 Quantity);
	UFUNCTION()
	void HandleInventoryFilterRequested(FGameplayTag FilterTag, bool bEnabled);
	UFUNCTION()
	void HandleInventoryClearFiltersRequested();
	UFUNCTION()
	void HandleInventoryPreviousPageRequested();
	UFUNCTION()
	void HandleInventoryNextPageRequested();
	UFUNCTION()
	void HandleInventoryCloseRequested();

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Inventory", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UArenaInventoryWidget> InventoryWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UArenaInventoryWidget> InventoryWidget;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Pause Menu", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UArenaPauseMenuWidget> PauseMenuWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UArenaPauseMenuWidget> PauseMenuWidget;

	TWeakObjectPtr<AArenaGameState> BoundArenaGameState;
	TWeakObjectPtr<AArenaPlayerState> BoundUpgradePlayerState;
	TWeakObjectPtr<UArenaInventoryComponent> BoundInventoryComponent;
	TWeakObjectPtr<UAbilitySystemComponent> BoundInventoryAbilitySystemComponent;
	FDelegateHandle InventoryDeadTagDelegateHandle;
	FDelegateHandle InventoryStunnedTagDelegateHandle;
	FDelegateHandle InventoryConsumableCooldownTagDelegateHandle;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Inventory", meta = (ClampMin = "1.0"))
	float InventoryInteractionDistance = 220.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Inventory", meta = (ClampMin = "0.05"))
	float InventoryHoldThreshold = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|UI", meta = (AllowPrivateAccess = "true", ClampMin = "0.01"))
	float PlayerHUDBindingRetryInterval = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|UI|Performance", meta = (AllowPrivateAccess = "true"))
	bool bPerformanceStatsVisibleByDefault = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|UI|Performance", meta = (AllowPrivateAccess = "true", ClampMin = "0.1"))
	float PerformanceStatsUpdateInterval = 0.25f;

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
	FGameplayTagContainer ActiveInventoryFilters;
	FGuid SelectedInventoryStackId;
	int32 InventoryPageIndex = 0;
	double InventoryTabPressStartTime = 0.0;
	double PerformanceStatsSampleStartTime = 0.0;
	int32 PerformanceStatsFrameCount = 0;
	bool bThirdPersonInputMode = false;
	bool bPerformanceStatsVisible = true;
	bool bUpgradeInputMode = false;
	bool bInventoryInputMode = false;
	bool bPauseMenuInputMode = false;
	bool bInventoryTabPressActive = false;
	bool bInventoryWasOpenOnTabPress = false;
	bool bBossIntroInputMode = false;
	bool bBossOutroInputMode = false;
	bool bVictoryInputMode = false;
	bool bBossIntroCameraBlendingOut = false;
	bool bBossIntroSkipHeldLocally = false;
	bool bBossIntroSkipHeldOnServer = false;
	bool bBossOutroCameraBlendingOut = false;
	bool bBossOutroSkipHeldLocally = false;
	bool bBossOutroSkipHeldOnServer = false;
	bool bLocalUIMoveInputLocked = false;
	bool bLocalUILookInputLocked = false;
	mutable bool bWarnedMissingBossIntroCamera = false;
	mutable bool bWarnedMissingBossOutroCamera = false;
};
