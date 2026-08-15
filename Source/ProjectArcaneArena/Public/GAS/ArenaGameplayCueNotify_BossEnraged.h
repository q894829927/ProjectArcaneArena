#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "ArenaGameplayCueNotify_BossEnraged.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;

UCLASS(Blueprintable, NotPlaceable)
class PROJECTARCANEARENA_API AArenaGameplayCueNotify_BossEnraged : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	// 创建附着 Boss 根组件的 Niagara，并启用低频重播检查以兼容非循环资源。
	AArenaGameplayCueNotify_BossEnraged();

	// 持续 Cue 活跃时按配置间隔重启已结束的 Niagara 内部 Burst。
	virtual void Tick(float DeltaSeconds) override;

protected:
	// 首次激活时附着目标并启动狂暴表现。
	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;

	// 晚加入客户端收到 WhileActive 时重建相同的附着表现。
	virtual bool WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;

	// 效果移除时立即停止 Niagara，避免死亡或终局后残留。
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Enrage")
	TObjectPtr<UNiagaraSystem> EnrageSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Enrage")
	FVector EnrageScale = FVector(1.5f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Enrage")
	float VerticalOffset = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Enrage")
	bool bRestartSystemWhileActive = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Enrage", meta = (ClampMin = "0.05"))
	float SystemReplayInterval = 0.8f;

private:
	// 将 Cue Actor 附着到当前 Boss，并以相对坐标激活配置的 Niagara。
	bool ConfigureAndActivate(AActor* MyTarget);

	UPROPERTY(VisibleAnywhere, Category = "Arena|Boss|Enrage")
	TObjectPtr<UNiagaraComponent> EnrageComponent;

	bool bCuePresentationActive = false;
	float SystemReplayElapsedTime = 0.0f;
};
