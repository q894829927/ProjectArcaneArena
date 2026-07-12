#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "MyMassFragments.generated.h"

/**
 * 单个集群实体的位置和运动数据。
 *
 * 本示例没有使用 FTransformFragment，
 * 目的是先用最简单的自定义 Fragment 展示 Mass 数据布局。
 */
USTRUCT()
struct PROJECTARCANEARENA_API FMyMovementFragment : public FMassFragment
{
	GENERATED_BODY()

	/** 当前世界位置 */
	UPROPERTY()
	FVector Position = FVector::ZeroVector;

	/** 当前世界速度 */
	UPROPERTY()
	FVector Velocity = FVector::ZeroVector;

	/** 集群中心 */
	UPROPERTY()
	FVector ClusterCenter = FVector::ZeroVector;

	/** 最大移动速度 */
	UPROPERTY()
	float MaxSpeed = 300.0f;
};

/**
 * 用来标记“属于本集群”的实体。
 *
 * Tag 不保存数据，只用于过滤 Archetype。
 */
USTRUCT()
struct PROJECTARCANEARENA_API FMyClusterTag : public FMassTag
{
	GENERATED_BODY()
};