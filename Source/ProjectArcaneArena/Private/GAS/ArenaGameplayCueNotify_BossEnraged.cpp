#include "GAS/ArenaGameplayCueNotify_BossEnraged.h"

#include "Components/SceneComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

// 狂暴 Cue 使用独立 Actor 附着 Boss，避免世界落点参数把表现留在旧位置。
AArenaGameplayCueNotify_BossEnraged::AArenaGameplayCueNotify_BossEnraged()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickInterval = 0.05f;
	bAutoAttachToOwner = true;
	bAutoDestroyOnRemove = true;

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	EnrageComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("EnrageComponent"));
	EnrageComponent->SetupAttachment(SceneRoot);
	EnrageComponent->SetAutoActivate(false);
}

// 非循环 Niagara 完成后按低频间隔重启，完整覆盖 Infinite GE 的持续时间。
void AArenaGameplayCueNotify_BossEnraged::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bCuePresentationActive || !bRestartSystemWhileActive || !EnrageComponent || !EnrageSystem)
	{
		return;
	}

	SystemReplayElapsedTime += DeltaSeconds;
	const float SafeReplayInterval = FMath::Max(SystemReplayInterval, 0.05f);
	if (!EnrageComponent->IsActive() || SystemReplayElapsedTime >= SafeReplayInterval)
	{
		EnrageComponent->SetAsset(EnrageSystem);
		EnrageComponent->Activate(true);
		SystemReplayElapsedTime = 0.0f;
	}
}

// 首次 Cue 激活和晚加入同步共用同一附着配置路径。
bool AArenaGameplayCueNotify_BossEnraged::OnActive_Implementation(
	AActor* MyTarget,
	const FGameplayCueParameters& Parameters)
{
	return ConfigureAndActivate(MyTarget);
}

// WhileActive 重新应用相对 Transform，确保复制顺序不同的客户端仍贴合 Boss。
bool AArenaGameplayCueNotify_BossEnraged::WhileActive_Implementation(
	AActor* MyTarget,
	const FGameplayCueParameters& Parameters)
{
	return ConfigureAndActivate(MyTarget);
}

// 移除持续 Cue 时停止重播和粒子组件，再由 GameplayCueManager 回收 Actor。
bool AArenaGameplayCueNotify_BossEnraged::OnRemove_Implementation(
	AActor* MyTarget,
	const FGameplayCueParameters& Parameters)
{
	bCuePresentationActive = false;
	SystemReplayElapsedTime = 0.0f;
	SetActorTickEnabled(false);
	if (EnrageComponent)
	{
		EnrageComponent->DeactivateImmediate();
	}
	return true;
}

// 附着到目标根组件并使用相对缩放，避免 Cue 参数中的世界位置影响持续跟随。
bool AArenaGameplayCueNotify_BossEnraged::ConfigureAndActivate(AActor* MyTarget)
{
	if (!IsValid(MyTarget) || !MyTarget->GetRootComponent() || !EnrageComponent || !EnrageSystem)
	{
		return false;
	}

	AttachToComponent(MyTarget->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	SetActorRelativeLocation(FVector(0.0f, 0.0f, VerticalOffset));
	SetActorRelativeRotation(FRotator::ZeroRotator);
	SetActorRelativeScale3D(EnrageScale);
	EnrageComponent->SetAsset(EnrageSystem);
	EnrageComponent->Activate(true);
	bCuePresentationActive = true;
	SystemReplayElapsedTime = 0.0f;
	SetActorTickEnabled(bRestartSystemWhileActive);
	return true;
}
