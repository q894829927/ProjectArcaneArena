#include "Core/ArenaMainMenuGameMode.h"

#include "Core/ArenaLobbyPlayerState.h"
#include "Core/ArenaMainMenuGameState.h"
#include "Core/ArenaMainMenuPlayerController.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"

DEFINE_LOG_CATEGORY_STATIC(LogArenaLobby, Log, All);

// 菜单关卡仅创建专用 Controller，并同时禁用默认 Pawn、SpectatorPawn、HUD 和战斗框架对象。
AArenaMainMenuGameMode::AArenaMainMenuGameMode()
{
	PlayerControllerClass = AArenaMainMenuPlayerController::StaticClass();
	PlayerStateClass = AArenaLobbyPlayerState::StaticClass();
	GameStateClass = AArenaMainMenuGameState::StaticClass();
	DefaultPawnClass = nullptr;
	SpectatorClass = nullptr;
	HUDClass = nullptr;
	bStartPlayersAsSpectators = true;
	bUseSeamlessTravel = true;
}

// Super 先让 GameSession 读取 MaxPlayers，再保存项目自己的 Lobby 标记和二至四人容量。
void AArenaMainMenuGameMode::InitGame(
	const FString& MapName,
	const FString& Options,
	FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	bConfiguredAsLobby = UGameplayStatics::HasOption(Options, TEXT("ArenaLobby"))
		&& UGameplayStatics::ParseOption(Options, TEXT("ArenaLobby")) == TEXT("1");
	const FString MaxPlayersOption = UGameplayStatics::ParseOption(Options, TEXT("MaxPlayers"));
	ConfiguredLobbyMaxPlayers = FMath::Clamp(
		MaxPlayersOption.IsNumeric() ? FCString::Atoi(*MaxPlayersOption) : 2,
		2,
		4);
}

// GameState 存在后发布 Lobby 配置，普通启动菜单不会误显示玩家列表或 Ready 操作。
void AArenaMainMenuGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (AArenaMainMenuGameState* MenuGameState = GetGameState<AArenaMainMenuGameState>())
	{
		MenuGameState->InitializeLobby(ConfiguredLobbyMaxPlayers, bConfiguredAsLobby);
	}
}

// GameSession 先处理容量；若 Host 已锁定比赛，则拒绝旅行切换窗口内的新 Direct IP 登录。
void AArenaMainMenuGameMode::PreLogin(
	const FString& Options,
	const FString& Address,
	const FUniqueNetIdRepl& UniqueId,
	FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	const AArenaMainMenuGameState* MenuGameState = GetGameState<AArenaMainMenuGameState>();
	if (MenuGameState
		&& MenuGameState->IsLobbyActive()
		&& MenuGameState->PlayerArray.Num() >= ConfiguredLobbyMaxPlayers)
	{
		ErrorMessage = TEXT("房间已满。");
	}
	else if (ErrorMessage.IsEmpty() && MenuGameState && MenuGameState->IsTravelStarting())
	{
		ErrorMessage = TEXT("比赛已经开始，无法中途加入。");
	}
}

// Listen Server 的本地首名成员成为唯一 Host；其余成员保持未准备等待独立确认。
void AArenaMainMenuGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	AArenaLobbyPlayerState* LobbyPlayerState = NewPlayer
		? NewPlayer->GetPlayerState<AArenaLobbyPlayerState>()
		: nullptr;
	if (!bConfiguredAsLobby || !LobbyPlayerState)
	{
		return;
	}

	const bool bShouldBeHost = NewPlayer->IsLocalController() && FindLobbyHostPlayerState() == nullptr;
	LobbyPlayerState->SetLobbyHost(bShouldBeHost);
	LobbyPlayerState->SetLobbyReady(false);
	UE_LOG(
		LogArenaLobby,
		Log,
		TEXT("Lobby member %s joined as %s (%d/%d)."),
		*GetNameSafe(LobbyPlayerState),
		bShouldBeHost ? TEXT("Host") : TEXT("Client"),
		GameState ? GameState->PlayerArray.Num() : 0,
		ConfiguredLobbyMaxPlayers);
}

// Super 移除 PlayerState 后其余客户端通过 PlayerArray 复制自动刷新 Ready 条件。
void AArenaMainMenuGameMode::Logout(AController* Exiting)
{
	UE_LOG(LogArenaLobby, Log, TEXT("Lobby controller %s is leaving."), *GetNameSafe(Exiting));
	Super::Logout(Exiting);
}

