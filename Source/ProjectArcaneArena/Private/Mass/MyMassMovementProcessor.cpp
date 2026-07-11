#include "Mass/MyMassMovementProcessor.h"

#include "Mass/MyMassFragments.h"
#include "MassExecutionContext.h"

UMyMassMovementProcessor::UMyMassMovementProcessor()
	: EntityQuery(*this)
{
	// 在物理模拟之前更新集群位置。
	ProcessingPhase = EMassProcessingPhase::PrePhysics;

	// 自动加入全局 Mass Processor 执行列表。
	bAutoRegisterWithProcessingPhases = true;
}

void UMyMassMovementProcessor::ConfigureQueries(
	const TSharedRef<FMassEntityManager>& /*EntityManager*/)
{
	// UE 5.6 在初始化时传入实体管理器；查询配置保持与具体 World 无关。
	// MovementFragment 会被 Processor 修改。
	EntityQuery.AddRequirement<FMyMovementFragment>(
		EMassFragmentAccess::ReadWrite,
		EMassFragmentPresence::All);

	// 只处理带有 FMyClusterTag 的实体。
	EntityQuery.AddTagRequirement<FMyClusterTag>(
		EMassFragmentPresence::All);
}

void UMyMassMovementProcessor::Execute(
	FMassEntityManager& /*EntityManager*/,
	FMassExecutionContext& Context)
{
	/*
	 * 为了避免读取已经在本帧被修改的数据，
	 * 第一遍先把所有位置和速度保存为快照。
	 */
	TArray<FVector> PositionSnapshot;
	TArray<FVector> VelocitySnapshot;

	auto GatherSnapshot =
		[&PositionSnapshot, &VelocitySnapshot](
			FMassExecutionContext& ChunkContext)
		{
			const TConstArrayView<FMyMovementFragment> Movements =
				ChunkContext.GetFragmentView<FMyMovementFragment>();

			PositionSnapshot.Reserve(
				PositionSnapshot.Num() + Movements.Num());

			VelocitySnapshot.Reserve(
				VelocitySnapshot.Num() + Movements.Num());

			for (const FMyMovementFragment& Movement : Movements)
			{
				PositionSnapshot.Add(Movement.Position);
				VelocitySnapshot.Add(Movement.Velocity);
			}
		};

	EntityQuery.ForEachEntityChunk(
		Context,
		GatherSnapshot);

	if (PositionSnapshot.IsEmpty())
	{
		return;
	}

	/*
	 * 简化 Boids 参数。
	 * 后续可以把它们放进 Shared Fragment 或 DeveloperSettings。
	 */
	constexpr float NeighborRadius = 300.0f;
	constexpr float SeparationRadius = 90.0f;

	constexpr float SeparationWeight = 2.2f;
	constexpr float AlignmentWeight = 0.8f;
	constexpr float CohesionWeight = 0.65f;
	constexpr float ReturnToCenterWeight = 1.4f;

	constexpr float ClusterRadius = 1500.0f;
	constexpr float MaximumSteeringForce = 450.0f;
	constexpr float MinimumSpeedRatio = 0.35f;

	const float NeighborRadiusSquared =
		FMath::Square(NeighborRadius);

	const float SeparationRadiusSquared =
		FMath::Square(SeparationRadius);

	const float ClusterRadiusSquared =
		FMath::Square(ClusterRadius);

	int32 GlobalEntityIndex = 0;

	auto UpdateMovement =
		[&PositionSnapshot,
		 &VelocitySnapshot,
		 &GlobalEntityIndex,
		 NeighborRadiusSquared,
		 SeparationRadiusSquared,
		 ClusterRadiusSquared](
			FMassExecutionContext& ChunkContext)
		{
			TArrayView<FMyMovementFragment> Movements =
				ChunkContext.GetMutableFragmentView<
					FMyMovementFragment>();

			const float DeltaTime =
				ChunkContext.GetDeltaTimeSeconds();

			for (int32 LocalIndex = 0;
				 LocalIndex < Movements.Num();
				 ++LocalIndex, ++GlobalEntityIndex)
			{
				FMyMovementFragment& Movement =
					Movements[LocalIndex];

				const int32 SelfIndex = GlobalEntityIndex;

				if (!PositionSnapshot.IsValidIndex(SelfIndex))
				{
					continue;
				}

				const FVector SelfPosition =
					PositionSnapshot[SelfIndex];

				const FVector SelfVelocity =
					VelocitySnapshot[SelfIndex];

				FVector SeparationForce = FVector::ZeroVector;
				FVector AverageNeighborVelocity =
					FVector::ZeroVector;
				FVector NeighborCenter = FVector::ZeroVector;

				int32 NeighborCount = 0;

				for (int32 OtherIndex = 0;
					 OtherIndex < PositionSnapshot.Num();
					 ++OtherIndex)
				{
					if (OtherIndex == SelfIndex)
					{
						continue;
					}

					const FVector ToOther =
						PositionSnapshot[OtherIndex]
						- SelfPosition;

					const float DistanceSquared =
						ToOther.SizeSquared();

					if (DistanceSquared >=
						NeighborRadiusSquared)
					{
						continue;
					}

					AverageNeighborVelocity +=
						VelocitySnapshot[OtherIndex];

					NeighborCenter +=
						PositionSnapshot[OtherIndex];

					++NeighborCount;

					if (DistanceSquared <
							SeparationRadiusSquared
						&& DistanceSquared >
							UE_SMALL_NUMBER)
					{
						/*
						 * ToOther 指向邻居。
						 * 取负值即可形成远离邻居的力。
						 */
						SeparationForce -=
							ToOther
							/ FMath::Max(
								DistanceSquared,
								1.0f);
					}
				}

				FVector AlignmentForce = FVector::ZeroVector;
				FVector CohesionForce = FVector::ZeroVector;

				if (NeighborCount > 0)
				{
					AverageNeighborVelocity /=
						static_cast<float>(NeighborCount);

					NeighborCenter /=
						static_cast<float>(NeighborCount);

					AlignmentForce =
						AverageNeighborVelocity
						- SelfVelocity;

					CohesionForce =
						NeighborCenter
						- SelfPosition;
				}

				FVector SteeringForce =
					SeparationForce.GetSafeNormal()
						* SeparationWeight
					+ AlignmentForce.GetSafeNormal()
						* AlignmentWeight
					+ CohesionForce.GetSafeNormal()
						* CohesionWeight;

				/*
				 * 集群跑出指定半径时，将其拉回中心。
				 */
				const FVector ToClusterCenter =
					Movement.ClusterCenter
					- SelfPosition;

				if (ToClusterCenter.SizeSquared() >
					ClusterRadiusSquared)
				{
					SteeringForce +=
						ToClusterCenter.GetSafeNormal()
						* ReturnToCenterWeight;
				}

				SteeringForce =
					SteeringForce.GetClampedToMaxSize(
						MaximumSteeringForce);

				Movement.Velocity +=
					SteeringForce * DeltaTime;

				Movement.Velocity =
					Movement.Velocity.GetClampedToMaxSize(
						Movement.MaxSpeed);

				const float MinimumSpeed =
					Movement.MaxSpeed * MinimumSpeedRatio;

				if (Movement.Velocity.SizeSquared()
					< FMath::Square(MinimumSpeed))
				{
					const FVector Direction =
						Movement.Velocity.GetSafeNormal(
							UE_SMALL_NUMBER,
							FVector::ForwardVector);

					Movement.Velocity =
						Direction * MinimumSpeed;
				}

				Movement.Position +=
					Movement.Velocity * DeltaTime;
			}
		};

	EntityQuery.ForEachEntityChunk(
		Context,
		UpdateMovement);
}
