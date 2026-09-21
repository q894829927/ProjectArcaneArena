#pragma once

#include "CoreMinimal.h"

class AArenaEnemyCharacter;

// P3 轻量二维 Spatial Hash；目标只按中心点进入一个 Cell，Projectile Query 用最大目标半径扩张查询 AABB。
class PROJECTARCANEARENA_API FArenaProjectileSpatialGrid
{
public:
	// 注册服务器存活周期内可被 Data Projectile 命中的敌人；重复注册不会创建重复项。
	void RegisterTarget(AArenaEnemyCharacter* Target);

	// 敌人 EndPlay 时移除目标；弱引用失效也会在 Rebuild 时被清理。
	void UnregisterTarget(AArenaEnemyCharacter* Target);

	// 每帧按当前 Transform 重建 Cell 索引，并跳过已死亡目标。
	void Rebuild(float InCellSize);

	// 查询扫掠线段附近的候选敌人；只负责宽相，不做精确命中判断。
	void QuerySegment(
		const FVector& Start,
		const FVector& End,
		float ProjectileRadius,
		TArray<AArenaEnemyCharacter*>& OutCandidates) const;

	int32 GetRegisteredTargetCount() const { return RegisteredTargets.Num(); }

private:
	FIntPoint PositionToCell(const FVector& Position) const;

	TArray<TWeakObjectPtr<AArenaEnemyCharacter>> RegisteredTargets;
	TMap<FIntPoint, TArray<int32>> Cells;
	float CellSize = 300.0f;
	float MaxTargetRadius = 0.0f;
};
