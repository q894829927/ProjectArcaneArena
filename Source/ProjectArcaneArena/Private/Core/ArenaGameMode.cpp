#include "Core/ArenaGameMode.h"

#include "Character/ArenaPlayerCharacter.h"
#include "Core/ArenaGameState.h"
#include "Core/ArenaPlayerController.h"
#include "Core/ArenaPlayerState.h"

AArenaGameMode::AArenaGameMode()
{
	GameStateClass = AArenaGameState::StaticClass();
	PlayerControllerClass = AArenaPlayerController::StaticClass();
	PlayerStateClass = AArenaPlayerState::StaticClass();
	DefaultPawnClass = AArenaPlayerCharacter::StaticClass();
}
