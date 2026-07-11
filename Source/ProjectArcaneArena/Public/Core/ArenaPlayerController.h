#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "ArenaPlayerController.generated.h"

class UArenaPlayerHUDWidget;

UCLASS()
class PROJECTARCANEARENA_API AArenaPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AArenaPlayerController();

	// 切换本地鼠标捕获和第三人称准星，不复制任何相机表现状态。
	void SetThirdPersonInputMode(bool bEnableThirdPerson);

protected:
	// 初始化本地输入模式，确保第一次鼠标点击不会被视口捕获吞掉。
	virtual void BeginPlay() override;

	// Pawn 切换后重试 HUD 绑定，兼容未来重生流程。
	virtual void OnPossess(APawn* InPawn) override;

private:
	// 创建本地玩家 HUD，Dedicated Server 和非本地 Controller 不创建 UI。
	void CreatePlayerHUD();

	// 从 PlayerState 获取 ASC/AttributeSet 并绑定到 HUD，未就绪时短时间重试。
	void TryBindPlayerHUD();

	// PlayerState 或 ASC 复制到客户端可能晚于 BeginPlay，需要延迟重试。
	void SchedulePlayerHUDBindingRetry();

	// 绑定成功后停止重试，避免无意义定时器常驻。
	void ClearPlayerHUDBindingRetry();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UArenaPlayerHUDWidget> PlayerHUDWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UArenaPlayerHUDWidget> PlayerHUDWidget;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|UI", meta = (AllowPrivateAccess = "true", ClampMin = "0.01"))
	float PlayerHUDBindingRetryInterval = 0.1f;

	FTimerHandle PlayerHUDBindingRetryTimerHandle;
	bool bThirdPersonInputMode = false;
};
