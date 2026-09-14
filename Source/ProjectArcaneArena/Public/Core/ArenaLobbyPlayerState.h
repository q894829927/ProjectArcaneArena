#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "ArenaLobbyPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FArenaLobbyPlayerStateChangedSignature);

UCLASS()
class PROJECTARCANEARENA_API AArenaLobbyPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	// 注册 Host 与 Ready 状态的复制字段。
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 返回该 Lobby 成员是否为当前 Listen Host。
	UFUNCTION(BlueprintPure, Category = "Arena|Lobby")
	bool IsLobbyHost() const { return bIsLobbyHost; }

	// 返回该 Client 当前是否已准备；Host 始终由服务器保持 Ready。
	UFUNCTION(BlueprintPure, Category = "Arena|Lobby")
	bool IsLobbyReady() const { return bIsLobbyReady; }

	// 仅服务器设置 Host 身份，并同步 Host 自动 Ready 规则。
	void SetLobbyHost(bool bNewHost);

	// 仅服务器更新非 Host Ready 状态，旅行锁定后由 GameMode 拒绝调用。
	void SetLobbyReady(bool bNewReady);

	UPROPERTY(BlueprintAssignable, Category = "Arena|Lobby")
	FArenaLobbyPlayerStateChangedSignature OnLobbyPlayerStateChanged;

private:
	// Host 身份复制到客户端后刷新 Lobby View。
	UFUNCTION()
	void OnRep_IsLobbyHost();

	// Ready 状态复制到客户端后刷新 Lobby View。
	UFUNCTION()
	void OnRep_IsLobbyReady();

	UPROPERTY(ReplicatedUsing = OnRep_IsLobbyHost)
	bool bIsLobbyHost = false;

	UPROPERTY(ReplicatedUsing = OnRep_IsLobbyReady)
	bool bIsLobbyReady = false;
};
