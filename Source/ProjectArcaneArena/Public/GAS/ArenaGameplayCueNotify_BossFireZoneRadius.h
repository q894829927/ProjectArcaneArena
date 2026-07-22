#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "ArenaGameplayCueNotify_BossFireZoneRadius.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS(Blueprintable, NotPlaceable)
class PROJECTARCANEARENA_API AArenaGameplayCueNotify_BossFireZoneRadius : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	// 创建固定世界位置的主体、可选边界 Niagara 与常驻圆环 Mesh，并为需要持续播放的主体准备低频重启检查。
	AArenaGameplayCueNotify_BossFireZoneRadius();
	// Active 火区仅在 Niagara 自行完成后重启主体系统，保持表现覆盖整个 Area 生命周期。
	virtual void Tick(float DeltaSeconds) override;

protected:
	// 首次收到持续 Cue 时按 RawMagnitude 表示的真实半径启动圆形表现。
	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	// 客户端中途同步持续 Cue 时按同一组参数重建圆形表现。
	virtual bool WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	// Cue 移除时立即停止 Niagara 并隐藏圆环 Mesh，避免预警或火区残留。
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Fire Zone")
	TObjectPtr<UNiagaraSystem> ZoneSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Fire Zone")
	bool bRestartZoneSystemWhileActive = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Fire Zone", meta = (ClampMin = "0.05"))
	float ZoneSystemReplayInterval = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Fire Zone")
	TObjectPtr<UNiagaraSystem> BoundarySystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Fire Zone")
	TObjectPtr<UStaticMesh> BoundaryMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Fire Zone")
	TObjectPtr<UMaterialInterface> BoundaryMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Fire Zone", meta = (ClampMin = "1.0"))
	float ReferenceRadius = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Fire Zone")
	float VerticalOffset = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Fire Zone")
	float BoundaryVerticalOffset = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Fire Zone", meta = (ClampMin = "0.01"))
	float BoundaryMeshThicknessScale = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss|Fire Zone", meta = (ClampMin = "0.01"))
	float HeightScale = 1.0f;

private:
	// 把固定位置和真实半径转换为世界 Transform，并激活主体、可选边界 Niagara 与使用持续材质的常驻圆环 Mesh。
	bool ConfigureAndActivate(const FGameplayCueParameters& Parameters);

	UPROPERTY(VisibleAnywhere, Category = "Arena|Boss|Fire Zone")
	TObjectPtr<UNiagaraComponent> ZoneComponent;

	UPROPERTY(VisibleAnywhere, Category = "Arena|Boss|Fire Zone")
	TObjectPtr<UNiagaraComponent> BoundaryComponent;

	UPROPERTY(VisibleAnywhere, Category = "Arena|Boss|Fire Zone")
	TObjectPtr<UStaticMeshComponent> BoundaryMeshComponent;

	bool bCuePresentationActive = false;
	float ZoneSystemReplayElapsedTime = 0.0f;
};
