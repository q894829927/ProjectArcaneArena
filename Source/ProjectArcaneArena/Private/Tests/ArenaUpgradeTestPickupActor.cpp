#include "Tests/ArenaUpgradeTestPickupActor.h"

#include "AbilitySystemComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/ArenaPlayerCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Core/ArenaGameMode.h"
#include "Core/ArenaPlayerState.h"
#include "Core/ArenaUpgradeDataAsset.h"
#include "Engine/World.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
struct FUpgradePickupPresentation
{
	FName UpgradeID;
	const TCHAR* DisplayName;
	const TCHAR* Description;
	FColor DisplayColor;
};

// 使用默认 TextRender 字体支持的 ASCII 文案，避免中文 DataAsset 名称显示为乱码。
const FUpgradePickupPresentation* FindUpgradePickupPresentation(FName UpgradeID)
{
	static const FUpgradePickupPresentation Presentations[] =
	{
		{ FName(TEXT("Upgrade.AttackPower")), TEXT("Attack Power"), TEXT("Increase attack damage"), FColor(255, 105, 95) },
		{ FName(TEXT("Upgrade.MaxHealth")), TEXT("Max Health"), TEXT("Increase maximum health"), FColor(90, 235, 130) },
		{ FName(TEXT("Upgrade.MoveSpeed")), TEXT("Move Speed"), TEXT("Increase movement speed"), FColor(90, 220, 255) },
		{ FName(TEXT("Upgrade.Fireball.Damage")), TEXT("Fireball Damage"), TEXT("Fireball damage +20% per stack"), FColor(255, 145, 55) },
		{ FName(TEXT("Upgrade.Fireball.Burning")), TEXT("Burning Fireball"), TEXT("Fireball applies Burning"), FColor(255, 85, 40) },
		{ FName(TEXT("Upgrade.LightningStorm.Damage")), TEXT("Storm Damage"), TEXT("Storm damage +16% per stack"), FColor(85, 185, 255) },
		{ FName(TEXT("Upgrade.LightningStorm.Shocked")), TEXT("Shocked"), TEXT("Storm applies Shocked for 4s"), FColor(115, 125, 255) },
		{ FName(TEXT("Upgrade.Combo.Overload")), TEXT("Overload"), TEXT("Lightning on Burning explodes"), FColor(220, 90, 255) },
		{ FName(TEXT("Upgrade.Trigger.EnergyOnKill")), TEXT("Energy on Kill"), TEXT("Kills restore Energy"), FColor(75, 255, 205) },
		{ FName(TEXT("Upgrade.Crit.Chance")), TEXT("Critical Chance"), TEXT("Critical chance +5% per stack"), FColor(255, 220, 70) },
		{ FName(TEXT("Upgrade.Trigger.EnergyOnCrit")), TEXT("Energy on Crit"), TEXT("Critical hits restore Energy"), FColor(255, 175, 70) },
		{ FName(TEXT("Upgrade.Shield.Amount")), TEXT("Shield Amount"), TEXT("Shield amount +20% per stack"), FColor(100, 210, 255) },
		{ FName(TEXT("Upgrade.Trigger.ShieldBreakBlast")), TEXT("Shield Break Blast"), TEXT("Shield break creates a blast"), FColor(75, 130, 255) },
		{ FName(TEXT("Upgrade.Dash.Cooldown")), TEXT("Dash Cooldown"), TEXT("Dash cooldown -15% per stack"), FColor(190, 255, 255) },
		{ FName(TEXT("Upgrade.Dash.LightningTrail")), TEXT("Dash Lightning Trail"), TEXT("Dash leaves a lightning trail"), FColor(105, 155, 255) },
	};

	for (const FUpgradePickupPresentation& Presentation : Presentations)
	{
		if (Presentation.UpgradeID == UpgradeID)
		{
			return &Presentation;
		}
	}
	return nullptr;
}
}

