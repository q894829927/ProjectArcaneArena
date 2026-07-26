#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "GAS/ArenaAbilityNetworkDebug.h"
#include "ArenaGameplayAbility.generated.h"

UCLASS(Abstract, Blueprintable)
class PROJECTARCANEARENA_API UArenaGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UArenaGameplayAbility();

	// 统一阻止 BossIntro 战斗技能，并在服务器 CanActivate 阶段注入开发拒绝以验证预测回滚。
	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	// Ability 输入标签由 Character/ASC 用来把本地输入路由到对应技能。
	const FGameplayTag& GetInputTag() const { return InputTag; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Input")
	FGameplayTag InputTag;

	EArenaNetworkAbilityId NetworkAbilityId = EArenaNetworkAbilityId::None;
};
