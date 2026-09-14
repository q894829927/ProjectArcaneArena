#include "Core/ArenaDirectConnectSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogArenaDirectConnect, Log, All);

namespace ArenaDirectConnect
{
	constexpr int32 DefaultPort = 7777;
	constexpr int32 MinimumLobbyPlayers = 2;
	constexpr int32 MaximumLobbyPlayers = 4;
}

// 绑定全局引擎失败委托，连接状态由 GameInstanceSubsystem 跨地图持续保存。
void UArenaDirectConnectSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (GEngine)
	{
		NetworkFailureDelegateHandle = GEngine->OnNetworkFailure().AddUObject(
			this,
			&UArenaDirectConnectSubsystem::HandleNetworkFailure);
		TravelFailureDelegateHandle = GEngine->OnTravelFailure().AddUObject(
			this,
			&UArenaDirectConnectSubsystem::HandleTravelFailure);
	}
}

// GameInstance 结束时解除全局委托并清空排队标记，防止旧会话污染下一次启动。
void UArenaDirectConnectSubsystem::Deinitialize()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureDelegateHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureDelegateHandle);
	}

	NetworkFailureDelegateHandle.Reset();
	TravelFailureDelegateHandle.Reset();
	bReturnToMenuQueued = false;
	Super::Deinitialize();
}

// 验证容量后使用同一主菜单地图建立 Listen Lobby，Lobby 玩法状态稍后由菜单 GameMode 初始化。
bool UArenaDirectConnectSubsystem::HostLobby(const int32 MaxPlayers)
{
	if (ConnectionState != EArenaDirectConnectState::Idle
		&& ConnectionState != EArenaDirectConnectState::Failed)
	{
		return false;
	}

	if (MaxPlayers < ArenaDirectConnect::MinimumLobbyPlayers
		|| MaxPlayers > ArenaDirectConnect::MaximumLobbyPlayers)
	{
		PendingError = NSLOCTEXT("ArenaNetwork", "InvalidLobbyCapacity", "房间人数必须在 2 到 4 之间。");
		SetConnectionState(EArenaDirectConnectState::Failed);
		return false;
	}

	UWorld* World = GetWorld();
	if (!World || MainMenuMapName.IsNone())
	{
		FailAndReturnToMenu(NSLOCTEXT("ArenaNetwork", "MissingMenuWorld", "无法创建房间：主菜单地图不可用。"));
		return false;
	}

	PendingError = FText::GetEmpty();
	SetConnectionState(EArenaDirectConnectState::Hosting);
	const FString Options = FString::Printf(TEXT("listen?ArenaLobby=1?MaxPlayers=%d"), MaxPlayers);
	UGameplayStatics::OpenLevel(this, MainMenuMapName, true, Options);
	return true;
}

// 地址验证成功后由首个本地 Controller 连接 Host；连接结果通过全局失败委托或 Lobby 复制确认。
bool UArenaDirectConnectSubsystem::JoinLobby(const FString& Address)
{
	if (ConnectionState != EArenaDirectConnectState::Idle
		&& ConnectionState != EArenaDirectConnectState::Failed)
	{
		return false;
	}

	FString NormalizedAddress;
	FText ValidationError;
	if (!NormalizeJoinAddress(Address, NormalizedAddress, ValidationError))
	{
		PendingError = ValidationError;
		SetConnectionState(EArenaDirectConnectState::Failed);
		return false;
	}

	APlayerController* LocalController = UGameplayStatics::GetPlayerController(this, 0);
	if (!LocalController)
	{
		FailAndReturnToMenu(NSLOCTEXT("ArenaNetwork", "MissingLocalController", "无法加入房间：本地控制器尚未就绪。"));
		return false;
	}

	PendingError = FText::GetEmpty();
	SetConnectionState(EArenaDirectConnectState::Joining);
	LocalController->ClientTravel(NormalizedAddress, TRAVEL_Absolute);
	return true;
}

// Listen Host 使用 GameMode 通知所有 Client 返回菜单；普通 Client 只断开自己并绝对旅行到本地菜单。
void UArenaDirectConnectSubsystem::LeaveNetworkGame()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		SetConnectionState(EArenaDirectConnectState::Idle);
		return;
	}

	PendingError = FText::GetEmpty();
	SetConnectionState(EArenaDirectConnectState::Idle);
	if (World->GetNetMode() == NM_ListenServer)
	{
		if (AGameModeBase* GameMode = World->GetAuthGameMode())
		{
			GameMode->ReturnToMainMenuHost();
		}
	}
	else if (APlayerController* LocalController = UGameplayStatics::GetPlayerController(this, 0))
	{
		LocalController->ClientTravel(MainMenuMapName.ToString(), TRAVEL_Absolute);
	}

}

