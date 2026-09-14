#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaUpgradeTestPickupActor.generated.h"

class AArenaPlayerCharacter;
class AArenaPlayerState;
class UArenaUpgradeDataAsset;
class UMaterialInstanceDynamic;
class UPrimitiveComponent;
class URotatingMovementComponent;
class USphereComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS(Blueprintable)
class PROJECTARCANEARENA_API AArenaUpgradeTestPickupActor : public AActor
{
	GENERATED_BODY()

public:
	// 创建可复制的测试升级拾取物，每次有效重叠都由服务器授予。
	AArenaUpgradeTestPickupActor();

	// 在各客户端让名称和描述朝向本地相机，兼容顶视角与第三人称观察。
	virtual void Tick(float DeltaSeconds) override;

	// 实例 UpgradeData 变化时同步 ASCII 名称、简短描述和构筑颜色。
	virtual void OnConstruction(const FTransform& Transform) override;

	// 复制测试升级引用，使动态生成时各客户端显示一致。
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 供编辑器脚本在写入 UpgradeData 后立即刷新可视化，运行时也可安全重复调用。
	UFUNCTION(BlueprintCallable, Category = "Arena|Test Upgrade Pickup")
	void RefreshPickupPresentation();

protected:
	// 开始游戏时从已序列化的 UpgradeData 刷新 Host 表现，客户端随后仍由 OnRep 兜底。
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena|Test Upgrade Pickup")
	TObjectPtr<USphereComponent> PickupCollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena|Test Upgrade Pickup")
	TObjectPtr<UStaticMeshComponent> PickupMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena|Test Upgrade Pickup")
	TObjectPtr<UTextRenderComponent> PickupLabelComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena|Test Upgrade Pickup")
	TObjectPtr<UTextRenderComponent> PickupDescriptionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena|Test Upgrade Pickup")
	TObjectPtr<URotatingMovementComponent> RotatingMovementComponent;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PickupMaterialInstance;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_UpgradeData, Category = "Arena|Test Upgrade Pickup")
	TObjectPtr<UArenaUpgradeDataAsset> UpgradeData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Test Upgrade Pickup", meta = (ClampMin = "0.0"))
	float MinimumGrantInterval = 0.25f;

private:
	// 仅服务器允许存活玩家触发，拾取物保留以支持堆叠和多人测试。
	UFUNCTION()
	void HandlePickupOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	// 升级引用复制到达时刷新本地标签表现。
	UFUNCTION()
	void OnRep_UpgradeData();

	// 仅调整本地文字组件朝向，不复制相机或玩法状态。
	void FaceTextToLocalCamera();

	// 同步球体动态材质和两行文字颜色，使不同测试构筑可直接辨认。
	void ApplyPickupDisplayColor(const FColor& DisplayColor);

	TMap<TWeakObjectPtr<AArenaPlayerState>, double> LastGrantTimeByPlayer;
};
