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
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "UI/ArenaDamageNumberActor.h"

UArenaHitReactionComponent::UArenaHitReactionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// 保留单条反馈兼容入口，并交给批次路径统一执行角色反应。
void UArenaHitReactionComponent::PresentDamageFeedback(const FArenaDamageFeedbackData& DamageFeedback)
{
	TArray<FArenaDamageFeedbackData> SingleFeedbackBatch;
	SingleFeedbackBatch.Add(DamageFeedback);
	PresentDamageFeedbackBatch(SingleFeedbackBatch);
	PresentDamageFeedbackSound(DamageFeedback.FeedbackType);
}

// 每段伤害保留独立数字，同 Tick 的闪光、CameraShake 和 HUD 只汇总播放一次；结果音由可靠 RPC 独立派发。
void UArenaHitReactionComponent::PresentDamageFeedbackBatch(
	const TArray<FArenaDamageFeedbackData>& DamageFeedbackBatch)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || OwnerActor->GetNetMode() == NM_DedicatedServer || DamageFeedbackBatch.IsEmpty())
	{
		return;
	}

	float TotalShieldDamage = 0.0f;
	float TotalHealthDamage = 0.0f;
	float TotalHealthDamageRatio = 0.0f;
	bool bBrokeShield = false;
	const FArenaDamageFeedbackData* DominantHealthFeedback = nullptr;
	const FArenaDamageFeedbackData* DominantTotalFeedback = nullptr;
	float DominantHealthDamage = -1.0f;
	float DominantTotalDamage = -1.0f;

	for (const FArenaDamageFeedbackData& DamageFeedback : DamageFeedbackBatch)
	{
		if (DamageFeedback.FeedbackType == EArenaDamageFeedbackType::None
			|| DamageFeedback.GetTotalDamage() <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		TotalShieldDamage += FMath::Max(DamageFeedback.ActualShieldDamage, 0.0f);
		TotalHealthDamage += FMath::Max(DamageFeedback.ActualHealthDamage, 0.0f);
		TotalHealthDamageRatio += FMath::Max(DamageFeedback.HealthDamageRatio, 0.0f);
		bBrokeShield |= DamageFeedback.FeedbackType == EArenaDamageFeedbackType::ShieldBreak
			|| DamageFeedback.FeedbackType == EArenaDamageFeedbackType::ShieldBreakWithHealthDamage;

		// 每段结算仍生成自己的数字和暴击样式，批次只合并角色反应层。
		SpawnDamageNumber(
			DamageFeedback.GetTotalDamage(),
			DamageFeedback.CueParameters.AggregatedSourceTags.HasTagExact(ArenaGameplayTags::Damage_Critical),
			DamageFeedback.FeedbackType);

		const bool bHasBetterTotalSource = DamageFeedback.bHasDamageSourceLocation
			&& DominantTotalFeedback
			&& !DominantTotalFeedback->bHasDamageSourceLocation
			&& FMath::IsNearlyEqual(DamageFeedback.GetTotalDamage(), DominantTotalDamage);
		if (!DominantTotalFeedback
			|| DamageFeedback.GetTotalDamage() > DominantTotalDamage
			|| bHasBetterTotalSource)
		{
			DominantTotalFeedback = &DamageFeedback;
			DominantTotalDamage = DamageFeedback.GetTotalDamage();
		}

		const bool bHasBetterHealthSource = DamageFeedback.bHasDamageSourceLocation
			&& DominantHealthFeedback
			&& !DominantHealthFeedback->bHasDamageSourceLocation
			&& FMath::IsNearlyEqual(DamageFeedback.ActualHealthDamage, DominantHealthDamage);
		if (DamageFeedback.ActualHealthDamage > KINDA_SMALL_NUMBER
			&& (!DominantHealthFeedback
				|| DamageFeedback.ActualHealthDamage > DominantHealthDamage
				|| bHasBetterHealthSource))
		{
			DominantHealthFeedback = &DamageFeedback;
			DominantHealthDamage = DamageFeedback.ActualHealthDamage;
		}
	}

	const FArenaDamageFeedbackData* DominantFeedback = TotalHealthDamage > KINDA_SMALL_NUMBER
		? DominantHealthFeedback
		: DominantTotalFeedback;
	if (!DominantFeedback || TotalShieldDamage + TotalHealthDamage <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	FArenaDamageFeedbackData AggregatedFeedback = *DominantFeedback;
	AggregatedFeedback.ActualShieldDamage = TotalShieldDamage;
	AggregatedFeedback.ActualHealthDamage = TotalHealthDamage;
	AggregatedFeedback.HealthDamageRatio = FMath::Clamp(TotalHealthDamageRatio, 0.0f, 1.0f);
	if (TotalHealthDamage > KINDA_SMALL_NUMBER)
	{
		AggregatedFeedback.FeedbackType = bBrokeShield
			? EArenaDamageFeedbackType::ShieldBreakWithHealthDamage
			: EArenaDamageFeedbackType::HealthOnly;
	}
	else
	{
		AggregatedFeedback.FeedbackType = bBrokeShield
			? EArenaDamageFeedbackType::ShieldBreak
			: EArenaDamageFeedbackType::ShieldOnly;
	}

	ApplyMaterialFlash(AggregatedFeedback.FeedbackType);
	PresentLocalPlayerFeedback(AggregatedFeedback);
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

// 结束时恢复原 Overlay 并清理表现对象，避免切图或销毁期间残留闪光。
void UArenaHitReactionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(HitFlashTimerHandle);
	}
	ResetMaterialFlash();
	HitFlashMaterialInstance = nullptr;
	OriginalOverlayMaterial = nullptr;
	CachedHitFlashMesh = nullptr;
	Super::EndPlay(EndPlayReason);
}

