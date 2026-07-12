#pragma once

#include "CoreMinimal.h"
#include "MassEntityHandle.h"
#include "GameFramework/Actor.h"
#include "MassEntityTypes.h"
#include "MyMassClusterActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;

UCLASS()
class PROJECTARCANEARENA_API AMyMassClusterActor
	: public AActor
{
	GENERATED_BODY()

public:
	AMyMassClusterActor();

protected:
	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	virtual void EndPlay(
		const EEndPlayReason::Type EndPlayReason) override;

private:
	void SpawnCluster();
	void UpdateInstanceTransforms();

private:
	UPROPERTY(VisibleAnywhere, Category = "Cluster")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent>
		InstancedMeshComponent;

	UPROPERTY(EditAnywhere, Category = "Cluster")
	TObjectPtr<UStaticMesh> InstanceStaticMesh;

	/** 创建的实体数量 */
	UPROPERTY(EditAnywhere, Category = "Cluster",
		meta = (ClampMin = "1", ClampMax = "10000"))
	int32 EntityCount = 1000;

	/** 初始生成范围 */
	UPROPERTY(EditAnywhere, Category = "Cluster",
		meta = (ClampMin = "1.0"))
	float SpawnRadius = 800.0f;

	/** 实例统一缩放 */
	UPROPERTY(EditAnywhere, Category = "Cluster",
		meta = (ClampMin = "0.01"))
	float InstanceScale = 0.2f;

	UPROPERTY(EditAnywhere, Category = "Cluster")
	float MinimumInitialSpeed = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Cluster")
	float MaximumInitialSpeed = 300.0f;

	/** 各网络 World 使用相同种子生成一致的初始位置和速度；仅用于本地表现模拟。 */
	UPROPERTY(EditAnywhere, Category = "Cluster")
	int32 RandomSeed = 1337;

	TArray<FMassEntityHandle> SpawnedEntities;
};
