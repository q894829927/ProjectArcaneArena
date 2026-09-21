#include "Weapon/ArenaWeaponLoadoutComponent.h"

#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "Weapon/ArenaWeaponDataAsset.h"

// 创建 PlayerState 武器 Model；组件自身复制，具体槽位数据只发给拥有者。
UArenaWeaponLoadoutComponent::UArenaWeaponLoadoutComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

// 初始化固定槽位，并仅在 Authority 上把 Blueprint 默认武器转换成带 RuntimeID 的运行实例。
void UArenaWeaponLoadoutComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsureSlotStorage();

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	const int32 DefaultCount = FMath::Min(DefaultWeapons.Num(), MaxWeaponSlots);
	for (int32 SlotIndex = 0; SlotIndex < DefaultCount; ++SlotIndex)
	{
		if (DefaultWeapons[SlotIndex])
		{
			EquipWeapon(SlotIndex, DefaultWeapons[SlotIndex]);
		}
	}
}

// 武器装备属于玩家私有构筑信息，首版只复制给 Owner；权威命中仍完全由服务器决定。
void UArenaWeaponLoadoutComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UArenaWeaponLoadoutComponent, WeaponRuntimes, COND_OwnerOnly);
}

// 查询槽位快照，避免 Blueprint 或外部系统直接修改组件内部数组。
bool UArenaWeaponLoadoutComponent::GetWeaponRuntimeAtSlot(int32 SlotIndex, FArenaWeaponRuntime& OutRuntime) const
{
	OutRuntime = FArenaWeaponRuntime();
	const FArenaWeaponRuntime* Runtime = FindWeaponRuntimeAtSlot(SlotIndex);
	if (!Runtime || !Runtime->IsValid())
	{
		return false;
	}

	OutRuntime = *Runtime;
	return true;
}

// 服务器装备或替换一个槽位；新实例总是获得新的 RuntimeID，防止迟到 Projectile 与旧实例串状态。
bool UArenaWeaponLoadoutComponent::EquipWeapon(int32 SlotIndex, UArenaWeaponDataAsset* WeaponDefinition)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !WeaponDefinition)
	{
		return false;
	}

	EnsureSlotStorage();
	if (!WeaponRuntimes.IsValidIndex(SlotIndex))
	{
		return false;
	}

	FArenaWeaponRuntime& Runtime = WeaponRuntimes[SlotIndex];
	if (Runtime.WeaponRuntimeID > 0)
	{
		NextAttackInstanceIDByRuntime.Remove(Runtime.WeaponRuntimeID);
	}

	Runtime.SlotIndex = SlotIndex;
	Runtime.WeaponRuntimeID = AllocateWeaponRuntimeID();
	Runtime.WeaponDefinition = WeaponDefinition;
	ResetAttackCounterForRuntime(Runtime.WeaponRuntimeID);

	OnWeaponLoadoutChanged.Broadcast();
	OwnerActor->ForceNetUpdate();
	return true;
}

// 服务器卸下武器并移除该 Runtime 的攻击轮次计数，旧 RuntimeID 不会再次分配给当前进程中的下一实例。
bool UArenaWeaponLoadoutComponent::UnequipWeapon(int32 SlotIndex)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return false;
	}

	EnsureSlotStorage();
	if (!WeaponRuntimes.IsValidIndex(SlotIndex))
	{
		return false;
	}

	FArenaWeaponRuntime& Runtime = WeaponRuntimes[SlotIndex];
	if (!Runtime.IsValid())
	{
		return false;
	}

	NextAttackInstanceIDByRuntime.Remove(Runtime.WeaponRuntimeID);
	Runtime = FArenaWeaponRuntime();
	Runtime.SlotIndex = SlotIndex;

	OnWeaponLoadoutChanged.Broadcast();
	OwnerActor->ForceNetUpdate();
	return true;
}

// 每个 WeaponRuntime 独立生成 AttackInstanceID，散射时同一轮可复用该 ID，多武器之间不会共享计数器。
int32 UArenaWeaponLoadoutComponent::AllocateAttackInstanceID(int32 WeaponRuntimeID)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || WeaponRuntimeID <= 0)
	{
		return 0;
	}

	int32* NextID = NextAttackInstanceIDByRuntime.Find(WeaponRuntimeID);
	if (!NextID)
	{
		return 0;
	}

	const int32 AllocatedID = *NextID;
	++(*NextID);
	if (*NextID <= 0)
	{
		*NextID = 1;
	}
	return AllocatedID;
}

// C++ 调度器读取固定槽位；返回指针只在当前调用栈内使用，不允许缓存跨复制/换装。
const FArenaWeaponRuntime* UArenaWeaponLoadoutComponent::FindWeaponRuntimeAtSlot(int32 SlotIndex) const
{
	return WeaponRuntimes.IsValidIndex(SlotIndex) ? &WeaponRuntimes[SlotIndex] : nullptr;
}

// OwnerOnly 装备状态复制完成后通知本地 HUD/表现层刷新；不参与权威发射判定。
void UArenaWeaponLoadoutComponent::OnRep_WeaponRuntimes()
{
	OnWeaponLoadoutChanged.Broadcast();
}

// 保持数组长度与 MaxWeaponSlots 一致，并为每个空槽写入稳定 SlotIndex。
void UArenaWeaponLoadoutComponent::EnsureSlotStorage()
{
	MaxWeaponSlots = FMath::Clamp(MaxWeaponSlots, 1, 6);
	const int32 OldNum = WeaponRuntimes.Num();
	WeaponRuntimes.SetNum(MaxWeaponSlots);

	for (int32 SlotIndex = 0; SlotIndex < WeaponRuntimes.Num(); ++SlotIndex)
	{
		if (SlotIndex >= OldNum || WeaponRuntimes[SlotIndex].SlotIndex == INDEX_NONE)
		{
			WeaponRuntimes[SlotIndex].SlotIndex = SlotIndex;
		}
	}
}

// RuntimeID 在 PlayerState 生命周期内单调递增；回绕后从 1 开始，0/INDEX_NONE 均保留为无效值。
int32 UArenaWeaponLoadoutComponent::AllocateWeaponRuntimeID()
{
	const int32 AllocatedID = NextWeaponRuntimeID;
	++NextWeaponRuntimeID;
	if (NextWeaponRuntimeID <= 0)
	{
		NextWeaponRuntimeID = 1;
	}
	return AllocatedID;
}

// 新 Runtime 的第一轮 AttackInstanceID 从 1 开始，装备替换不会继承旧武器的攻击轮次。
void UArenaWeaponLoadoutComponent::ResetAttackCounterForRuntime(int32 WeaponRuntimeID)
{
	if (WeaponRuntimeID > 0)
	{
		NextAttackInstanceIDByRuntime.Add(WeaponRuntimeID, 1);
	}
}