// 延迟到第一次受击再创建项目 Overlay MID，Dedicated Server 永远不分配表现对象。
void UArenaHitReactionComponent::EnsureHitFlashOverlay()
{
	if (HitFlashMaterialInstance)
	{
		return;
	}

	const ACharacter* CharacterOwner = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* MeshComponent = CharacterOwner ? CharacterOwner->GetMesh() : nullptr;
	if (!MeshComponent || !HitFlashOverlayMaterial || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	CachedHitFlashMesh = MeshComponent;
	HitFlashMaterialInstance = UMaterialInstanceDynamic::Create(HitFlashOverlayMaterial.Get(), this);
}

// ShieldOnly 保持青色，HealthOnly 使用红色，复合伤害以高亮破盾 Overlay 为主。
void UArenaHitReactionComponent::ApplyMaterialFlash(EArenaDamageFeedbackType FeedbackType)
{
	EnsureHitFlashOverlay();
	if (!CachedHitFlashMesh || !HitFlashMaterialInstance)
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

	// 每次从非伤害 Overlay 进入闪光时重新捕获，兼容其他本地表现系统临时替换 Overlay。
	if (!bHitFlashOverlayApplied)
	{
		OriginalOverlayMaterial = CachedHitFlashMesh->GetOverlayMaterial();
	}
	HitFlashMaterialInstance->SetVectorParameterValue(HitFlashColorParameterName, FlashColor);
	HitFlashMaterialInstance->SetScalarParameterValue(HitFlashIntensityParameterName, FlashIntensity);
	CachedHitFlashMesh->SetOverlayMaterial(HitFlashMaterialInstance.Get());
	bHitFlashOverlayApplied = true;

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

// 清零复用 MID，并且只在当前仍为伤害 Overlay 时恢复进入闪光前的材质。
void UArenaHitReactionComponent::ResetMaterialFlash()
{
	if (HitFlashMaterialInstance)
	{
		HitFlashMaterialInstance->SetScalarParameterValue(HitFlashIntensityParameterName, 0.0f);
	}
	if (bHitFlashOverlayApplied
		&& CachedHitFlashMesh
		&& CachedHitFlashMesh->GetOverlayMaterial() == HitFlashMaterialInstance.Get())
	{
		CachedHitFlashMesh->SetOverlayMaterial(OriginalOverlayMaterial.Get());
	}
	bHitFlashOverlayApplied = false;
	OriginalOverlayMaterial = nullptr;
}

// 仅本地控制玩家能影响自己的 HUD 和 PlayerCameraManager，第三人称额外衰减震屏幅度。
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
	if (PlayerCharacter->IsUsingThirdPersonView())
	{
		CameraShakeScale *= ThirdPersonCameraShakeScaleMultiplier;
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

// 播放服务器汇总后的单一结果音层，并覆盖第三方 SoundCue 的短距离衰减以适配竞技场镜头。
void UArenaHitReactionComponent::PresentDamageFeedbackSound(EArenaDamageFeedbackType FeedbackType) const
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
			UGameplayStatics::PlaySoundAtLocation(
				this,
				Sound,
				OwnerActor->GetActorLocation(),
				1.0f,
				1.0f,
				0.0f,
				HitFeedbackAttenuationSettings.Get());
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
