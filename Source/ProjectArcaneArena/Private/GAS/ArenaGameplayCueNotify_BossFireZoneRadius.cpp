#include "GAS/ArenaGameplayCueNotify_BossFireZoneRadius.h"

#include "Components/SceneComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

// 圆形 Cue Actor 统一处理 FireZone 预警和 Active 表现，蓝图只配置不同 Niagara。
AArenaGameplayCueNotify_BossFireZoneRadius::AArenaGameplayCueNotify_BossFireZoneRadius()
{
	PrimaryActorTick.bCanEverTick = false;
	bAutoAttachToOwner = false;
	bAutoDestroyOnRemove = true;

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	ZoneComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ZoneComponent"));
	ZoneComponent->SetupAttachment(SceneRoot);
	ZoneComponent->SetAutoActivate(false);
}

// OnActive 与 WhileActive 使用同一路径，确保首次激活和晚加入客户端外观一致。
bool AArenaGameplayCueNotify_BossFireZoneRadius::OnActive_Implementation(
	AActor* MyTarget,
	const FGameplayCueParameters& Parameters)
{
	return ConfigureAndActivate(Parameters);
}

// 复制 Actor 晚到时重新应用世界位置与半径，不依赖 Boss 当前所在位置。
bool AArenaGameplayCueNotify_BossFireZoneRadius::WhileActive_Implementation(
	AActor* MyTarget,
	const FGameplayCueParameters& Parameters)
{
	return ConfigureAndActivate(Parameters);
}

// Area 或预警结束时立即停掉粒子，并允许 GameplayCueManager 回收 Actor。
bool AArenaGameplayCueNotify_BossFireZoneRadius::OnRemove_Implementation(
	AActor* MyTarget,
	const FGameplayCueParameters& Parameters)
{
	if (ZoneComponent)
	{
		ZoneComponent->DeactivateImmediate();
	}
	return true;
}

// Niagara 以均匀 XY 缩放表达半径，Z 轴保持独立高度，兼顾顶视角和第三人称可读性。
bool AArenaGameplayCueNotify_BossFireZoneRadius::ConfigureAndActivate(
	const FGameplayCueParameters& Parameters)
{
	if (!ZoneComponent || !ZoneSystem)
	{
		return false;
	}

	const float RadiusScale = FMath::Max(
		Parameters.RawMagnitude / FMath::Max(ReferenceRadius, 1.0f),
		0.01f);
	SetActorLocation(FVector(Parameters.Location) + FVector(0.0f, 0.0f, VerticalOffset));
	SetActorRotation(FRotator::ZeroRotator);
	SetActorScale3D(FVector(RadiusScale, RadiusScale, FMath::Max(HeightScale, 0.01f)));
	ZoneComponent->SetAsset(ZoneSystem);
	ZoneComponent->Activate(true);
	return true;
}
