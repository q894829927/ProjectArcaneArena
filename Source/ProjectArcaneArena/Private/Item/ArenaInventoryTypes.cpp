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

// 原子执行补栈与溢出拆栈，并限制单次新建条目数量以防错误 Pickup 阻塞服务器。
bool ArenaInventory::ApplyStackAddition(
	TArray<int32>& InOutStackQuantities,
	int32 MaxStackSize,
	int32 AddedQuantity)
{
	if (MaxStackSize <= 0 || AddedQuantity <= 0)
	{
		return false;
	}

	for (const int32 ExistingQuantity : InOutStackQuantities)
	{
		if (ExistingQuantity <= 0 || ExistingQuantity > MaxStackSize)
		{
			return false;
		}
	}

	int64 RemainingQuantityForValidation = AddedQuantity;
	for (const int32 ExistingQuantity : InOutStackQuantities)
	{
		RemainingQuantityForValidation -= FMath::Min<int64>(
			static_cast<int64>(MaxStackSize) - ExistingQuantity,
			RemainingQuantityForValidation);
		if (RemainingQuantityForValidation <= 0)
		{
			break;
		}
	}

	const int64 RequiredNewStackCount = RemainingQuantityForValidation > 0
		? (RemainingQuantityForValidation + static_cast<int64>(MaxStackSize) - 1) / MaxStackSize
		: 0;
	if (RequiredNewStackCount > MaxNewStacksPerAddition)
	{
		return false;
	}

	TArray<int32> ResolvedQuantities = InOutStackQuantities;
	int32 RemainingQuantity = AddedQuantity;
	for (int32& ExistingQuantity : ResolvedQuantities)
	{
		const int32 AddedToExisting = FMath::Min(MaxStackSize - ExistingQuantity, RemainingQuantity);
		ExistingQuantity += AddedToExisting;
		RemainingQuantity -= AddedToExisting;
		if (RemainingQuantity <= 0)
		{
			InOutStackQuantities = MoveTemp(ResolvedQuantities);
			return true;
		}
	}

	while (RemainingQuantity > 0)
	{
		const int32 NewStackQuantity = FMath::Min(MaxStackSize, RemainingQuantity);
		ResolvedQuantities.Add(NewStackQuantity);
		RemainingQuantity -= NewStackQuantity;
	}
	InOutStackQuantities = MoveTemp(ResolvedQuantities);
	return true;
}

// 统一拒绝非正数、空堆栈和超量丢弃请求，不替调用方静默修正数量。
bool ArenaInventory::IsValidDropQuantity(int32 RequestedQuantity, int32 AvailableQuantity)
{
	return RequestedQuantity > 0
		&& AvailableQuantity > 0
		&& RequestedQuantity <= AvailableQuantity;
}

// 关闭中的短按保持新打开状态，长按临时查看或原本已打开的按键在松开时关闭。
bool ArenaInventory::ShouldCloseOnTabRelease(
	bool bWasOpenOnPress,
	bool bIsOpenOnRelease,
	double HeldDuration,
	double HoldThreshold)
{
	if (!bIsOpenOnRelease)
	{
		return false;
	}

	const double SafeHeldDuration = FMath::Max(HeldDuration, 0.0);
	const double SafeHoldThreshold = FMath::Max(HoldThreshold, 0.0);
	return bWasOpenOnPress || SafeHeldDuration >= SafeHoldThreshold;
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
