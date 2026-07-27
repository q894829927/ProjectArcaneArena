#include "Item/ArenaInventoryPickupActor.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Core/ArenaGameState.h"
#include "Core/ArenaPlayerState.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "GAS/ArenaGameplayTags.h"
#include "Item/ArenaInventoryComponent.h"
#include "Item/ArenaItemDataAsset.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

// 创建可复制的交互 Pickup，并提供原生球体、旋转和世界名称占位表现。
AArenaInventoryPickupActor::AArenaInventoryPickupActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	bReplicates = true;
	SetReplicateMovement(true);

	PickupCollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("PickupCollisionComponent"));
	SetRootComponent(PickupCollisionComponent);
	PickupCollisionComponent->InitSphereRadius(45.0f);
	PickupCollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupCollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	PickupCollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupCollisionComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	PickupCollisionComponent->SetGenerateOverlapEvents(false);
	PickupCollisionComponent->SetCanEverAffectNavigation(false);

	PickupMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMeshComponent"));
	PickupMeshComponent->SetupAttachment(PickupCollisionComponent);
	PickupMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupMeshComponent->SetGenerateOverlapEvents(false);
	PickupMeshComponent->SetRelativeScale3D(FVector(0.25f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		PickupMeshComponent->SetStaticMesh(SphereMesh.Object);
	}

	RotatingMovementComponent = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("RotatingMovementComponent"));
	RotatingMovementComponent->RotationRate = FRotator(0.0f, 90.0f, 0.0f);
	RotatingMovementComponent->SetUpdatedComponent(PickupMeshComponent);

	PickupLabelComponent = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PickupLabelComponent"));
	PickupLabelComponent->SetupAttachment(PickupCollisionComponent);
	PickupLabelComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 75.0f));
	PickupLabelComponent->SetHorizontalAlignment(EHTA_Center);
	PickupLabelComponent->SetVerticalAlignment(EVRTA_TextCenter);
	PickupLabelComponent->SetWorldSize(22.0f);
	PickupLabelComponent->SetTextRenderColor(FColor::White);
	PickupLabelComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupLabelComponent->SetCastShadow(false);
	PickupLabelComponent->SetTranslucentSortPriority(10);
}

// 复制 ItemData、数量和短暂拾取保护，世界 Actor 的可见状态由服务器统一发布。
void AArenaInventoryPickupActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AArenaInventoryPickupActor, ItemData);
	DOREPLIFETIME(AArenaInventoryPickupActor, Quantity);
	DOREPLIFETIME(AArenaInventoryPickupActor, IgnoredPlayerState);
	DOREPLIFETIME(AArenaInventoryPickupActor, IgnoreUntilServerTime);
}

// 只更新本地文字朝向；旋转组件独立驱动物品球体。
void AArenaInventoryPickupActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	FaceLabelToLocalCamera();
}

// 编辑器实例修改 ItemData 或数量后立即刷新标签，便于直接检查关卡摆放。
void AArenaInventoryPickupActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshPickupPresentation();
}

// 启动服务器寿命并用当前复制数据初始化本地标签。
void AArenaInventoryPickupActor::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() == NM_DedicatedServer)
	{
		SetActorTickEnabled(false);
		if (RotatingMovementComponent)
		{
			RotatingMovementComponent->SetComponentTickEnabled(false);
		}
	}
	if (HasAuthority() && PickupLifeSpan > 0.0f)
	{
		SetLifeSpan(PickupLifeSpan);
	}
	RefreshPickupPresentation();
	FaceLabelToLocalCamera();
}

