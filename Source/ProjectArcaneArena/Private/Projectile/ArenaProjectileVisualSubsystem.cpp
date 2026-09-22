#include "Projectile/ArenaProjectileVisualSubsystem.h"

#include "Core/ArenaLogCategories.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "NiagaraComponent.h"
#include "NiagaraDataChannelAccessor.h"
#include "NiagaraDataChannel.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Projectile/ArenaProjectileSimulationSubsystem.h"
#include "Projectile/ArenaProjectileTypes.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
	TAutoConsoleVariable<int32> CVarArenaProjectileVisualEnabled(
		TEXT("arena.Projectile.Visual.Enabled"),
		1,
		TEXT("Enable the shared Niagara/NDC projectile presentation bridge when non-zero."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarArenaProjectileVisualMaxSamples(
		TEXT("arena.Projectile.Visual.MaxSamples"),
		10000,
		TEXT("Maximum active projectile samples submitted to Niagara per frame. Visual-only budget; gameplay is unchanged."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarArenaProjectileVisualMaxImpacts(
		TEXT("arena.Projectile.Visual.MaxImpacts"),
		2048,
		TEXT("Maximum projectile impact presentation events submitted to Niagara per frame. Visual-only budget."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarArenaProjectileVisualLogWrites(
		TEXT("arena.Projectile.Visual.LogWrites"),
		0,
		TEXT("Log shared Niagara Data Channel write counts when non-zero."),
		ECVF_Default);

	const TCHAR* ProjectileDataChannelPath =
		TEXT("/Game/ProjectArcaneArena/Combat/Projectiles/VFX/DataChannels/NDC_ArenaProjectiles.NDC_ArenaProjectiles");
	const TCHAR* ImpactDataChannelPath =
		TEXT("/Game/ProjectArcaneArena/Combat/Projectiles/VFX/DataChannels/NDC_ArenaProjectileImpacts.NDC_ArenaProjectileImpacts");
	const TCHAR* SharedProjectileSystemPath =
		TEXT("/Game/ProjectArcaneArena/Combat/Projectiles/VFX/Systems/NS_ArenaProjectiles_Shared.NS_ArenaProjectiles_Shared");

	const FName PositionName(TEXT("Position"));
	const FName VelocityName(TEXT("Velocity"));
	const FName RadiusName(TEXT("Radius"));
	const FName RemainingLifeName(TEXT("RemainingLife"));
	const FName VisualTypeName(TEXT("VisualType"));
	const FName ProjectileSlotName(TEXT("ProjectileSlot"));
	const FName GenerationName(TEXT("Generation"));
	const FName WeaponRuntimeIDName(TEXT("WeaponRuntimeID"));
	const FName AttackInstanceIDName(TEXT("AttackInstanceID"));
	const FName PelletIndexName(TEXT("PelletIndex"));
	const FName PelletCountName(TEXT("PelletCount"));
	const FName NormalName(TEXT("Normal"));
	const FName ProjectileHitOrdinalName(TEXT("ProjectileHitOrdinal"));
}

// 先初始化 SimulationSubsystem，再绑定其帧完成事件；表现层不参与服务器权威模拟。
void UArenaProjectileVisualSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UArenaProjectileSimulationSubsystem>();
	Super::Initialize(Collection);

	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	UArenaProjectileSimulationSubsystem* Simulation =
		World->GetSubsystem<UArenaProjectileSimulationSubsystem>();
	if (!Simulation)
	{
		return;
	}

	SimulationSubsystem = Simulation;
	SimulationUpdatedHandle = Simulation->OnSimulationUpdated().AddUObject(
		this,
		&UArenaProjectileVisualSubsystem::HandleSimulationUpdated);

	UE_LOG(
		LogArenaProjectile,
		Warning,
		TEXT("[P5Diag] VisualSubsystem initialized. World=%s NetMode=%d Simulation=%s DelegateBound=%d"),
		*GetNameSafe(World),
		static_cast<int32>(World->GetNetMode()),
		*GetNameSafe(Simulation),
		SimulationUpdatedHandle.IsValid() ? 1 : 0);
}

// World 收尾时对称解绑，避免 PIE 重开后旧 Subsystem 回调进入新 World。
void UArenaProjectileVisualSubsystem::Deinitialize()
{
	if (UArenaProjectileSimulationSubsystem* Simulation = SimulationSubsystem.Get())
	{
		if (SimulationUpdatedHandle.IsValid())
		{
			Simulation->OnSimulationUpdated().Remove(SimulationUpdatedHandle);
		}
	}

	SimulationUpdatedHandle.Reset();
	SimulationSubsystem.Reset();

	if (IsValid(SharedProjectileComponent))
	{
		SharedProjectileComponent->Deactivate();
		SharedProjectileComponent->DestroyComponent();
	}
	SharedProjectileComponent = nullptr;
	SharedProjectileSystem = nullptr;
	ProjectileDataChannel = nullptr;
	ImpactDataChannel = nullptr;
	VisualSamples.Reset();
	ImpactEvents.Reset();

	Super::Deinitialize();
}

// 编辑器资产预览 World 不创建 Data Projectile 表现桥。
bool UArenaProjectileVisualSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

// Simulation 完成后立刻提交同帧视觉数据；即使视觉预算截断，也不会改变真实 Projectile/HitCommand。
void UArenaProjectileVisualSubsystem::HandleSimulationUpdated()
{
	static int32 DiagCounter = 0;
	++DiagCounter;

	if (DiagCounter % 60 == 0)
	{
		UE_LOG(
			LogArenaProjectile,
			Warning,
			TEXT("[P5Diag] HandleSimulationUpdated called. Enabled=%d"),
			CVarArenaProjectileVisualEnabled.GetValueOnGameThread());
	}

	if (CVarArenaProjectileVisualEnabled.GetValueOnGameThread() == 0)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArenaProjectileVisualNDC);
	if (!EnsureVisualAssets())
	{
		if (DiagCounter % 60 == 0)
		{
			UE_LOG(LogArenaProjectile, Warning, TEXT("[P5Diag] EnsureVisualAssets failed."));
		}
		return;
	}

	EnsureSharedProjectileSystem();
	WriteProjectileSnapshot();
	WriteImpactEvents();
}

// 资产路径固定为 P5 约定目录；首次不存在时保持 Gameplay 正常并给出一次明确配置提示。
bool UArenaProjectileVisualSubsystem::EnsureVisualAssets()
{
	if (!bAttemptedAssetLoad)
	{
		bAttemptedAssetLoad = true;
		ProjectileDataChannel = LoadObject<UNiagaraDataChannelAsset>(nullptr, ProjectileDataChannelPath);
		ImpactDataChannel = LoadObject<UNiagaraDataChannelAsset>(nullptr, ImpactDataChannelPath);
		SharedProjectileSystem = LoadObject<UNiagaraSystem>(nullptr, SharedProjectileSystemPath);

		UE_LOG(
			LogArenaProjectile,
			Warning,
			TEXT("[P5Diag] Asset load: ProjectileNDC=%s ImpactNDC=%s SharedSystem=%s"),
			ProjectileDataChannel ? TEXT("OK") : TEXT("NULL"),
			ImpactDataChannel ? TEXT("OK") : TEXT("NULL"),
			SharedProjectileSystem ? TEXT("OK") : TEXT("NULL"));
	}

	const bool bReady = ProjectileDataChannel && ImpactDataChannel && SharedProjectileSystem;
	if (!bReady && !bLoggedMissingAssets)
	{
		bLoggedMissingAssets = true;
		UE_LOG(
			LogArenaProjectile,
			Warning,
			TEXT("P5 projectile visuals are waiting for assets: %s, %s, %s. Gameplay simulation remains active."),
			ProjectileDataChannelPath,
			ImpactDataChannelPath,
			SharedProjectileSystemPath);
	}

	return bReady;
}

// 一个 World 只保留一个共享 Projectile Niagara Component；Projectile 数量不会增加 Component 数。
void UArenaProjectileVisualSubsystem::EnsureSharedProjectileSystem()
{
	if (IsValid(SharedProjectileComponent) || !SharedProjectileSystem)
	{
		return;
	}

	SharedProjectileComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		this,
		SharedProjectileSystem,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		FVector::OneVector,
		false,
		true,
		ENCPoolMethod::None,
		false);

	if (!SharedProjectileComponent)
	{
		UE_LOG(LogArenaProjectile, Warning, TEXT("Failed to create shared projectile Niagara component."));
	}
}

// 每帧把 Active Projectile 以一个 Batch 写入 Global NDC；Position/Velocity 等变量名必须与 NDC 资产一致。
void UArenaProjectileVisualSubsystem::WriteProjectileSnapshot()
{
	UArenaProjectileSimulationSubsystem* Simulation = SimulationSubsystem.Get();
	if (!Simulation || !ProjectileDataChannel)
	{
		return;
	}

	const int32 MaxSamples = FMath::Max(CVarArenaProjectileVisualMaxSamples.GetValueOnGameThread(), 0);
	Simulation->BuildVisualSnapshot(VisualSamples, MaxSamples);

	static int32 SnapshotDiagCounter = 0;
	++SnapshotDiagCounter;
	if (SnapshotDiagCounter % 30 == 0 || !VisualSamples.IsEmpty())
	{
		UE_LOG(
			LogArenaProjectile,
			Warning,
			TEXT("[P5Diag] Snapshot Active=%d Samples=%d Max=%d"),
			Simulation->GetActiveProjectileCount(),
			VisualSamples.Num(),
			MaxSamples);
	}

	if (VisualSamples.IsEmpty())
	{
		return;
	}

	FNiagaraDataChannelSearchParameters SearchParams;
	UNiagaraDataChannelWriter* Writer = UNiagaraDataChannelLibrary::WriteToNiagaraDataChannel(
		this,
		ProjectileDataChannel,
		SearchParams,
		VisualSamples.Num(),
		false,
		true,
		true,
		TEXT("ArenaProjectileVisualSnapshot"));
	if (!Writer)
	{
		UE_LOG(
			LogArenaProjectile,
			Error,
			TEXT("[P5Diag] Projectile NDC Writer is NULL. Samples=%d"),
			VisualSamples.Num());
		return;
	}

	for (int32 Index = 0; Index < VisualSamples.Num(); ++Index)
	{
		const FArenaProjectileVisualSample& Sample = VisualSamples[Index];
		Writer->WritePosition(PositionName, Index, Sample.Position);
		Writer->WriteVector(VelocityName, Index, Sample.Velocity);
		Writer->WriteFloat(RadiusName, Index, Sample.Radius);
		Writer->WriteFloat(RemainingLifeName, Index, Sample.RemainingLife);
		Writer->WriteInt(VisualTypeName, Index, Sample.VisualTypeID);
		Writer->WriteInt(ProjectileSlotName, Index, Sample.Handle.Slot);
		Writer->WriteInt(GenerationName, Index, Sample.Handle.Generation);
		Writer->WriteInt(WeaponRuntimeIDName, Index, Sample.WeaponRuntimeID);
		Writer->WriteInt(AttackInstanceIDName, Index, Sample.AttackInstanceID);
		Writer->WriteInt(PelletIndexName, Index, Sample.PelletIndex);
		Writer->WriteInt(PelletCountName, Index, Sample.PelletCount);
	}

	if (CVarArenaProjectileVisualLogWrites.GetValueOnGameThread() != 0)
	{
		UE_LOG(
			LogArenaProjectile,
			Log,
			TEXT("Projectile Visual NDC snapshot wrote %d/%d active samples."),
			VisualSamples.Num(),
			Simulation->GetActiveProjectileCount());
	}
}

// Impact 使用独立 NDC，一次 Tick 批量写入所有权威命中；后续 Niagara 可按 VisualType 选择不同 Burst。
void UArenaProjectileVisualSubsystem::WriteImpactEvents()
{
	UArenaProjectileSimulationSubsystem* Simulation = SimulationSubsystem.Get();
	if (!Simulation || !ImpactDataChannel)
	{
		return;
	}

	Simulation->CopyFrameImpactVisualEvents(ImpactEvents);
	const int32 MaxImpacts = FMath::Max(CVarArenaProjectileVisualMaxImpacts.GetValueOnGameThread(), 0);
	if (ImpactEvents.Num() > MaxImpacts)
	{
		ImpactEvents.SetNum(MaxImpacts, EAllowShrinking::No);
	}

	if (ImpactEvents.IsEmpty())
	{
		return;
	}

	FNiagaraDataChannelSearchParameters SearchParams;
	UNiagaraDataChannelWriter* Writer = UNiagaraDataChannelLibrary::WriteToNiagaraDataChannel(
		this,
		ImpactDataChannel,
		SearchParams,
		ImpactEvents.Num(),
		false,
		true,
		true,
		TEXT("ArenaProjectileImpactBatch"));
	if (!Writer)
	{
		return;
	}

	for (int32 Index = 0; Index < ImpactEvents.Num(); ++Index)
	{
		const FArenaProjectileImpactVisualEvent& Impact = ImpactEvents[Index];
		Writer->WritePosition(PositionName, Index, Impact.Position);
		Writer->WriteVector(NormalName, Index, Impact.Normal);
		Writer->WriteInt(VisualTypeName, Index, Impact.VisualTypeID);
		Writer->WriteInt(WeaponRuntimeIDName, Index, Impact.WeaponRuntimeID);
		Writer->WriteInt(AttackInstanceIDName, Index, Impact.AttackInstanceID);
		Writer->WriteInt(PelletIndexName, Index, Impact.PelletIndex);
		Writer->WriteInt(ProjectileHitOrdinalName, Index, Impact.ProjectileHitOrdinal);
	}

	if (CVarArenaProjectileVisualLogWrites.GetValueOnGameThread() != 0)
	{
		UE_LOG(
			LogArenaProjectile,
			Log,
			TEXT("Projectile Impact NDC wrote %d events."),
			ImpactEvents.Num());
	}
}
