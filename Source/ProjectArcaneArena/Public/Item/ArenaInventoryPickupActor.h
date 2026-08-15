#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaInventoryPickupActor.generated.h"

class AArenaPlayerState;
class UPrimitiveComponent;
class URotatingMovementComponent;
class USphereComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UArenaItemDataAsset;

UCLASS(Blueprintable)
class PROJECTARCANEARENA_API AArenaInventoryPickupActor : public AActor
{
	GENERATED_BODY()

public:
	// 创建可复制的按键交互 Pickup，并提供基础旋转和文字占位表现。
	AArenaInventoryPickupActor();

	// 复制物品数据、数量和丢弃者保护时间。
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Deferred Spawn 完成前验证并写入物品、数量和可选丢弃者保护，无效数据返回失败。
	bool InitializePickup(
		UArenaItemDataAsset* InItemData,
		int32 InQuantity,
		AArenaPlayerState* InIgnoredPlayerState = nullptr,
		float IgnoreDuration = 0.0f);

	// 客户端和服务器共同使用该只读条件过滤无效或处于保护期的 Pickup。
	bool CanBeInteractedBy(const AArenaPlayerState* PlayerState) const;

	// 服务器先占用消费门闩，再把全部数量加入目标背包；失败时回滚门闩和碰撞。
	bool TryCollect(AArenaPlayerState* PlayerState);

	// 返回当前复制的物品定义，供本地候选显示和服务器拾取验证读取。
	UFUNCTION(BlueprintPure, Category = "Arena|Inventory Pickup")
	UArenaItemDataAsset* GetItemData() const { return ItemData; }

	// 返回当前复制的整组数量，不允许客户端通过该只读接口修改。
	UFUNCTION(BlueprintPure, Category = "Arena|Inventory Pickup")
	int32 GetQuantity() const { return Quantity; }

protected:
	// 每帧只在有本地相机的客户端刷新文字朝向，服务器玩法不依赖表现 Tick。
	virtual void Tick(float DeltaSeconds) override;

	// 编辑器调整 ItemData 或数量后立即刷新放置 Actor 的文字。
	virtual void OnConstruction(const FTransform& Transform) override;

	// 服务器启动世界寿命，所有端刷新复制物品的名称和数量。
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena|Inventory Pickup")
	TObjectPtr<USphereComponent> PickupCollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena|Inventory Pickup")
	TObjectPtr<UStaticMeshComponent> PickupMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena|Inventory Pickup")
	TObjectPtr<URotatingMovementComponent> RotatingMovementComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena|Inventory Pickup")
	TObjectPtr<UTextRenderComponent> PickupLabelComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Inventory Pickup", meta = (ClampMin = "0.0"))
	float PickupLifeSpan = 30.0f;

private:
	// 复制物品或数量变化后刷新本地世界标签。
	UFUNCTION()
	void OnRep_PickupData();

	// 使用当前 ItemData/Quantity 刷新所有端可见的世界标签。
	void RefreshPickupPresentation();

	// 按 ItemData 的软引用网格、材质和相对变换刷新世界外观，缺失配置时保留蓝图默认表现。
	void RefreshPickupMeshPresentation();

	// 让物品名称始终面向当前世界的本地玩家相机，兼容顶视角和第三人称。
	void FaceLabelToLocalCamera();

	// 优先读取同步服务器时间；缺少 ArenaGameState 的测试世界回退到 World 时间。
	float GetInteractionTimeSeconds() const;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		ReplicatedUsing = OnRep_PickupData,
		Category = "Arena|Inventory Pickup",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UArenaItemDataAsset> ItemData;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		ReplicatedUsing = OnRep_PickupData,
		Category = "Arena|Inventory Pickup",
		meta = (AllowPrivateAccess = "true", ClampMin = "1"))
	int32 Quantity = 1;

	UPROPERTY(Replicated)
	TObjectPtr<AArenaPlayerState> IgnoredPlayerState;

	UPROPERTY(Replicated)
	float IgnoreUntilServerTime = 0.0f;

	// 服务器同步调用背包前先占用该门闩，防止委托重入或多人竞争重复领取。
	bool bConsumed = false;
};
