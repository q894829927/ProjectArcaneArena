#include "Mass/MyMassClusterActor.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Mass/MyMassFragments.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"

AMyMassClusterActor::AMyMassClusterActor()
{
	PrimaryActorTick.bCanEverTick = true;

	/*
	 * Mass 移动 Processor 在 PrePhysics 阶段更新。
	 * Actor 在 PostPhysics 同步渲染，尽量保证读取到本帧新位置。
	 */
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	InstancedMeshComponent =
		CreateDefaultSubobject<
			UHierarchicalInstancedStaticMeshComponent>(
				TEXT("InstancedMeshComponent"));

	SetRootComponent(InstancedMeshComponent);

	InstancedMeshComponent->SetCollisionEnabled(
		ECollisionEnabled::NoCollision);

	InstancedMeshComponent->SetGenerateOverlapEvents(false);
}

void AMyMassClusterActor::BeginPlay()
{
	Super::BeginPlay();

	if (!IsValid(InstanceStaticMesh))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"MyMassClusterActor has no InstanceStaticMesh"));

		SetActorTickEnabled(false);
		return;
	}

	InstancedMeshComponent->SetStaticMesh(
		InstanceStaticMesh);

	SpawnCluster();
}

void AMyMassClusterActor::SpawnCluster()
{
	UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		return;
	}

	UMassEntitySubsystem* MassSubsystem =
		World->GetSubsystem<UMassEntitySubsystem>();

	if (!IsValid(MassSubsystem))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("MassEntitySubsystem is unavailable"));

		return;
	}

	FMassEntityManager& EntityManager =
		MassSubsystem->GetMutableEntityManager();

	/*
	 * 一个 Archetype：
	 *
	 * FMyMovementFragment
	 * FMyClusterTag
	 */
	TArray<const UScriptStruct*> Composition;
	Composition.Reserve(2);

	Composition.Add(
		FMyMovementFragment::StaticStruct());

	Composition.Add(
		FMyClusterTag::StaticStruct());

	const FMassArchetypeHandle ClusterArchetype =
		EntityManager.CreateArchetype(Composition);

	SpawnedEntities.Reset();
	SpawnedEntities.Reserve(EntityCount);

	/*
	 * 批量创建比循环 CreateEntity 更适合 Mass。
	 *
	 * CreationContext 存活期间会暂缓 Observer 通知，
	 * 函数结束后统一发送。
	 */
	const TSharedRef<FMassEntityManager::FEntityCreationContext>
		CreationContext =
			EntityManager.BatchCreateEntities(
				ClusterArchetype,
				EntityCount,
				SpawnedEntities);

	const FVector Center = GetActorLocation();

	for (int32 Index = 0;
		 Index < SpawnedEntities.Num();
		 ++Index)
	{
		const FMassEntityHandle Entity =
			SpawnedEntities[Index];

		FMyMovementFragment& Movement =
			EntityManager.GetFragmentDataChecked<
				FMyMovementFragment>(Entity);

		const FVector RandomDirection =
			FMath::VRand().GetSafeNormal(
				UE_SMALL_NUMBER,
				FVector::ForwardVector);

		const float RandomDistance =
			FMath::FRandRange(
				0.0f,
				SpawnRadius);

		Movement.Position =
			Center
			+ RandomDirection * RandomDistance;

		Movement.ClusterCenter = Center;

		Movement.MaxSpeed =
			FMath::FRandRange(
				MinimumInitialSpeed,
				MaximumInitialSpeed);

		Movement.Velocity =
			FMath::VRand().GetSafeNormal(
				UE_SMALL_NUMBER,
				FVector::ForwardVector)
			* Movement.MaxSpeed;

		const FTransform InstanceTransform(
			Movement.Velocity.Rotation(),
			Movement.Position,
			FVector(InstanceScale));

		/*
		 * 第二个参数为 true，表示传入的是世界空间 Transform。
		 */
		InstancedMeshComponent->AddInstance(
			InstanceTransform,
			true);
	}
}

void AMyMassClusterActor::Tick(
	const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateInstanceTransforms();
}

void AMyMassClusterActor::UpdateInstanceTransforms()
{
	UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		return;
	}

	UMassEntitySubsystem* MassSubsystem =
		World->GetSubsystem<UMassEntitySubsystem>();

	if (!IsValid(MassSubsystem))
	{
		return;
	}

	FMassEntityManager& EntityManager =
		MassSubsystem->GetMutableEntityManager();

	const int32 LastInstanceIndex =
		SpawnedEntities.Num() - 1;

	for (int32 Index = 0;
		 Index < SpawnedEntities.Num();
		 ++Index)
	{
		const FMassEntityHandle Entity =
			SpawnedEntities[Index];

		if (!EntityManager.IsEntityActive(Entity))
		{
			continue;
		}

		const FMyMovementFragment* Movement =
			EntityManager.GetFragmentDataPtr<
				FMyMovementFragment>(Entity);

		if (Movement == nullptr)
		{
			continue;
		}

		const FRotator Rotation =
			Movement->Velocity.IsNearlyZero()
				? FRotator::ZeroRotator
				: Movement->Velocity.Rotation();

		const FTransform InstanceTransform(
			Rotation,
			Movement->Position,
			FVector(InstanceScale));

		/*
		 * 前面的实例不立即刷新 RenderState，
		 * 最后一个实例更新时统一刷新。
		 */
		const bool bMarkRenderStateDirty =
			Index == LastInstanceIndex;

		InstancedMeshComponent->UpdateInstanceTransform(
			Index,
			InstanceTransform,
			true,
			bMarkRenderStateDirty,
			true);
	}
}

void AMyMassClusterActor::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UMassEntitySubsystem* MassSubsystem =
			World->GetSubsystem<UMassEntitySubsystem>())
		{
			FMassEntityManager& EntityManager =
				MassSubsystem->GetMutableEntityManager();

			EntityManager.BatchDestroyEntities(
				SpawnedEntities);
		}
	}

	SpawnedEntities.Reset();

	if (IsValid(InstancedMeshComponent))
	{
		InstancedMeshComponent->ClearInstances();
	}

	Super::EndPlay(EndPlayReason);
}
