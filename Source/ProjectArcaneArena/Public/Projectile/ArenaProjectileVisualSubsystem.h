#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArenaProjectileVisualSubsystem.generated.h"

class UNiagaraComponent;
class UNiagaraDataChannelAsset;
class UNiagaraSystem;
class UArenaProjectileSimulationSubsystem;

// P5 客户端/本地批量表现桥：从 Data Projectile Simulation 读取纯视觉快照并写入共享 Niagara Data Channel。
// 不拥有 Projectile 生命周期、碰撞或伤害；Dedicated Server 不创建 Niagara 组件。
UCLASS()
class PROJECTARCANEARENA_API UArenaProjectileVisualSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// 等待 SimulationSubsystem 初始化后绑定其帧完成事件，确保读取的是本帧稳定快照。
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// World 结束时解绑 Simulation 回调并销毁唯一共享 Niagara Component。
	virtual void Deinitialize() override;

protected:
	// 只在 Game/PIE World 创建；Dedicated Server 会在 Initialize 中直接跳过表现绑定。
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
	// Simulation Tick 完成后的同帧回调；批量写 Projectile Snapshot 与 Impact Event。
	void HandleSimulationUpdated();

	// 延迟加载约定路径下的 NDC 与 Shared Niagara System，资产缺失时只记录一次提示。
	bool EnsureVisualAssets();

	// 全局只创建一个共享 Projectile Niagara System，不随 Projectile 数量增加组件。
	void EnsureSharedProjectileSystem();

	// 将当前 Active Projectile 批量写入 NDC_ArenaProjectiles。
	void WriteProjectileSnapshot();

	// 将本帧权威 Impact 批量写入 NDC_ArenaProjectileImpacts。
	void WriteImpactEvents();

	TWeakObjectPtr<UArenaProjectileSimulationSubsystem> SimulationSubsystem;
	FDelegateHandle SimulationUpdatedHandle;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraDataChannelAsset> ProjectileDataChannel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraDataChannelAsset> ImpactDataChannel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> SharedProjectileSystem = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> SharedProjectileComponent = nullptr;

	TArray<struct FArenaProjectileVisualSample> VisualSamples;
	TArray<struct FArenaProjectileImpactVisualEvent> ImpactEvents;

	bool bAttemptedAssetLoad = false;
	bool bLoggedMissingAssets = false;
};
