#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "ArenaCharacterBase.generated.h"

class AArenaDamageNumberActor;
class UArenaHitReactionComponent;

UCLASS(Abstract)
class PROJECTARCANEARENA_API AArenaCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AArenaCharacterBase();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// 返回所有角色共用的本地受击表现组件。
	UArenaHitReactionComponent* GetHitReactionComponent() const { return HitReactionComponent; }

	// 允许已有角色蓝图向公共组件提供兼容的伤害数字类。
	virtual TSubclassOf<AArenaDamageNumberActor> GetDamageNumberActorClassForFeedback() const;
	// 允许角色类型保留已有数字高度，默认使用组件配置。
	virtual FVector GetDamageNumberSpawnOffsetForFeedback(const FVector& ComponentDefault) const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena|Feedback")
	TObjectPtr<UArenaHitReactionComponent> HitReactionComponent;
};
