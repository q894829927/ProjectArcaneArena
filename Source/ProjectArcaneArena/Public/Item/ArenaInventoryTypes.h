#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "ArenaInventoryTypes.generated.h"

class UArenaInventoryComponent;
class UArenaItemDataAsset;

namespace ArenaInventory
{
	// 每页固定二十槽，Controller、View 和自动化测试共用同一常量。
	inline constexpr int32 ItemsPerPage = 20;

	// 根据筛选后的有效堆栈数量返回至少一页的动态页数。
	PROJECTARCANEARENA_API int32 CalculatePageCount(int32 FilteredStackCount);

	// 使用 ItemTag 与 ItemTags 对多选筛选执行 OR 和 GameplayTag 层级匹配。
	PROJECTARCANEARENA_API bool MatchesFilters(
		const UArenaItemDataAsset* ItemData,
		const FGameplayTagContainer& ActiveFilters);
}

USTRUCT(BlueprintType)
struct PROJECTARCANEARENA_API FArenaInventoryEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Inventory")
	FGuid StackId;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Inventory")
	TObjectPtr<UArenaItemDataAsset> ItemData;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Inventory")
	int32 Quantity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Inventory")
	int32 DisplayOrder = INDEX_NONE;
};

USTRUCT()
struct PROJECTARCANEARENA_API FArenaInventoryList : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FArenaInventoryEntry> Entries;

	// 将 FastArray 的增量复制交给引擎，并把回调路由到所属 InventoryComponent。
	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams);

	// 整个 Delta 批次及对象引用映射完成后统一刷新 Model，避免 View 读取半更新数组。
	void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters);

	UArenaInventoryComponent* OwnerComponent = nullptr;
};

template<>
struct TStructOpsTypeTraits<FArenaInventoryList> : public TStructOpsTypeTraitsBase2<FArenaInventoryList>
{
	enum
	{
		WithNetDeltaSerializer = true
	};
};
