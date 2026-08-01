#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ArenaDirectConnectSubsystem.generated.h"

UENUM(BlueprintType)
enum class EArenaDirectConnectState : uint8
{
	Idle,
	Hosting,
	Joining,
	InLobby,
	Traveling,
	Failed
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FArenaDirectConnectStateChangedSignature,
	EArenaDirectConnectState,
	OldState,
	EArenaDirectConnectState,
	NewState);

UCLASS()
class PROJECTARCANEARENA_API UArenaDirectConnectSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// 绑定引擎网络和旅行失败回调，使错误能跨关卡返回主菜单展示。
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// 对称解除引擎委托，避免 GameInstance 销毁后收到迟到连接失败通知。
	virtual void Deinitialize() override;

	// 在本地打开带 Listen 参数的菜单 Lobby，容量限制为二至四人。
	UFUNCTION(BlueprintCallable, Category = "Arena|Network")
	bool HostLobby(int32 MaxPlayers);

	// 校验并规范化 IPv4 或 localhost 地址，再让本地 Controller 进行绝对 ClientTravel。
	UFUNCTION(BlueprintCallable, Category = "Arena|Network")
	bool JoinLobby(const FString& Address);

	// 主动退出当前 Listen/Client 会话并返回无网络的主菜单。
	UFUNCTION(BlueprintCallable, Category = "Arena|Network")
	void LeaveNetworkGame();

	// 返回当前仅属于本地 GameInstance 的连接状态。
	UFUNCTION(BlueprintPure, Category = "Arena|Network")
	EArenaDirectConnectState GetConnectionState() const { return ConnectionState; }

	// 读取并清空待展示错误，保证同一次失败只在新主菜单显示一次。
	UFUNCTION(BlueprintCallable, Category = "Arena|Network")
	FText ConsumePendingError();

	// 菜单 Controller 确认复制 Lobby 已就绪后推进本地连接状态。
	void NotifyEnteredLobby();

	// Lobby 开始 Seamless Travel 时标记本地正在进入正式战斗。
	void NotifyMatchTravelStarting();

	// Controller 收到 Host 主动关闭通知时保存可读错误并返回菜单。
	void HandleHostClosedLobby(const FText& Reason);

	UPROPERTY(BlueprintAssignable, Category = "Arena|Network")
	FArenaDirectConnectStateChangedSignature OnConnectionStateChanged;

private:
	// 严格解析 IPv4[:Port] 或 localhost[:Port]，未写端口时补充默认 7777。
	bool NormalizeJoinAddress(const FString& Address, FString& OutNormalizedAddress, FText& OutError) const;

	// 更新连接状态并仅在状态真实变化时广播给本地菜单。
	void SetConnectionState(EArenaDirectConnectState NewState);

	// 保存失败原因并安排下一 Tick 返回菜单，避免在引擎失败委托栈内同步旅行。
	void FailAndReturnToMenu(const FText& ErrorMessage);

	// 执行延迟的无网络主菜单旅行，并防止同一失败重复排队。
	void ReturnToMainMenuAfterFailure();

	// 将引擎 NetworkFailure 转换为简短本地错误并回到主菜单。
	void HandleNetworkFailure(
		UWorld* World,
		class UNetDriver* NetDriver,
		ENetworkFailure::Type FailureType,
		const FString& ErrorString);

	// 从服务器拒绝文本中保留“房间已满”和“比赛已开始”等可操作原因，其余错误使用稳定通用提示。
	FText BuildNetworkFailureMessage(const FString& ErrorString) const;

	// 将引擎 TravelFailure 转换为简短本地错误并回到主菜单。
	void HandleTravelFailure(
		UWorld* World,
		ETravelFailure::Type FailureType,
		const FString& ErrorString);

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Network")
	FName MainMenuMapName = TEXT("/Game/UI/MainMenu/Lvl_MainMenu");

	UPROPERTY(Transient)
	EArenaDirectConnectState ConnectionState = EArenaDirectConnectState::Idle;

	FText PendingError;
	FDelegateHandle NetworkFailureDelegateHandle;
	FDelegateHandle TravelFailureDelegateHandle;
	bool bReturnToMenuQueued = false;
};
