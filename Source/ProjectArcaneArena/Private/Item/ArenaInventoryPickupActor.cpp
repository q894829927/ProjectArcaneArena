#include "Item/ArenaInventoryPickupActor.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Core/ArenaBalanceTelemetryComponent.h"
#include "Core/ArenaGameState.h"
#include "Core/ArenaPlayerState.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "GAS/ArenaGameplayTags.h"
#include "Item/ArenaInventoryComponent.h"
#include "Item/ArenaItemDataAsset.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogArenaInventoryPickup, Log, All);

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

// Deferred Spawn 阶段只接受权威、有效物品、合法堆栈和正数量，避免复制无效 Pickup。
bool AArenaInventoryPickupActor::InitializePickup(
	UArenaItemDataAsset* InItemData,
	int32 InQuantity,
	AArenaPlayerState* InIgnoredPlayerState,
	float IgnoreDuration)
{
	if (!HasAuthority() || !InItemData || InQuantity <= 0)
	{
		return false;
	}
	FText RuntimeDefinitionError;
	if (!InItemData->IsRuntimeDefinitionValid(&RuntimeDefinitionError))
	{
		UE_LOG(
			LogArenaInventoryPickup,
			Error,
			TEXT("Rejected invalid inventory pickup definition %s: %s"),
			*GetNameSafe(InItemData),
			*RuntimeDefinitionError.ToString());
		return false;
	}

	ItemData = InItemData;
	Quantity = InQuantity;
	IgnoredPlayerState = InIgnoredPlayerState;
	IgnoreUntilServerTime = GetInteractionTimeSeconds() + FMath::Max(IgnoreDuration, 0.0f);
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
		if (GetInteractionTimeSeconds() < IgnoreUntilServerTime)
		{
			return false;
		}
	}
	return true;
}

// Authority 占用门闩并完成背包加入；成功后记录所属玩家拾取事务并只销毁一次 Actor。
bool AArenaInventoryPickupActor::TryCollect(AArenaPlayerState* PlayerState)
{
	if (!HasAuthority() || !CanBeInteractedBy(PlayerState))
	{
		return false;
	}

	const ECollisionEnabled::Type PreviousCollisionState = PickupCollisionComponent
		? PickupCollisionComponent->GetCollisionEnabled()
		: ECollisionEnabled::NoCollision;
	bConsumed = true;
	if (PickupCollisionComponent)
	{
		PickupCollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	UArenaInventoryComponent* InventoryComponent = PlayerState->GetInventoryComponent();
	if (!InventoryComponent || !InventoryComponent->TryAddItem(ItemData, Quantity))
	{
		bConsumed = false;
		if (PickupCollisionComponent)
		{
			PickupCollisionComponent->SetCollisionEnabled(PreviousCollisionState);
		}
		return false;
	}

	if (const AArenaGameState* GameState = GetWorld()
		? GetWorld()->GetGameState<AArenaGameState>()
		: nullptr)
	{
		if (UArenaBalanceTelemetryComponent* Telemetry =
			GameState->GetBalanceTelemetryComponent())
		{
			Telemetry->RecordPickupTransaction(
				PlayerState,
				EArenaBalancePickupTransaction::Collected,
				ItemData->ItemTag,
				Quantity);
		}
	}
	Destroy();
	return true;
}

// ItemData 或数量复制变化时刷新世界标签。
void AArenaInventoryPickupActor::OnRep_PickupData()
{
	RefreshPickupPresentation();
}

// 使用 DataAsset 的世界外观、玩家可见名称和数量刷新无需额外 WBP 的 Pickup 表现。
void AArenaInventoryPickupActor::RefreshPickupPresentation()
{
	RefreshPickupMeshPresentation();
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

// 客户端和编辑器按 ItemData 应用软引用世界外观；专服不加载纯表现资产。
void AArenaInventoryPickupActor::RefreshPickupMeshPresentation()
{
	if (!PickupMeshComponent
		|| !ItemData
		|| ItemData->WorldMesh.IsNull()
		|| GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	UStaticMesh* ResolvedMesh = ItemData->WorldMesh.LoadSynchronous();
	if (!ResolvedMesh)
	{
		UE_LOG(
			LogArenaInventoryPickup,
			Warning,
			TEXT("Inventory item %s failed to load its configured WorldMesh."),
			*GetNameSafe(ItemData));
		return;
	}

	PickupMeshComponent->SetStaticMesh(ResolvedMesh);
	PickupMeshComponent->SetRelativeLocation(ItemData->WorldMeshRelativeLocation);
	PickupMeshComponent->SetRelativeRotation(ItemData->WorldMeshRelativeRotation);
	PickupMeshComponent->SetRelativeScale3D(ItemData->WorldMeshRelativeScale);

	const int32 MaterialSlotCount = ResolvedMesh->GetStaticMaterials().Num();
	for (int32 MaterialIndex = 0; MaterialIndex < MaterialSlotCount; ++MaterialIndex)
	{
		UMaterialInterface* Material = ItemData->WorldMaterials.IsValidIndex(MaterialIndex)
			? ItemData->WorldMaterials[MaterialIndex].LoadSynchronous()
			: nullptr;
		PickupMeshComponent->SetMaterial(MaterialIndex, Material);
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

// 使用 GameState 的同步服务器时间维护拾取保护；无 GameState 时仍让测试世界中的保护正常过期。
float AArenaInventoryPickupActor::GetInteractionTimeSeconds() const
{
	const UWorld* World = GetWorld();
	const AArenaGameState* ArenaGameState = World ? World->GetGameState<AArenaGameState>() : nullptr;
	return ArenaGameState
		? ArenaGameState->GetServerWorldTimeSeconds()
		: (World ? static_cast<float>(World->GetTimeSeconds()) : 0.0f);
}
