#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetActor.h"
#include "ArenaTargetActor_DashDirection.generated.h"

UCLASS()
class PROJECTARCANEARENA_API AArenaTargetActor_DashDirection : public AGameplayAbilityTargetActor
{
	GENERATED_BODY()

public:
	AArenaTargetActor_DashDirection();

	// 缓存当前 Avatar，供即时采集移动方向时使用。
	virtual void StartTargeting(UGameplayAbility* Ability) override;

	// 按加速度、最近移动输入、角色朝向的顺序生成单条规范化方向 TargetData。
	virtual void ConfirmTargetingAndContinue() override;

private:
	FVector ResolveLocalDashDirection() const;
};
