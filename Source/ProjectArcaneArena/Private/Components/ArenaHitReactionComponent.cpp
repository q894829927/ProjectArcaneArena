#include "Components/ArenaHitReactionComponent.h"

#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/ArenaCharacterBase.h"
#include "Character/ArenaPlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/ArenaPlayerController.h"
#include "Engine/World.h"
#include "GAS/ArenaGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "UI/ArenaDamageNumberActor.h"

UArenaHitReactionComponent::UArenaHitReactionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// 统一消费一段复制反馈；世界表现对各端可见，本地 HUD 和相机仍受所有权限制。
void UArenaHitReactionComponent::PresentDamageFeedback(const FArenaDamageFeedbackData& DamageFeedback)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor
		|| OwnerActor->GetNetMode() == NM_DedicatedServer
		|| DamageFeedback.FeedbackType == EArenaDamageFeedbackType::None
		|| DamageFeedback.GetTotalDamage() <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	ApplyMaterialFlash(DamageFeedback.FeedbackType);
	PlayFeedbackSounds(DamageFeedback.FeedbackType);
	SpawnDamageNumber(
		DamageFeedback.GetTotalDamage(),
		DamageFeedback.CueParameters.AggregatedSourceTags.HasTagExact(ArenaGameplayTags::Damage_Critical),
		DamageFeedback.FeedbackType);
	PresentLocalPlayerFeedback(DamageFeedback);
}

// 使用单一 Actor/Widget 管线生成数字，并用循环槽位错开同 Tick 的重叠反馈。
void UArenaHitReactionComponent::SpawnDamageNumber(
	float DamageAmount,
	bool bCriticalHit,
	EArenaDamageFeedbackType FeedbackType)
{
	AActor* OwnerActor = GetOwner();
	const TSubclassOf<AArenaDamageNumberActor> NumberActorClass = ResolveDamageNumberClass();
	if (!OwnerActor
		|| OwnerActor->GetNetMode() == NM_DedicatedServer
		|| DamageAmount <= KINDA_SMALL_NUMBER
		|| !NumberActorClass)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	static constexpr int32 LaneCount = 5;
	const int32 LaneIndex = DamageNumberSequence++ % LaneCount;
	const float CenteredLane = static_cast<float>(LaneIndex) - static_cast<float>(LaneCount - 1) * 0.5f;
	const FVector LaneOffset = OwnerActor->GetActorRightVector() * CenteredLane * DamageNumberLaneSpacing;

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = OwnerActor;
	SpawnParameters.Instigator = Cast<APawn>(OwnerActor);
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AArenaDamageNumberActor* DamageNumberActor = World->SpawnActor<AArenaDamageNumberActor>(
		NumberActorClass,
		OwnerActor->GetActorLocation() + ResolveDamageNumberOffset() + LaneOffset,
		FRotator::ZeroRotator,
		SpawnParameters);
	if (DamageNumberActor)
	{
		DamageNumberActor->SetDamageFeedbackPresentation(DamageAmount, bCriticalHit, FeedbackType);
	}
}

// 结束时恢复共享材质参数，避免切图或销毁期间留下非零闪烁状态。
void UArenaHitReactionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(HitFlashTimerHandle);
	}
	ResetMaterialFlash();
	DynamicMaterials.Reset();
	Super::EndPlay(EndPlayReason);
}

// 延迟到第一次受击再创建 MID，且 Dedicated Server 永远不分配表现对象。
void UArenaHitReactionComponent::EnsureDynamicMaterials()
{
	if (!DynamicMaterials.IsEmpty())
	{
		return;
	}

	const ACharacter* CharacterOwner = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* MeshComponent = CharacterOwner ? CharacterOwner->GetMesh() : nullptr;
	if (!MeshComponent || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const int32 MaterialCount = MeshComponent->GetNumMaterials();
	for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
	{
		if (UMaterialInstanceDynamic* DynamicMaterial = MeshComponent->CreateAndSetMaterialInstanceDynamic(MaterialIndex))
		{
			DynamicMaterials.Add(DynamicMaterial);
		}
	}
}

// ShieldOnly 保持青色，HealthOnly 使用红色，复合伤害以高亮破盾色为主。
void UArenaHitReactionComponent::ApplyMaterialFlash(EArenaDamageFeedbackType FeedbackType)
{
	EnsureDynamicMaterials();
	if (DynamicMaterials.IsEmpty())
	{
		return;
	}

	FLinearColor FlashColor = HealthHitFlashColor;
	float FlashIntensity = HealthHitFlashIntensity;
	float FlashDuration = HealthHitFlashDuration;
	switch (FeedbackType)
	{
	case EArenaDamageFeedbackType::ShieldOnly:
		FlashColor = ShieldHitFlashColor;
		FlashIntensity = ShieldHitFlashIntensity;
		FlashDuration = ShieldHitFlashDuration;
		break;
	case EArenaDamageFeedbackType::ShieldBreak:
	case EArenaDamageFeedbackType::ShieldBreakWithHealthDamage:
		FlashColor = ShieldBreakFlashColor;
		FlashIntensity = ShieldBreakFlashIntensity;
		FlashDuration = ShieldBreakFlashDuration;
		break;
	case EArenaDamageFeedbackType::HealthOnly:
		break;
	default:
		return;
	}

	for (UMaterialInstanceDynamic* DynamicMaterial : DynamicMaterials)
	{
		if (DynamicMaterial)
		{
			DynamicMaterial->SetVectorParameterValue(HitFlashColorParameterName, FlashColor);
			DynamicMaterial->SetScalarParameterValue(HitFlashIntensityParameterName, FlashIntensity);
		}
	}

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			HitFlashTimerHandle,
			this,
			&UArenaHitReactionComponent::ResetMaterialFlash,
			FMath::Max(FlashDuration, KINDA_SMALL_NUMBER),
			false);
	}
}

