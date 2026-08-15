#include "GAS/ArenaGameplayCueNotify_BossFireZoneRadius.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

// 圆形 Cue Actor 统一处理 FireZone 主体和可选边界，Active
// 火区可持续显示真实伤害范围。
AArenaGameplayCueNotify_BossFireZoneRadius::AArenaGameplayCueNotify_BossFireZoneRadius()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickInterval = 0.05f;
	bAutoAttachToOwner = false;
	bAutoDestroyOnRemove = true;

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	ZoneComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ZoneComponent"));
	ZoneComponent->SetupAttachment(SceneRoot);
	ZoneComponent->SetAutoActivate(false);
	BoundaryComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("BoundaryComponent"));
	BoundaryComponent->SetupAttachment(SceneRoot);
	BoundaryComponent->SetAutoActivate(false);
	BoundaryComponent->SetTranslucentSortPriority(10);
	BoundaryMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoundaryMeshComponent"));
	BoundaryMeshComponent->SetupAttachment(SceneRoot);
	BoundaryMeshComponent->SetMobility(EComponentMobility::Movable);
	BoundaryMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoundaryMeshComponent->SetGenerateOverlapEvents(false);
	BoundaryMeshComponent->SetCastShadow(false);
	BoundaryMeshComponent->SetHiddenInGame(true);
	BoundaryMeshComponent->SetVisibility(false);
	BoundaryMeshComponent->SetTranslucentSortPriority(11);
	BoundaryMeshComponent->SetAbsolute(false, false, true);
}

// 持续模式同时检查组件完成状态并按固定间隔强制重播，兼容 System 仍 Active 但内部 Burst 已经结束的 Niagara。
void AArenaGameplayCueNotify_BossFireZoneRadius::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bCuePresentationActive || !bRestartZoneSystemWhileActive || !ZoneComponent || !ZoneSystem)
	{
		return;
	}

	ZoneSystemReplayElapsedTime += DeltaSeconds;
	const float SafeReplayInterval = FMath::Max(ZoneSystemReplayInterval, 0.05f);
	if (!ZoneComponent->IsActive() || ZoneSystemReplayElapsedTime >= SafeReplayInterval)
	{
		ZoneComponent->SetAsset(ZoneSystem);
		ZoneComponent->Activate(true);
		ZoneSystemReplayElapsedTime = 0.0f;
	}
}

// OnActive 与 WhileActive 使用同一路径，确保首次激活和晚加入客户端外观一致。
bool AArenaGameplayCueNotify_BossFireZoneRadius::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	return ConfigureAndActivate(Parameters);
}

// 复制 Actor 晚到时重新应用世界位置与半径，不依赖 Boss 当前所在位置。
bool AArenaGameplayCueNotify_BossFireZoneRadius::WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	return ConfigureAndActivate(Parameters);
}

// Area 或预警结束时立即停掉粒子，并允许 GameplayCueManager 回收 Actor。
bool AArenaGameplayCueNotify_BossFireZoneRadius::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	bCuePresentationActive = false;
	ZoneSystemReplayElapsedTime = 0.0f;
	SetActorTickEnabled(false);
	if (ZoneComponent)
	{
		ZoneComponent->DeactivateImmediate();
	}
	if (BoundaryComponent)
	{
		BoundaryComponent->DeactivateImmediate();
	}
	if (BoundaryMeshComponent)
	{
		BoundaryMeshComponent->SetHiddenInGame(true);
		BoundaryMeshComponent->SetVisibility(false, true);
	}
	return true;
}

// 主体和边界共用真实 XY 半径；Active 主体开启完成后重启，静态圆环使用持续材质，二者共同覆盖完整 Area 生命周期。
bool AArenaGameplayCueNotify_BossFireZoneRadius::ConfigureAndActivate(const FGameplayCueParameters& Parameters)
{
	if (!ZoneComponent || !ZoneSystem)
	{
		return false;
	}

	const float RadiusScale = FMath::Max(Parameters.RawMagnitude / FMath::Max(ReferenceRadius, 1.0f), 0.01f);
	SetActorLocation(FVector(Parameters.Location) + FVector(0.0f, 0.0f, VerticalOffset));
	SetActorRotation(FRotator::ZeroRotator);
	SetActorScale3D(FVector(RadiusScale, RadiusScale, FMath::Max(HeightScale, 0.01f)));
	ZoneComponent->SetAsset(ZoneSystem);
	ZoneComponent->Activate(true);
	bCuePresentationActive = true;
	ZoneSystemReplayElapsedTime = 0.0f;
	SetActorTickEnabled(bRestartZoneSystemWhileActive);
	if (BoundaryComponent)
	{
		BoundaryComponent->SetRelativeLocation(FVector(0.0f, 0.0f, BoundaryVerticalOffset));
		if (BoundarySystem)
		{
			BoundaryComponent->SetAsset(BoundarySystem);
			BoundaryComponent->Activate(true);
		}
		else
		{
			BoundaryComponent->DeactivateImmediate();
		}
	}
	if (BoundaryMeshComponent)
	{
		BoundaryMeshComponent->SetWorldLocation(GetActorLocation() + FVector(0.0f, 0.0f, BoundaryVerticalOffset));
		BoundaryMeshComponent->SetWorldRotation(FRotator::ZeroRotator);
		if (BoundaryMesh)
		{
			BoundaryMeshComponent->SetStaticMesh(BoundaryMesh);
			if (BoundaryMaterial)
			{
				const int32 MaterialSlotCount = FMath::Max(BoundaryMeshComponent->GetNumMaterials(), 1);
				for (int32 MaterialIndex = 0; MaterialIndex < MaterialSlotCount; ++MaterialIndex)
				{
					BoundaryMeshComponent->SetMaterial(MaterialIndex, BoundaryMaterial);
				}
			}
			const FVector MeshExtent = BoundaryMesh->GetBounds().BoxExtent;
			const float MeshRadius = FMath::Max(FMath::Max(MeshExtent.X, MeshExtent.Y), 1.0f);
			const float MeshRadiusScale = Parameters.RawMagnitude / MeshRadius;
			BoundaryMeshComponent->SetWorldScale3D(
				FVector(MeshRadiusScale, MeshRadiusScale, FMath::Max(BoundaryMeshThicknessScale, 0.01f)));
			BoundaryMeshComponent->SetHiddenInGame(false);
			BoundaryMeshComponent->SetVisibility(true, true);
		}
		else
		{
			BoundaryMeshComponent->SetHiddenInGame(true);
			BoundaryMeshComponent->SetVisibility(false, true);
		}
	}
	return true;
}
