#include "Item/ArenaPickupActor.h"

#include "AbilitySystemComponent.h"
#include "Character/ArenaPlayerCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GAS/ArenaAttributeSet.h"
#include "GAS/ArenaGameplayEffect_EnergyRestore.h"
#include "GAS/ArenaGameplayEffect_HealthRestore.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "GameplayEffect.h"
#include "UObject/ConstructorHelpers.h"

AArenaPickupActor::AArenaPickupActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	PickupCollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("PickupCollisionComponent"));
	SetRootComponent(PickupCollisionComponent);
	PickupCollisionComponent->InitSphereRadius(70.0f);
	PickupCollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupCollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	PickupCollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupCollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	PickupCollisionComponent->SetGenerateOverlapEvents(true);
	PickupCollisionComponent->SetCanEverAffectNavigation(false);
	PickupCollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &AArenaPickupActor::HandlePickupOverlap);

	PickupMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMeshComponent"));
	PickupMeshComponent->SetupAttachment(PickupCollisionComponent);
	PickupMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupMeshComponent->SetGenerateOverlapEvents(false);
	PickupMeshComponent->SetCanEverAffectNavigation(false);
	PickupMeshComponent->SetRelativeScale3D(FVector(0.3f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		PickupMeshComponent->SetStaticMesh(SphereMesh.Object);
	}

	RotatingMovementComponent = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("RotatingMovementComponent"));
	RotatingMovementComponent->RotationRate = FRotator(0.0f, 90.0f, 0.0f);
}

// 启动服务器生命周期；客户端只等待权威 Actor 的复制销毁。
void AArenaPickupActor::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && PickupLifeSpan > 0.0f)
	{
		SetLifeSpan(PickupLifeSpan);
	}
}

// 成功恢复后设置消费门闩，确保同帧多人重叠时只有首个有效玩家拾取。
void AArenaPickupActor::HandlePickupOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!HasAuthority() || bConsumed)
	{
		return;
	}

	AArenaPlayerCharacter* PlayerCharacter = Cast<AArenaPlayerCharacter>(OtherActor);
	if (!PlayerCharacter || !TryApplyRestore(PlayerCharacter))
	{
		return;
	}

	bConsumed = true;
	PickupCollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Destroy();
}

// 恢复前后比较权威属性，防止满资源、Max=0 或失效 GE 配置误消耗掉落物。
bool AArenaPickupActor::TryApplyRestore(AArenaPlayerCharacter* PlayerCharacter)
{
	UAbilitySystemComponent* PlayerASC = PlayerCharacter ? PlayerCharacter->GetAbilitySystemComponent() : nullptr;
	const UArenaAttributeSet* AttributeSet = PlayerASC
		? Cast<const UArenaAttributeSet>(PlayerASC->GetAttributeSet(UArenaAttributeSet::StaticClass()))
		: nullptr;
	if (!PlayerASC || !AttributeSet || RestoreAmount <= KINDA_SMALL_NUMBER
		|| PlayerASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead))
	{
		return false;
	}

	const bool bHealthPickup = PickupType == EArenaPickupType::Health;
	const float ValueBefore = bHealthPickup ? AttributeSet->GetHealth() : AttributeSet->GetEnergy();
	const float MaxValue = bHealthPickup ? AttributeSet->GetMaxHealth() : AttributeSet->GetMaxEnergy();
	if (MaxValue <= KINDA_SMALL_NUMBER || ValueBefore >= MaxValue - KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const TSubclassOf<UGameplayEffect> RestoreEffectClass = bHealthPickup
		? UArenaGameplayEffect_HealthRestore::StaticClass()
		: UArenaGameplayEffect_EnergyRestore::StaticClass();
	const FGameplayTag RecoveryMagnitudeTag = bHealthPickup
		? ArenaGameplayTags::SetByCaller_Recovery_Health
		: ArenaGameplayTags::SetByCaller_Recovery_Energy;

	FGameplayEffectContextHandle EffectContext = PlayerASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);
	FGameplayEffectSpecHandle RestoreSpecHandle = PlayerASC->MakeOutgoingSpec(
		RestoreEffectClass,
		1.0f,
		EffectContext);
	if (!RestoreSpecHandle.IsValid())
	{
		return false;
	}

	RestoreSpecHandle.Data->SetSetByCallerMagnitude(RecoveryMagnitudeTag, RestoreAmount);
	PlayerASC->ApplyGameplayEffectSpecToSelf(*RestoreSpecHandle.Data.Get());

	const float ValueAfter = bHealthPickup ? AttributeSet->GetHealth() : AttributeSet->GetEnergy();
	return ValueAfter > ValueBefore + KINDA_SMALL_NUMBER;
}