// MID 持续复用，只把统一强度参数清零。
void UArenaHitReactionComponent::ResetMaterialFlash()
{
	for (UMaterialInstanceDynamic* DynamicMaterial : DynamicMaterials)
	{
		if (DynamicMaterial)
		{
			DynamicMaterial->SetScalarParameterValue(HitFlashIntensityParameterName, 0.0f);
		}
	}
}

// 仅本地控制玩家能影响自己的 HUD 和 PlayerCameraManager，远程 Pawn 不进入该路径。
void UArenaHitReactionComponent::PresentLocalPlayerFeedback(const FArenaDamageFeedbackData& DamageFeedback)
{
	AArenaPlayerCharacter* PlayerCharacter = Cast<AArenaPlayerCharacter>(GetOwner());
	if (!PlayerCharacter || !PlayerCharacter->IsLocallyControlled())
	{
		return;
	}

	AArenaPlayerController* PlayerController = Cast<AArenaPlayerController>(PlayerCharacter->GetController());
	if (!PlayerController)
	{
		return;
	}

	float CameraShakeScale = 0.0f;
	TSubclassOf<UCameraShakeBase> CameraShakeClass;
	if (DamageFeedback.FeedbackType == EArenaDamageFeedbackType::ShieldOnly)
	{
		CameraShakeScale = ShieldHitCameraShakeScale;
		CameraShakeClass = LightDamageCameraShakeClass;
	}
	else if (DamageFeedback.FeedbackType == EArenaDamageFeedbackType::ShieldBreak)
	{
		CameraShakeScale = ShieldBreakBaseShakeScale;
		CameraShakeClass = ShieldBreakCameraShakeClass
			? ShieldBreakCameraShakeClass
			: MediumDamageCameraShakeClass;
	}
	else if (DamageFeedback.FeedbackType == EArenaDamageFeedbackType::ShieldBreakWithHealthDamage)
	{
		CameraShakeScale = ShieldBreakBaseShakeScale
			+ DamageFeedback.HealthDamageRatio * HealthDamageShakeMultiplier;
		CameraShakeClass = ShieldBreakCameraShakeClass
			? ShieldBreakCameraShakeClass
			: ResolveHealthCameraShake(DamageFeedback.HealthDamageRatio);
	}
	else if (DamageFeedback.FeedbackType == EArenaDamageFeedbackType::HealthOnly)
	{
		CameraShakeScale = DamageFeedback.HealthDamageRatio * HealthDamageShakeMultiplier;
		CameraShakeClass = ResolveHealthCameraShake(DamageFeedback.HealthDamageRatio);
	}
	CameraShakeScale = FMath::Clamp(CameraShakeScale, 0.0f, MaxCameraShakeScale);

	if (CameraShakeClass && CameraShakeScale > KINDA_SMALL_NUMBER && PlayerController->PlayerCameraManager)
	{
		PlayerController->PlayerCameraManager->StartCameraShake(CameraShakeClass, CameraShakeScale);
	}
	PlayerController->ShowLocalDamageFeedback(DamageFeedback, CameraShakeScale);
}

// 按服务器确认的 Health 损失比例选择表现等级，比例本身不参与玩法结算。
TSubclassOf<UCameraShakeBase> UArenaHitReactionComponent::ResolveHealthCameraShake(float HealthDamageRatio) const
{
	if (HealthDamageRatio < 0.05f)
	{
		return LightDamageCameraShakeClass;
	}
	if (HealthDamageRatio < 0.15f)
	{
		return MediumDamageCameraShakeClass;
	}
	return HeavyDamageCameraShakeClass;
}

// 复合伤害叠加破盾高频层与生命低频层，其余分类只播放一个对应音层。
void UArenaHitReactionComponent::PlayFeedbackSounds(EArenaDamageFeedbackType FeedbackType) const
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !GetWorld())
	{
		return;
	}

	auto PlaySound = [this, OwnerActor](USoundBase* Sound)
	{
		if (Sound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, Sound, OwnerActor->GetActorLocation());
		}
	};

	switch (FeedbackType)
	{
	case EArenaDamageFeedbackType::ShieldOnly:
		PlaySound(ShieldHitSound);
		break;
	case EArenaDamageFeedbackType::ShieldBreak:
		PlaySound(ShieldBreakSound);
		break;
	case EArenaDamageFeedbackType::HealthOnly:
		PlaySound(HealthHitSound);
		break;
	case EArenaDamageFeedbackType::ShieldBreakWithHealthDamage:
		PlaySound(ShieldBreakSound);
		PlaySound(HealthHitSound);
		break;
	default:
		break;
	}
}

// 优先使用组件统一配置，未迁移的敌人蓝图继续从角色兼容属性读取。
TSubclassOf<AArenaDamageNumberActor> UArenaHitReactionComponent::ResolveDamageNumberClass() const
{
	if (DamageNumberActorClass)
	{
		return DamageNumberActorClass;
	}

	const AArenaCharacterBase* CharacterOwner = Cast<AArenaCharacterBase>(GetOwner());
	return CharacterOwner ? CharacterOwner->GetDamageNumberActorClassForFeedback() : nullptr;
}

// 组件自定义偏移优先；敌人兼容配置可继续保留已有头顶高度。
FVector UArenaHitReactionComponent::ResolveDamageNumberOffset() const
{
	const AArenaCharacterBase* CharacterOwner = Cast<AArenaCharacterBase>(GetOwner());
	return CharacterOwner
		? CharacterOwner->GetDamageNumberSpawnOffsetForFeedback(DamageNumberSpawnOffset)
		: DamageNumberSpawnOffset;
}
