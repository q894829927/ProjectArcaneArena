#include "Item/ArenaInventoryTypes.h"

#include "Item/ArenaInventoryComponent.h"
#include "Item/ArenaItemDataAsset.h"

// 统一计算筛选后页数，空背包也保留一页二十个槽位。
int32 ArenaInventory::CalculatePageCount(int32 FilteredStackCount)
{
	return FMath::Max(
		1,
		FMath::DivideAndRoundUp(FMath::Max(FilteredStackCount, 0), ItemsPerPage));
}

// 将唯一 ItemTag 合并到分类 Tag 快照，再按 GameplayTag 层级执行任一匹配。
bool ArenaInventory::MatchesFilters(
	const UArenaItemDataAsset* ItemData,
	const FGameplayTagContainer& ActiveFilters)
{
	if (!ItemData)
	{
		return false;
	}
	if (ActiveFilters.IsEmpty())
	{
		return true;
	}

	FGameplayTagContainer SearchableTags = ItemData->ItemTags;
	if (ItemData->ItemTag.IsValid())
	{
		SearchableTags.AddTag(ItemData->ItemTag);
	}
	return SearchableTags.HasAny(ActiveFilters);
}

// 使用标准 FastArray Delta 序列化，仅复制发生变化的背包堆栈。
bool FArenaInventoryList::NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
{
	return FastArrayDeltaSerialize<FArenaInventoryEntry, FArenaInventoryList>(Entries, DeltaParams, *this);
}

// UE 5.6 在完整 Delta 批次接收后调用该钩子，此时 View 可以安全读取最终数组快照。
void FArenaInventoryList::PostReplicatedReceive(
	const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters)
{
	static_cast<void>(Parameters);
	if (OwnerComponent)
	{
		OwnerComponent->HandleReplicatedInventoryChanged();
	}
}