// Deferred Spawn 阶段只接受权威、有效物品和正数量，避免生成可复制的空 Pickup。
bool AArenaInventoryPickupActor::InitializePickup(
	UArenaItemDataAsset* InItemData,
	int32 InQuantity,
	AArenaPlayerState* InIgnoredPlayerState,
	float IgnoreDuration)
{
	if (!HasAuthority() || !InItemData || !InItemData->ItemTag.IsValid() || InQuantity <= 0)
	{
		return false;
	}

	ItemData = InItemData;
	Quantity = InQuantity;
	IgnoredPlayerState = InIgnoredPlayerState;
	const AArenaGameState* ArenaGameState = GetWorld() ? GetWorld()->GetGameState<AArenaGameState>() : nullptr;
	const float ServerTime = ArenaGameState ? ArenaGameState->GetServerWorldTimeSeconds() : 0.0f;
	IgnoreUntilServerTime = ServerTime + FMath::Max(IgnoreDuration, 0.0f);
	RefreshPickupPresentation();
	return true;
}

// 验证物品数据、数量和丢弃者保护；距离与视线由 Controller 单独检查。
bool AArenaInventoryPickupActor::CanBeInteractedBy(const AArenaPlayerState* PlayerState) const
{
	if (bConsumed || !PlayerState || !ItemData || Quantity <= 0)
	{
		return false;
	}

	if (IgnoredPlayerState == PlayerState)
	{
		const AArenaGameState* ArenaGameState = GetWorld() ? GetWorld()->GetGameState<AArenaGameState>() : nullptr;
		const float ServerTime = ArenaGameState ? ArenaGameState->GetServerWorldTimeSeconds() : 0.0f;
		if (ServerTime < IgnoreUntilServerTime)
		{
			return false;
		}
	}
	return true;
}

// Authority 使用消费门闩处理多人竞争，完整加入后再销毁世界 Actor。
bool AArenaInventoryPickupActor::TryCollect(AArenaPlayerState* PlayerState)
{
	if (!HasAuthority() || !CanBeInteractedBy(PlayerState))
	{
		return false;
	}

	UArenaInventoryComponent* InventoryComponent = PlayerState->GetInventoryComponent();
	if (!InventoryComponent || !InventoryComponent->TryAddItem(ItemData, Quantity))
	{
		return false;
	}

	bConsumed = true;
	PickupCollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Destroy();
	return true;
}

// ItemData 或数量复制变化时刷新世界标签。
void AArenaInventoryPickupActor::OnRep_PickupData()
{
	RefreshPickupPresentation();
}

// 使用 DataAsset 的玩家可见名称和数量构造无需额外 WBP 的原型标签。
void AArenaInventoryPickupActor::RefreshPickupPresentation()
{
	if (!PickupLabelComponent)
	{
		return;
	}

	if (!ItemData || Quantity <= 0)
	{
		PickupLabelComponent->SetText(FText::GetEmpty());
		return;
	}

	PickupLabelComponent->SetText(FText::Format(
		NSLOCTEXT("ArenaInventory", "PickupLabel", "{0} x{1}\n[G] Pick Up"),
		ItemData->DisplayName,
		FText::AsNumber(Quantity)));
	if (ItemData->ItemTags.HasTag(ArenaGameplayTags::Item_Effect_Restore_Health))
	{
		PickupLabelComponent->SetTextRenderColor(FColor(110, 255, 140));
	}
	else if (ItemData->ItemTags.HasTag(ArenaGameplayTags::Item_Effect_Restore_Energy))
	{
		PickupLabelComponent->SetTextRenderColor(FColor(90, 210, 255));
	}
	else
	{
		PickupLabelComponent->SetTextRenderColor(FColor::White);
	}
}

// 使用当前本地 PlayerCameraManager 旋转文字，不复制任何相机相关状态。
void AArenaInventoryPickupActor::FaceLabelToLocalCamera()
{
	if (!PickupLabelComponent || !GetWorld())
	{
		return;
	}

	APlayerController* LocalPlayerController = GetWorld()->GetFirstPlayerController();
	if (!LocalPlayerController || !LocalPlayerController->IsLocalController()
		|| !LocalPlayerController->PlayerCameraManager)
	{
		return;
	}

	const FVector CameraDirection =
		LocalPlayerController->PlayerCameraManager->GetCameraLocation()
		- PickupLabelComponent->GetComponentLocation();
	if (!CameraDirection.IsNearlyZero())
	{
		PickupLabelComponent->SetWorldRotation(CameraDirection.Rotation());
	}
}
