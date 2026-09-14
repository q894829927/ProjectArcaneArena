#pragma once

#include "CoreMinimal.h"
#include "ArenaLobbyTypes.generated.h"

USTRUCT(BlueprintType)
struct PROJECTARCANEARENA_API FArenaLobbyPlayerViewData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Lobby")
	int32 PlayerId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Lobby")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Lobby")
	bool bIsHost = false;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Lobby")
	bool bIsReady = false;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Lobby")
	bool bIsLocalPlayer = false;
};

USTRUCT(BlueprintType)
struct PROJECTARCANEARENA_API FArenaLobbyViewData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Lobby")
	TArray<FArenaLobbyPlayerViewData> Players;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Lobby")
	int32 MaxPlayers = 2;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Lobby")
	bool bTravelStarting = false;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Lobby")
	bool bLocalPlayerIsHost = false;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Lobby")
	bool bLocalPlayerIsReady = false;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Lobby")
	bool bCanHostStart = false;
};