// 消费错误文本后保留当前状态，由新菜单页面决定何时重新允许 Host/Join。
FText UArenaDirectConnectSubsystem::ConsumePendingError()
{
	FText Result = PendingError;
	PendingError = FText::GetEmpty();
	if (ConnectionState == EArenaDirectConnectState::Failed)
	{
		SetConnectionState(EArenaDirectConnectState::Idle);
	}
	return Result;
}

// 收到 Lobby 复制数据代表 Host/Join 旅行成功，统一进入 InLobby 状态。
void UArenaDirectConnectSubsystem::NotifyEnteredLobby()
{
	if (ConnectionState == EArenaDirectConnectState::Failed
		|| ConnectionState == EArenaDirectConnectState::Traveling)
	{
		return;
	}

	bReturnToMenuQueued = false;
	if (ConnectionState == EArenaDirectConnectState::Hosting
		|| ConnectionState == EArenaDirectConnectState::Joining)
	{
		PendingError = FText::GetEmpty();
	}
	SetConnectionState(EArenaDirectConnectState::InLobby);
}

// Lobby 锁定后仅更新本地状态，实际服务器旅行仍由菜单 GameMode 权威执行。
void UArenaDirectConnectSubsystem::NotifyMatchTravelStarting()
{
	if (ConnectionState != EArenaDirectConnectState::Failed)
	{
		SetConnectionState(EArenaDirectConnectState::Traveling);
	}
}

// Host 主动关闭时保存服务器给出的文本；空原因使用稳定的本地提示。
void UArenaDirectConnectSubsystem::HandleHostClosedLobby(const FText& Reason)
{
	const FText EffectiveReason = Reason.IsEmpty()
		? NSLOCTEXT("ArenaNetwork", "HostClosedLobby", "房主已关闭房间。")
		: Reason;
	FailAndReturnToMenu(EffectiveReason);
}

// 仅接受 localhost 或四段十进制 IPv4，显式拒绝 URL、地图参数和非法端口。
bool UArenaDirectConnectSubsystem::NormalizeJoinAddress(
	const FString& Address,
	FString& OutNormalizedAddress,
	FText& OutError) const
{
	const FString TrimmedAddress = Address.TrimStartAndEnd();
	if (TrimmedAddress.IsEmpty()
		|| TrimmedAddress != Address
		|| TrimmedAddress.Contains(TEXT(" "))
		|| TrimmedAddress.Contains(TEXT("\t"))
		|| TrimmedAddress.Contains(TEXT("://"))
		|| TrimmedAddress.Contains(TEXT("/"))
		|| TrimmedAddress.Contains(TEXT("?"))
		|| TrimmedAddress.Contains(TEXT("#")))
	{
		OutError = NSLOCTEXT("ArenaNetwork", "InvalidAddressFormat", "请输入 IPv4 或 localhost，可选端口，例如 192.168.1.10:7777。");
		return false;
	}

	FString HostPart = TrimmedAddress;
	int32 Port = ArenaDirectConnect::DefaultPort;
	FString PortPart;
	if (TrimmedAddress.Split(TEXT(":"), &HostPart, &PortPart, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
	{
		if (HostPart.Contains(TEXT(":")) || PortPart.IsEmpty() || !PortPart.IsNumeric())
		{
			OutError = NSLOCTEXT("ArenaNetwork", "InvalidPortFormat", "端口必须是 1 到 65535 的整数。");
			return false;
		}
		Port = FCString::Atoi(*PortPart);
	}

	if (Port < 1 || Port > 65535)
	{
		OutError = NSLOCTEXT("ArenaNetwork", "InvalidPortRange", "端口必须是 1 到 65535 的整数。");
		return false;
	}

	const bool bIsLocalhost = HostPart.Equals(TEXT("localhost"), ESearchCase::IgnoreCase);
	TArray<FString> Octets;
	HostPart.ParseIntoArray(Octets, TEXT("."), false);
	bool bIsValidIPv4 = Octets.Num() == 4;
	for (const FString& Octet : Octets)
	{
		if (!Octet.IsNumeric())
		{
			bIsValidIPv4 = false;
			break;
		}

		const int32 OctetValue = FCString::Atoi(*Octet);
		if (OctetValue < 0 || OctetValue > 255)
		{
			bIsValidIPv4 = false;
			break;
		}
	}

	if (!bIsLocalhost && !bIsValidIPv4)
	{
		OutError = NSLOCTEXT("ArenaNetwork", "InvalidHost", "主机地址必须是 IPv4 或 localhost。");
		return false;
	}

	OutNormalizedAddress = FString::Printf(TEXT("%s:%d"), *HostPart, Port);
	return true;
}

// 状态广播只面向本地 UI，不参与网络复制或服务器玩法判定。
void UArenaDirectConnectSubsystem::SetConnectionState(const EArenaDirectConnectState NewState)
{
	if (ConnectionState == NewState)
	{
		return;
	}

	const EArenaDirectConnectState OldState = ConnectionState;
	ConnectionState = NewState;
	OnConnectionStateChanged.Broadcast(OldState, NewState);
}

// 失败回调可能发生在旅行栈内，因此仅排队一次下一 Tick 返回操作。
void UArenaDirectConnectSubsystem::FailAndReturnToMenu(const FText& ErrorMessage)
{
	PendingError = ErrorMessage;
	SetConnectionState(EArenaDirectConnectState::Failed);
	if (bReturnToMenuQueued)
	{
		return;
	}

	bReturnToMenuQueued = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &UArenaDirectConnectSubsystem::ReturnToMainMenuAfterFailure));
	}
	else
	{
		bReturnToMenuQueued = false;
	}
}

