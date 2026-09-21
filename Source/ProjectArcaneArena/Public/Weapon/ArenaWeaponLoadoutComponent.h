#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ArenaWeaponLoadoutComponent.generated.h"

class UArenaWeaponDataAsset;

USTRUCT(BlueprintType)
struct PROJECTARCANEARENA_API FArenaWeaponRuntime
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Weapon")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Weapon")
	int32 WeaponRuntimeID = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Weapon")
	TObjectPtr<UArenaWeaponDataAsset> WeaponDefinition = nullptr;

	bool IsValid() const
	{
		return SlotIndex != INDEX_NONE && WeaponRuntimeID > 0 && WeaponDefinition != nullptr;
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FArenaWeaponLoadoutChangedSignature);

// PlayerState 持有的本局武器装备 Model；P4-A/B 先提供两槽、稳定 RuntimeID 与独立 AttackInstanceID 计数。
UCLASS(ClassGroup = (Arena), meta = (BlueprintSpawnableComponent))
class PROJECTARCANEARENA_API UArenaWeaponLoadoutComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UArenaWeaponLoadoutComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 返回固定槽位数量；首版默认两槽，后续可在 Blueprint Defaults 扩展到六槽。
	UFUNCTION(BlueprintPure, Category = "Arena|Weapon")
	int32 GetMaxWeaponSlots() const { return MaxWeaponSlots; }

	// 返回所有槽位的只读快照；空槽的 WeaponRuntimeID 为 INDEX_NONE。
	UFUNCTION(BlueprintPure, Category = "Arena|Weapon")
	TArray<FArenaWeaponRuntime> GetWeaponRuntimes() const { return WeaponRuntimes; }

	// 查询指定槽位；空槽或越界返回 false。
	UFUNCTION(BlueprintPure, Category = "Arena|Weapon")
	bool GetWeaponRuntimeAtSlot(int32 SlotIndex, FArenaWeaponRuntime& OutRuntime) const;

	// 仅服务器装备静态定义；替换现有武器会分配新的稳定 RuntimeID 并重置该实例攻击轮次。
	UFUNCTION(BlueprintCallable, Category = "Arena|Weapon")
	bool EquipWeapon(int32 SlotIndex, UArenaWeaponDataAsset* WeaponDefinition);

	// 仅服务器清空指定槽位。
	UFUNCTION(BlueprintCallable, Category = "Arena|Weapon")
	bool UnequipWeapon(int32 SlotIndex);

	// 服务器为指定 Runtime 分配独立递增 AttackInstanceID；0 永不作为有效攻击轮次。
	int32 AllocateAttackInstanceID(int32 WeaponRuntimeID);

	const FArenaWeaponRuntime* FindWeaponRuntimeAtSlot(int32 SlotIndex) const;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Weapon")
	FArenaWeaponLoadoutChangedSignature OnWeaponLoadoutChanged;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnRep_WeaponRuntimes();

	void EnsureSlotStorage();
	int32 AllocateWeaponRuntimeID();
	void ResetAttackCounterForRuntime(int32 WeaponRuntimeID);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Weapon", meta = (AllowPrivateAccess = "true", ClampMin = "1", ClampMax = "6"))
	int32 MaxWeaponSlots = 2;

	// 服务器 BeginPlay 时按槽位顺序装备；用于首版在 BP_ArenaPlayerState 上配置默认武器。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Weapon", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UArenaWeaponDataAsset>> DefaultWeapons;

	UPROPERTY(ReplicatedUsing = OnRep_WeaponRuntimes)
	TArray<FArenaWeaponRuntime> WeaponRuntimes;

	int32 NextWeaponRuntimeID = 1;
	TMap<int32, int32> NextAttackInstanceIDByRuntime;
};
