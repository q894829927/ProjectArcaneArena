#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Item/ArenaInventoryTypes.h"
#include "ArenaInventoryComponent.generated.h"

class AArenaInventoryPickupActor;
class APawn;
class UArenaItemDataAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FArenaInventoryChangedSignature);

UCLASS(ClassGroup = (Arena), meta = (BlueprintSpawnableComponent))
class PROJECTARCANEARENA_API UArenaInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// 创建仅由 PlayerState 权威维护并向所属客户端增量复制的背包 Model。
	UArenaInventoryComponent();

	// 注册 OwnerOnly FastArray 复制属性。
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 返回按 DisplayOrder 排序的只读背包快照，供 Controller 构建本地分页 ViewData。
	UFUNCTION(BlueprintPure, Category = "Arena|Inventory")
	TArray<FArenaInventoryEntry> GetInventorySnapshot() const;

	// 返回指定堆栈的最新权威数量；无效 StackId 返回零。
	UFUNCTION(BlueprintPure, Category = "Arena|Inventory")
	int32 GetStackQuantity(FGuid StackId) const;

	// 服务器重验阶段与角色状态，并以不可重入事务补栈和拆分溢出数量。
	bool TryAddItem(UArenaItemDataAsset* ItemData, int32 Quantity);

	// 服务器以不可重入且可回滚的冷却事务恢复资源，只有属性实际增加后才消费物品。
	bool TryUseItem(FGuid StackId);

	// 服务器以不可重入事务在安全地面生成复制 Pickup，成功后才扣除请求数量。
	bool TryDropItem(FGuid StackId, int32 Quantity, APawn* SourcePawn);

	// FastArray 回调和 Authority 修改统一通过该入口通知 Controller/View。
	void HandleReplicatedInventoryChanged();

	UPROPERTY(BlueprintAssignable, Category = "Arena|Inventory")
	FArenaInventoryChangedSignature OnInventoryChanged;

private:
	// 仅在服务器查找可修改条目，客户端不能通过返回指针写复制数组。
	FArenaInventoryEntry* FindMutableEntry(FGuid StackId);

	// 删除空堆栈、压紧 DisplayOrder，并只标记实际移除或改序的 FastArray 数据。
	void CompactEntries();

	// 在调用方完成精确 FastArray Dirty 标记后，统一刷新本地 View 和网络发送时机。
	void NotifyInventoryChanged();

	// 按 GameState 权限矩阵验证阶段，并要求玩家存活且未眩晕。
	bool CanPerformInventoryAction() const;

	// 根据 SetByCaller 恢复类型读取当前值和上限，满资源时不执行恢复 GE。
	bool GetTrackedResourceValues(
		const UArenaItemDataAsset* ItemData,
		float& OutCurrentValue,
		float& OutMaximumValue) const;

	// 在玩家前方查找有地面且未被墙体或 Pawn 占用的 Pickup 生成位置。
	bool FindSafeDropTransform(const APawn* SourcePawn, FTransform& OutSpawnTransform) const;

	UPROPERTY(Replicated)
	FArenaInventoryList InventoryList;

	// 阻止 GameplayEffect、Actor BeginPlay 或 UI 委托同步回调重入服务器写事务。
	bool bInventoryMutationInProgress = false;
};
