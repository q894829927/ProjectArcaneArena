#include "Core/ArenaGameMode.h"

#include "Character/ArenaPlayerCharacter.h"
#include "Core/ArenaGameState.h"
#include "Core/ArenaPlayerController.h"
#include "Core/ArenaPlayerState.h"
#include "Core/ArenaWaveDataAsset.h"
#include "Core/ArenaWaveManager.h"
#include "GAS/ArenaAbilitySystemComponent.h"
#include "GAS/ArenaGameplayTags.h"

// 构造游戏模式，指定项目默认的 GameState、Controller、PlayerState 和 Pawn。
AArenaGameMode::AArenaGameMode()
{
	GameStateClass = AArenaGameState::StaticClass();
	PlayerControllerClass = AArenaPlayerController::StaticClass();
	PlayerStateClass = AArenaPlayerState::StaticClass();
	DefaultPawnClass = AArenaPlayerCharacter::StaticClass();
	WaveManagerClass = AArenaWaveManager::StaticClass();
}

// 服务器创建 WaveManager；只有配置 WaveData 后才自动启动第一波。
void AArenaGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority() || !WaveManagerClass)
	{
		return;
	}

	WaveManager = GetWorld()->SpawnActor<AArenaWaveManager>(WaveManagerClass);
	if (!WaveManager)
	{
		return;
	}

	WaveManager->Initialize(WaveData);
	if (WaveData)
	{
		GetWorldTimerManager().SetTimer(InitialWaveTimerHandle, this, &AArenaGameMode::StartNextWave, InitialWaveDelay, false);
	}
}

// 提供给后续升级选择和当前手动测试的服务器波次推进入口。
void AArenaGameMode::StartNextWave()
{
	if (HasAuthority() && WaveManager)
	{
		WaveManager->StartNextWave();
	}
}

// 检查当前 PlayerArray 中的参战玩家，单人立即失败、多人仅全员死亡时失败。
void AArenaGameMode::NotifyPlayerDeath()
{
	AArenaGameState* ArenaGameState = GetGameState<AArenaGameState>();
	if (!HasAuthority() || !ArenaGameState
		|| ArenaGameState->GetGamePhase() == EArenaGamePhase::Defeat
		|| ArenaGameState->GetGamePhase() == EArenaGamePhase::Victory)
	{
		return;
	}

	bool bFoundParticipatingPlayer = false;
	for (APlayerState* PlayerState : ArenaGameState->PlayerArray)
	{
		const AArenaPlayerState* ArenaPlayerState = Cast<AArenaPlayerState>(PlayerState);
		const UArenaAbilitySystemComponent* ArenaASC = ArenaPlayerState ? ArenaPlayerState->GetArenaAbilitySystemComponent() : nullptr;
		if (!ArenaPlayerState || !ArenaPlayerState->GetPawn() || !ArenaASC)
		{
			continue;
		}

		bFoundParticipatingPlayer = true;
		if (!ArenaASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead))
		{
			return;
		}
	}

	if (bFoundParticipatingPlayer)
	{
		if (WaveManager)
		{
			WaveManager->StopForDefeat();
		}
		ArenaGameState->SetGamePhase(EArenaGamePhase::Defeat);
	}
}