// Ready 请求只接受当前 Lobby 中的非 Host 成员，旅行开始后所有状态冻结。
void AArenaMainMenuGameMode::SetLobbyPlayerReady(
	APlayerController* RequestingController,
	const bool bReady)
{
	AArenaMainMenuGameState* MenuGameState = GetGameState<AArenaMainMenuGameState>();
	AArenaLobbyPlayerState* LobbyPlayerState = RequestingController
		? RequestingController->GetPlayerState<AArenaLobbyPlayerState>()
		: nullptr;
	if (!HasAuthority()
		|| !MenuGameState
		|| !MenuGameState->IsLobbyActive()
		|| MenuGameState->IsTravelStarting()
		|| !LobbyPlayerState
		|| LobbyPlayerState->IsLobbyHost()
		|| !MenuGameState->PlayerArray.Contains(LobbyPlayerState))
	{
		return;
	}

	LobbyPlayerState->SetLobbyReady(bReady);
}

// Host 开始时重新验证当前成员和 Ready，并把实际人数写入正式地图等待与容量参数。
void AArenaMainMenuGameMode::TryStartLobbyMatch(APlayerController* RequestingController)
{
	AArenaMainMenuGameState* MenuGameState = GetGameState<AArenaMainMenuGameState>();
	const AArenaLobbyPlayerState* RequestingPlayerState = RequestingController
		? RequestingController->GetPlayerState<AArenaLobbyPlayerState>()
		: nullptr;
	if (!HasAuthority()
		|| !MenuGameState
		|| !RequestingPlayerState
		|| !RequestingPlayerState->IsLobbyHost()
		|| !CanStartLobbyMatch()
		|| GameplayMapName.IsNone())
	{
		return;
	}

	const FString GameplayPackageName = GameplayMapName.ToString();
	if (!FPackageName::DoesPackageExist(GameplayPackageName))
	{
		UE_LOG(LogArenaLobby, Error, TEXT("Lobby gameplay map does not exist: %s"), *GameplayPackageName);
		return;
	}

	const int32 TravelingPlayerCount = MenuGameState->PlayerArray.Num();
	MenuGameState->SetTravelStarting(true);
	const FString TravelURL = FString::Printf(
		TEXT("%s?listen?ArenaMatchStarted=1?ExpectedPlayers=%d?MaxPlayers=%d"),
		*GameplayPackageName,
		TravelingPlayerCount,
		TravelingPlayerCount);
	UE_LOG(LogArenaLobby, Log, TEXT("Starting seamless Lobby travel: %s"), *TravelURL);
	GetWorld()->ServerTravel(TravelURL, false);
}

// 开始条件只依赖当前实际成员，不要求填满 Host 选择容量；Host 始终视为 Ready。
bool AArenaMainMenuGameMode::CanStartLobbyMatch() const
{
	const AArenaMainMenuGameState* MenuGameState = GetGameState<AArenaMainMenuGameState>();
	if (!MenuGameState
		|| !MenuGameState->IsLobbyActive()
		|| MenuGameState->IsTravelStarting()
		|| MenuGameState->PlayerArray.Num() < 2
		|| !FindLobbyHostPlayerState())
	{
		return false;
	}

	for (const APlayerState* PlayerState : MenuGameState->PlayerArray)
	{
		const AArenaLobbyPlayerState* LobbyPlayerState = Cast<AArenaLobbyPlayerState>(PlayerState);
		if (!LobbyPlayerState || (!LobbyPlayerState->IsLobbyHost() && !LobbyPlayerState->IsLobbyReady()))
		{
			return false;
		}
	}
	return true;
}

// 精确返回唯一 Host；若错误配置产生多个 Host，则拒绝开始而不是任意选择一个。
AArenaLobbyPlayerState* AArenaMainMenuGameMode::FindLobbyHostPlayerState() const
{
	AArenaLobbyPlayerState* FoundHost = nullptr;
	if (!GameState)
	{
		return nullptr;
	}

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		AArenaLobbyPlayerState* LobbyPlayerState = Cast<AArenaLobbyPlayerState>(PlayerState);
		if (!LobbyPlayerState || !LobbyPlayerState->IsLobbyHost())
		{
			continue;
		}
		if (FoundHost)
		{
			return nullptr;
		}
		FoundHost = LobbyPlayerState;
	}
	return FoundHost;
}

// 菜单没有 PlayerStart；清空错误并保留 Controller 原点即可完成本地登录初始化。
bool AArenaMainMenuGameMode::UpdatePlayerStartSpot(AController* Player, const FString& Portal, FString& OutErrorMessage)
{
	(void)Portal;
	OutErrorMessage.Reset();

	if (Player)
	{
		Player->SetInitialLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
	}

	return Player != nullptr;
}

// 菜单 View 由专用 PlayerController 创建，跳过默认 AHUD Actor 的生成 RPC。
void AArenaMainMenuGameMode::InitializeHUDForPlayer_Implementation(APlayerController* NewPlayer)
{
	(void)NewPlayer;
}

// 菜单只需要 Controller 驱动 UI，跳过 AGameModeBase 的出生点查找和 Pawn 生成流程。
void AArenaMainMenuGameMode::RestartPlayer(AController* NewPlayer)
{
	(void)NewPlayer;
}