// 创建常驻测试拾取物：球体负责触发，旋转网格和文字只负责识别。
AArenaUpgradeTestPickupActor::AArenaUpgradeTestPickupActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(false);

	PickupCollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("PickupCollisionComponent"));
	SetRootComponent(PickupCollisionComponent);
	PickupCollisionComponent->InitSphereRadius(85.0f);
	PickupCollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupCollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	PickupCollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupCollisionComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	PickupCollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	PickupCollisionComponent->SetGenerateOverlapEvents(true);
	PickupCollisionComponent->SetCanEverAffectNavigation(false);
	PickupCollisionComponent->OnComponentBeginOverlap.AddDynamic(
		this,
		&AArenaUpgradeTestPickupActor::HandlePickupOverlap);

	PickupMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMeshComponent"));
	PickupMeshComponent->SetupAttachment(PickupCollisionComponent);
	PickupMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupMeshComponent->SetGenerateOverlapEvents(false);
	PickupMeshComponent->SetCanEverAffectNavigation(false);
	PickupMeshComponent->SetRelativeScale3D(FVector(0.75f));
	PickupMeshComponent->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		PickupMeshComponent->SetStaticMesh(SphereMesh.Object);
	}

	PickupLabelComponent = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PickupLabelComponent"));
	PickupLabelComponent->SetupAttachment(PickupCollisionComponent);
	PickupLabelComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 150.0f));
	PickupLabelComponent->SetHorizontalAlignment(EHTA_Center);
	PickupLabelComponent->SetVerticalAlignment(EVRTA_TextCenter);
	PickupLabelComponent->SetWorldSize(32.0f);
	PickupLabelComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupLabelComponent->SetCastShadow(false);
	PickupLabelComponent->SetTranslucentSortPriority(11);
	PickupLabelComponent->SetText(FText::FromString(TEXT("Upgrade")));

	PickupDescriptionComponent = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PickupDescriptionComponent"));
	PickupDescriptionComponent->SetupAttachment(PickupCollisionComponent);
	PickupDescriptionComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 112.0f));
	PickupDescriptionComponent->SetHorizontalAlignment(EHTA_Center);
	PickupDescriptionComponent->SetVerticalAlignment(EVRTA_TextCenter);
	PickupDescriptionComponent->SetWorldSize(20.0f);
	PickupDescriptionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupDescriptionComponent->SetCastShadow(false);
	PickupDescriptionComponent->SetTranslucentSortPriority(10);
	PickupDescriptionComponent->SetText(FText::FromString(TEXT("Upgrade effect")));
	PickupDescriptionComponent->SetTextRenderColor(FColor(235, 235, 235));

	RotatingMovementComponent = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("RotatingMovementComponent"));
	RotatingMovementComponent->RotationRate = FRotator(0.0f, 75.0f, 0.0f);
	RotatingMovementComponent->SetUpdatedComponent(PickupMeshComponent);
}

// 每帧只更新本地文字朝向，服务器仍独占升级授予和堆叠判定。
void AArenaUpgradeTestPickupActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	FaceTextToLocalCamera();
}

// 编辑器改变实例 UpgradeData 后立即重建标签，无需运行 PIE。
void AArenaUpgradeTestPickupActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshPickupPresentation();
}

// 让 Listen Server 和单机实例使用已序列化的 UpgradeData 初始化文字，避免只依赖客户端 OnRep。
void AArenaUpgradeTestPickupActor::BeginPlay()
{
	Super::BeginPlay();
	RefreshPickupPresentation();
	FaceTextToLocalCamera();
}

// 注册 UpgradeData 引用，放置或未来动态生成的测试拾取物都可正确显示。
void AArenaUpgradeTestPickupActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AArenaUpgradeTestPickupActor, UpgradeData);
}

// 仅权威端调用测试 GameMode 的正式升级管线，失败时保留道具便于补齐依赖后再拾取。
void AArenaUpgradeTestPickupActor::HandlePickupOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!HasAuthority() || !UpgradeData)
	{
		return;
	}

	AArenaPlayerCharacter* PlayerCharacter = Cast<AArenaPlayerCharacter>(OtherActor);
	AArenaPlayerState* ArenaPlayerState = PlayerCharacter
		? PlayerCharacter->GetPlayerState<AArenaPlayerState>()
		: nullptr;
	UAbilitySystemComponent* PlayerASC = PlayerCharacter
		? PlayerCharacter->GetAbilitySystemComponent()
		: nullptr;
	if (!ArenaPlayerState || !PlayerASC
		|| PlayerASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead))
	{
		return;
	}

	const double CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const TWeakObjectPtr<AArenaPlayerState> PlayerKey(ArenaPlayerState);
	const double* LastGrantTime = LastGrantTimeByPlayer.Find(PlayerKey);
	if (LastGrantTime && CurrentTime - *LastGrantTime < MinimumGrantInterval)
	{
		return;
	}

