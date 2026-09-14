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

	// 单次拾取最多创建一百个新堆栈，防止错误关卡数量在一帧内耗尽服务器内存。
	inline constexpr int32 MaxNewStacksPerAddition = 100;

	// 根据筛选后的有效堆栈数量返回至少一页的动态页数。
	PROJECTARCANEARENA_API int32 CalculatePageCount(int32 FilteredStackCount);

	// 原子补满已有数量并拆分溢出；非法状态或单次新增堆栈过多时保持输入不变并返回 false。
	PROJECTARCANEARENA_API bool ApplyStackAddition(
		TArray<int32>& InOutStackQuantities,
		int32 MaxStackSize,
		int32 AddedQuantity);

	// 校验丢弃数量为正且不超过最新可用堆栈，View 与 Authority 共用同一边界语义。
	PROJECTARCANEARENA_API bool IsValidDropQuantity(
		int32 RequestedQuantity,
		int32 AvailableQuantity);

	// 根据按下时开关状态和持续时间决定松开 Tab 是否关闭背包，供 Controller 与测试共用。
	PROJECTARCANEARENA_API bool ShouldCloseOnTabRelease(
		bool bWasOpenOnPress,
		bool bIsOpenOnRelease,
		double HeldDuration,
		double HoldThreshold);

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