// 若当前已经是无网络菜单则只保留错误，否则断开连接并回到本地主菜单。
void UArenaDirectConnectSubsystem::ReturnToMainMenuAfterFailure()
{
	bReturnToMenuQueued = false;
	UWorld* World = GetWorld();
	if (!World || MainMenuMapName.IsNone())
	{
		return;
	}

	const FString CurrentMap = UGameplayStatics::GetCurrentLevelName(this, true);
	if (World->GetNetMode() == NM_Standalone
		&& CurrentMap == FPackageName::GetShortName(MainMenuMapName.ToString()))
	{
		return;
	}

	if (APlayerController* LocalController = UGameplayStatics::GetPlayerController(this, 0))
	{
		LocalController->ClientTravel(MainMenuMapName.ToString(), TRAVEL_Absolute);
	}
}

// 网络失败统一转成房间不可用提示，详细引擎文本仅写日志便于开发排查。
void UArenaDirectConnectSubsystem::HandleNetworkFailure(
	UWorld* World,
	UNetDriver* NetDriver,
	const ENetworkFailure::Type FailureType,
	const FString& ErrorString)
{
	(void)World;
	(void)NetDriver;
	if (ConnectionState == EArenaDirectConnectState::Idle)
	{
		return;
	}
	UE_LOG(LogArenaDirectConnect, Warning, TEXT("Network failure %d: %s"), static_cast<int32>(FailureType), *ErrorString);
	FailAndReturnToMenu(BuildNetworkFailureMessage(ErrorString));
}

// Direct IP 拒绝原因可能包在引擎英文前缀中，因此使用不区分大小写的关键词保留玩家可处理的信息。
FText UArenaDirectConnectSubsystem::BuildNetworkFailureMessage(const FString& ErrorString) const
{
	if (ErrorString.Contains(TEXT("房间已满"))
		|| ErrorString.Contains(TEXT("server full"), ESearchCase::IgnoreCase)
		|| ErrorString.Contains(TEXT("server is full"), ESearchCase::IgnoreCase))
	{
		return NSLOCTEXT("ArenaNetwork", "LobbyFull", "房间已满。");
	}

	if (ErrorString.Contains(TEXT("比赛已经开始"))
		|| ErrorString.Contains(TEXT("match has started"), ESearchCase::IgnoreCase))
	{
		return NSLOCTEXT("ArenaNetwork", "MatchAlreadyStarted", "比赛已经开始，无法中途加入。");
	}

	if (ErrorString.Contains(TEXT("timeout"), ESearchCase::IgnoreCase)
		|| ErrorString.Contains(TEXT("timed out"), ESearchCase::IgnoreCase))
	{
		return NSLOCTEXT("ArenaNetwork", "ConnectionTimeout", "连接超时，请检查地址、端口和防火墙。");
	}

	return NSLOCTEXT("ArenaNetwork", "NetworkFailure", "连接失败、房间已关闭或房间不可用。");
}

// 旅行失败统一回到菜单，避免玩家停留在无 Pawn 的半初始化世界。
void UArenaDirectConnectSubsystem::HandleTravelFailure(
	UWorld* World,
	const ETravelFailure::Type FailureType,
	const FString& ErrorString)
{
	(void)World;
	UE_LOG(LogArenaDirectConnect, Warning, TEXT("Travel failure %d: %s"), static_cast<int32>(FailureType), *ErrorString);
	FailAndReturnToMenu(NSLOCTEXT("ArenaNetwork", "TravelFailure", "地图加载失败，已返回主菜单。"));
}