#if WITH_EDITOR
	AArenaGameMode* ArenaGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AArenaGameMode>() : nullptr;
	if (ArenaGameMode && ArenaGameMode->TryGrantDebugUpgrade(ArenaPlayerState, UpgradeData))
	{
		LastGrantTimeByPlayer.FindOrAdd(PlayerKey) = CurrentTime;
	}
#endif
}

// 客户端收到 UpgradeData 后只更新可视化，不在本地授予任何玩法状态。
void AArenaUpgradeTestPickupActor::OnRep_UpgradeData()
{
	RefreshPickupPresentation();
}

// 使用 ASCII 名称、简短效果和构筑颜色区分测试道具，空引用显示明确占位文字。
void AArenaUpgradeTestPickupActor::RefreshPickupPresentation()
{
	if (!PickupLabelComponent || !PickupDescriptionComponent)
	{
		return;
	}

	if (!UpgradeData)
	{
		PickupLabelComponent->SetText(FText::FromString(TEXT("Missing Upgrade")));
		PickupDescriptionComponent->SetText(FText::FromString(TEXT("Assign UpgradeData")));
		ApplyPickupDisplayColor(FColor::Red);
		return;
	}

	const FUpgradePickupPresentation* Presentation = FindUpgradePickupPresentation(UpgradeData->UpgradeID);
	if (Presentation)
	{
		PickupLabelComponent->SetText(FText::FromString(Presentation->DisplayName));
		PickupDescriptionComponent->SetText(FText::FromString(Presentation->Description));
		ApplyPickupDisplayColor(Presentation->DisplayColor);
		return;
	}

	PickupLabelComponent->SetText(FText::FromString(UpgradeData->UpgradeID.ToString()));
	PickupDescriptionComponent->SetText(FText::FromString(TEXT("Inspect UpgradeData for effect")));
	ApplyPickupDisplayColor(FColor::White);
}

// 使用本地 PlayerCameraManager 计算文字正面方向，Dedicated Server 没有本地相机时自然跳过。
void AArenaUpgradeTestPickupActor::FaceTextToLocalCamera()
{
	if (!PickupLabelComponent || !PickupDescriptionComponent || !GetWorld())
	{
		return;
	}

	APlayerController* LocalPlayerController = GetWorld()->GetFirstPlayerController();
	if (!LocalPlayerController || !LocalPlayerController->IsLocalController()
		|| !LocalPlayerController->PlayerCameraManager)
	{
		return;
	}

	const FVector CameraLocation = LocalPlayerController->PlayerCameraManager->GetCameraLocation();
	const FVector CameraDirection = CameraLocation - PickupLabelComponent->GetComponentLocation();
	if (CameraDirection.IsNearlyZero())
	{
		return;
	}

	const FRotator FacingRotation = CameraDirection.Rotation();
	PickupLabelComponent->SetWorldRotation(FacingRotation);
	PickupDescriptionComponent->SetWorldRotation(FacingRotation);
}

// 为当前 Actor 创建独立 MID，并让球体与文字共享同一测试颜色。
void AArenaUpgradeTestPickupActor::ApplyPickupDisplayColor(const FColor& DisplayColor)
{
	if (PickupLabelComponent)
	{
		PickupLabelComponent->SetTextRenderColor(DisplayColor);
	}
	if (PickupDescriptionComponent)
	{
		PickupDescriptionComponent->SetTextRenderColor(DisplayColor);
	}

	if (!PickupMeshComponent || !GetWorld() || !GetWorld()->IsGameWorld())
	{
		return;
	}
	if (!PickupMaterialInstance)
	{
		PickupMaterialInstance = PickupMeshComponent->CreateAndSetMaterialInstanceDynamic(0);
	}
	if (PickupMaterialInstance)
	{
		PickupMaterialInstance->SetVectorParameterValue(
			TEXT("Color"),
			FLinearColor::FromSRGBColor(DisplayColor));
	}
}
