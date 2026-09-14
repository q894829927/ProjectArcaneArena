#include "GAS/ArenaGameplayCueNotify_BossChargeTelegraph.h"

#include "Components/SceneComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

// 原生 Cue Actor 负责把网络参数转换为固定世界空间预警线，蓝图只配置 Niagara 资产。
AArenaGameplayCueNotify_BossChargeTelegraph::AArenaGameplayCueNotify_BossChargeTelegraph()
{
	PrimaryActorTick.bCanEverTick = false;
	bAutoAttachToOwner = false;
	bAutoDestroyOnRemove = true;

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	TelegraphComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("TelegraphComponent"));
	TelegraphComponent->SetupAttachment(SceneRoot);
	TelegraphComponent->SetAutoActivate(false);
}

// OnActive 与 WhileActive 使用相同参数路径，保证首次激活和中途同步外观一致。
bool AArenaGameplayCueNotify_BossChargeTelegraph::OnActive_Implementation(
	AActor* MyTarget,
	const FGameplayCueParameters& Parameters)
{
	return ConfigureAndActivate(Parameters);
}

// 客户端晚收到持续 Cue 时重新应用固定 Transform，不依赖 Boss 当前所在位置。
bool AArenaGameplayCueNotify_BossChargeTelegraph::WhileActive_Implementation(
	AActor* MyTarget,
	const FGameplayCueParameters& Parameters)
{
	return ConfigureAndActivate(Parameters);
}

// 预警兑现或 Ability 取消后立刻停掉粒子，避免持续 Cue 留在场景中。
bool AArenaGameplayCueNotify_BossChargeTelegraph::OnRemove_Implementation(
	AActor* MyTarget,
	const FGameplayCueParameters& Parameters)
{
	if (TelegraphComponent)
	{
		TelegraphComponent->DeactivateImmediate();
	}
	return true;
}

// Niagara 默认以本地 X 轴表示长度，并独立放大横截面，保证顶视角和第三人称都能读清预警。
bool AArenaGameplayCueNotify_BossChargeTelegraph::ConfigureAndActivate(
	const FGameplayCueParameters& Parameters)
{
	if (!TelegraphComponent || !TelegraphSystem)
	{
		return false;
	}

	FVector Direction = FVector(Parameters.Normal.X, Parameters.Normal.Y, 0.0f).GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		Direction = FVector::ForwardVector;
	}

	SetActorLocation(FVector(Parameters.Location) + FVector(0.0f, 0.0f, VerticalOffset));
	SetActorRotation(Direction.Rotation());
	SetActorScale3D(FVector(
		FMath::Max(Parameters.RawMagnitude / FMath::Max(ReferenceLength, 1.0f), 0.01f),
		FMath::Max(WidthScale, 0.01f),
		FMath::Max(HeightScale, 0.01f)));
	TelegraphComponent->SetAsset(TelegraphSystem);
	TelegraphComponent->Activate(true);
	return true;
}
