#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ArenaItemDataAsset.generated.h"

class AArenaInventoryPickupActor;
class UGameplayEffect;
class UMaterialInterface;
class UStaticMesh;
class UTexture2D;

UCLASS(BlueprintType)
class PROJECTARCANEARENA_API UArenaItemDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	// 校验物品身份、堆叠和使用事务，供 Pickup、背包 Model 与编辑器共用。
	bool IsRuntimeDefinitionValid(FText* OutError = nullptr) const;

	// 校验恢复路由、数值及 Use/Cooldown GE 生命周期，供服务器事务和编辑器验证共用。
	bool IsUseConfigurationValid(FText* OutError = nullptr) const;

#if WITH_EDITOR
	// 在 Content Validation 阶段拒绝会导致运行时恢复、冷却或世界表现失效的物品配置。
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Item")
	FGameplayTag ItemTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Item")
	FGameplayTagContainer ItemTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Item")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Item", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Item")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Item", meta = (ClampMin = "1"))
	int32 MaxStackSize = 10;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Item|Use")
	TSubclassOf<UGameplayEffect> UseGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Item|Use")
	FGameplayTag SetByCallerMagnitudeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Item|Use", meta = (ClampMin = "0.0"))
	float UseMagnitude = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Item|Use")
	TSubclassOf<UGameplayEffect> CooldownGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Item|World")
	TSubclassOf<AArenaInventoryPickupActor> WorldPickupClass;

	// 为原生或蓝图 Pickup 提供统一世界网格，避免 DataAsset 与专属 Pickup Blueprint 形成硬引用环。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Item|World")
	TSoftObjectPtr<UStaticMesh> WorldMesh;

	// 按 StaticMesh 材质槽顺序覆盖世界表现；空项保留网格默认材质。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Item|World")
	TArray<TSoftObjectPtr<UMaterialInterface>> WorldMaterials;

	// 数据驱动设置世界网格相对位置，碰撞仍由 Pickup Actor 的根组件负责。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Item|World")
	FVector WorldMeshRelativeLocation = FVector::ZeroVector;

	// 数据驱动设置世界网格相对旋转，不改变复制 Actor 的玩法朝向。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Item|World")
	FRotator WorldMeshRelativeRotation = FRotator::ZeroRotator;

	// 数据驱动设置世界网格相对缩放，药水等不同资源可复用同一 Pickup Actor。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Item|World")
	FVector WorldMeshRelativeScale = FVector(0.25f);
};
