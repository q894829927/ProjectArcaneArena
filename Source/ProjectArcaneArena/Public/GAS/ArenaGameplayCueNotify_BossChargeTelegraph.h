#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "ArenaGameplayCueNotify_BossChargeTelegraph.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;

UCLASS(Blueprintable, NotPlaceable)
class PROJECTARCANEARENA_API AArenaGameplayCueNotify_BossChargeTelegraph : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	// 创建不附着目标的 Niagara 组件，使预警保持在 Commit 时的固定世界位置。
	AArenaGameplayCueNotify_BossChargeTelegraph();

protected:
	// 首次收到持续 Cue 时按方向和距离配置预警线并启动 Niagara。
	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	// 加入中途同步时重建同一条世界空间预警线。
	virtual bool WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	// Cue 移除时立即停止 Niagara，随后允许基类回收 Actor。
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Charge")
	TObjectPtr<UNiagaraSystem> TelegraphSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Charge", meta = (ClampMin = "1.0"))
	float ReferenceLength = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Charge")
	float VerticalOffset = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Charge", meta = (ClampMin = "0.01"))
	float WidthScale = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Charge", meta = (ClampMin = "0.01"))
	float HeightScale = 2.0f;

private:
	// 将 Cue 参数中的固定位置、方向和实际距离转换为加宽抬高的 Niagara 世界 Transform。
	bool ConfigureAndActivate(const FGameplayCueParameters& Parameters);

	UPROPERTY(VisibleAnywhere, Category = "Arena|Boss|Charge")
	TObjectPtr<UNiagaraComponent> TelegraphComponent;
};
