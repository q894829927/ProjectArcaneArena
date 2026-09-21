#include "Projectile/ArenaProjectileSpatialGrid.h"

#include "AbilitySystemComponent.h"
#include "Character/ArenaEnemyCharacter.h"
#include "Components/CapsuleComponent.h"
#include "GAS/ArenaGameplayTags.h"

// 目标注册发生在 Enemy BeginPlay；数量规模较小，AddUnique 足够且避免额外 UObject/Handle 系统。
void FArenaProjectileSpatialGrid::RegisterTarget(AArenaEnemyCharacter* Target)
{
	if (IsValid(Target))
	{
		RegisteredTargets.AddUnique(Target);
	}
}

// EndPlay 主动移除目标，避免等待下一帧 WeakObjectPtr 清理。
void FArenaProjectileSpatialGrid::UnregisterTarget(AArenaEnemyCharacter* Target)
{
	if (!Target)
	{
		return;
	}

	RegisteredTargets.RemoveAllSwap(
		[Target](const TWeakObjectPtr<AArenaEnemyCharacter>& Entry)
		{
			return !Entry.IsValid() || Entry.Get() == Target;
		},
		EAllowShrinking::No);
}

// 按当前敌人位置重建 Cell；死亡目标保留注册关系但不进入本帧碰撞索引。
void FArenaProjectileSpatialGrid::Rebuild(float InCellSize)
{
	CellSize = FMath::Max(InCellSize, 50.0f);
	Cells.Reset();
	MaxTargetRadius = 0.0f;

	for (int32 Index = RegisteredTargets.Num() - 1; Index >= 0; --Index)
	{
		AArenaEnemyCharacter* Target = RegisteredTargets[Index].Get();
		if (!IsValid(Target))
		{
			RegisteredTargets.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			continue;
		}

		const UAbilitySystemComponent* TargetASC = Target->GetAbilitySystemComponent();
		if (!TargetASC || TargetASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead))
		{
			continue;
		}

		float TargetRadius = 50.0f;
		if (const UCapsuleComponent* Capsule = Target->GetCapsuleComponent())
		{
			TargetRadius = FMath::Max(Capsule->GetScaledCapsuleRadius(), 1.0f);
		}
		MaxTargetRadius = FMath::Max(MaxTargetRadius, TargetRadius);

		Cells.FindOrAdd(PositionToCell(Target->GetActorLocation())).Add(Index);
	}
}

// Projectile 的扫掠 AABB 按最大敌人半径扩张，保证只存中心 Cell 的目标不会被边界漏掉。
void FArenaProjectileSpatialGrid::QuerySegment(
	const FVector& Start,
	const FVector& End,
	float ProjectileRadius,
	TArray<AArenaEnemyCharacter*>& OutCandidates) const
{
	OutCandidates.Reset();
	if (Cells.IsEmpty())
	{
		return;
	}

	const float Expansion = FMath::Max(ProjectileRadius, 0.0f) + MaxTargetRadius;
	const FVector MinPoint(
		FMath::Min(Start.X, End.X) - Expansion,
		FMath::Min(Start.Y, End.Y) - Expansion,
		0.0f);
	const FVector MaxPoint(
		FMath::Max(Start.X, End.X) + Expansion,
		FMath::Max(Start.Y, End.Y) + Expansion,
		0.0f);

	const FIntPoint MinCell = PositionToCell(MinPoint);
	const FIntPoint MaxCell = PositionToCell(MaxPoint);

	for (int32 CellX = MinCell.X; CellX <= MaxCell.X; ++CellX)
	{
		for (int32 CellY = MinCell.Y; CellY <= MaxCell.Y; ++CellY)
		{
			const TArray<int32>* CellEntries = Cells.Find(FIntPoint(CellX, CellY));
			if (!CellEntries)
			{
				continue;
			}

			for (const int32 RegisteredIndex : *CellEntries)
			{
				if (!RegisteredTargets.IsValidIndex(RegisteredIndex))
				{
					continue;
				}

				AArenaEnemyCharacter* Target = RegisteredTargets[RegisteredIndex].Get();
				if (IsValid(Target))
				{
					OutCandidates.Add(Target);
				}
			}
		}
	}
}

// 使用 Floor 而非截断，确保负世界坐标也映射到正确 Cell。
FIntPoint FArenaProjectileSpatialGrid::PositionToCell(const FVector& Position) const
{
	const float SafeCellSize = FMath::Max(CellSize, 50.0f);
	return FIntPoint(
		FMath::FloorToInt(Position.X / SafeCellSize),
		FMath::FloorToInt(Position.Y / SafeCellSize));
}
