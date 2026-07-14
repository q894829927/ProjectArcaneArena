#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaPickupActor.generated.h"

class AArenaPlayerCharacter;
class UPrimitiveComponent;
class URotatingMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EArenaPickupType : uint8
{
	Health,
	Energy
};

UCLASS(Blueprintable)
class PROJECTARCANEARENA_API AArenaPickupActor : public AActor
{
	GENERATED_BODY()

public:
	// 创建可复制的自动拾取物组件，并提供基础旋转占位表现。
	AArenaPickupActor();

protected:
	// 仅服务器启动生命周期计时，销毁结果通过 Actor 复制同步给客户端。
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena|Pickup")
	TObjectPtr<USphereComponent> PickupCollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena|Pickup")
	TObjectPtr<UStaticMeshComponent> PickupMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena|Pickup")
	TObjectPtr<URotatingMovementComponent> RotatingMovementComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Pickup")
	EArenaPickupType PickupType = EArenaPickupType::Health;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Pickup", meta = (ClampMin = "0.0"))
	float RestoreAmount = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Pickup", meta = (ClampMin = "0.0"))
	float PickupLifeSpan = 15.0f;

private:
	// Authority 过滤玩家和资源状态，并保证共享拾取物最多被消费一次。
	UFUNCTION()
	void HandlePickupOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	// 通过目标玩家 ASC 应用对应恢复 GE，只有属性实际增加才返回成功。
	bool TryApplyRestore(AArenaPlayerCharacter* PlayerCharacter);

	bool bConsumed = false;
};
