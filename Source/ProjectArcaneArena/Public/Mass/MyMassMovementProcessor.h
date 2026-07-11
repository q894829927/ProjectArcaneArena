#pragma once

#include "CoreMinimal.h"
#include "MassEntityQuery.h"
#include "MassProcessor.h"
#include "MyMassMovementProcessor.generated.h"

/**
 * 简化版 Boids 集群运动 Processor。
 */
UCLASS()
class PROJECTARCANEARENA_API UMyMassMovementProcessor
	: public UMassProcessor
{
	GENERATED_BODY()

public:
	UMyMassMovementProcessor();

protected:
	virtual void ConfigureQueries(
		const TSharedRef<FMassEntityManager>& EntityManager) override;

	virtual void Execute(
		FMassEntityManager& EntityManager,
		FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
