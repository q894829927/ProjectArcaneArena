#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ArenaMainMenuPlayerController.generated.h"

class UArenaMainMenuWidget;

UCLASS()
class PROJECTARCANEARENA_API AArenaMainMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AArenaMainMenuPlayerController();

protected:
	// 仅在本地 Controller 创建菜单、显示鼠标并建立 UIOnly 焦点。
	virtual void BeginPlay() override;

	// 关卡旅行或退出时解绑 View 意图并清理本地菜单引用。
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// 创建并显示配置的菜单 Widget；配置缺失时回退到原生 View 类。
	void CreateMainMenu();

	// 接收 View 的开始意图，防重后打开正式战斗关卡。
	UFUNCTION()
	void HandleStartGameRequested();

	// 接收 View 的退出意图，通过引擎统一接口退出 Standalone 或结束 PIE。
	UFUNCTION()
	void HandleQuitGameRequested();

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Main Menu")
	TSubclassOf<UArenaMainMenuWidget> MainMenuWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Main Menu")
	FName GameplayMapName = TEXT("/Game/TopDown/Lvl_TopDown");

	UPROPERTY(Transient)
	TObjectPtr<UArenaMainMenuWidget> MainMenuWidget;

	bool bTravelRequested = false;
};
