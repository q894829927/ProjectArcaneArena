#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetActor.h"
#include "ArenaTargetActor_MouseGround.generated.h"

UCLASS()
class PROJECTARCANEARENA_API AArenaTargetActor_MouseGround : public AGameplayAbilityTargetActor
{
	GENERATED_BODY()

public:
	// 配置 TargetActor 为客户端产生 TargetData 的轻量对象。
	AArenaTargetActor_MouseGround();

	// 初始化目标选择上下文，缓存当前 Avatar 作为 TargetData 的源位置。
	virtual void StartTargeting(UGameplayAbility* Ability) override;

	// 立即采集鼠标地面位置，并广播 LocationInfo TargetData 给 WaitTargetData。
	virtual void ConfirmTargetingAndContinue() override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Targeting", meta = (ClampMin = "0.0"))
	float FallbackDistance = 2200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Targeting")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

private:
	// 优先读取鼠标所在世界位置，失败时使用角色朝向生成一个兜底点。
	bool GetMouseGroundLocation(FVector& OutTargetLocation) const;

	// 只包装源点和目标点，不在 TargetActor 中保存玩法状态或执行伤害。
	FGameplayAbilityTargetDataHandle MakeLocationTargetData(const FVector& TargetLocation) const;
};
