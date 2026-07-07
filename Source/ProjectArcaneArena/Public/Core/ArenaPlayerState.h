#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "ArenaPlayerState.generated.h"

class UArenaAbilitySystemComponent;
class UArenaAttributeSet;
class UAbilitySystemComponent;

UCLASS()
class PROJECTARCANEARENA_API AArenaPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AArenaPlayerState();

	// PlayerState 拥有玩家 ASC，方便未来死亡重生时保留长期 GAS 状态。
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "Arena|GAS")
	UArenaAbilitySystemComponent* GetArenaAbilitySystemComponent() const;

	UFUNCTION(BlueprintPure, Category = "Arena|GAS")
	UArenaAttributeSet* GetArenaAttributeSet() const;

	// 防止重复授予启动技能，后续重生流程会复用该状态。
	bool HasGrantedStartupAbilities() const { return bGrantedStartupAbilities; }
	void SetGrantedStartupAbilities(bool bNewGrantedStartupAbilities);

	// 防止重复应用默认属性，后续可替换为 Init GameplayEffect 流程。
	bool HasAppliedDefaultAttributes() const { return bAppliedDefaultAttributes; }
	void SetAppliedDefaultAttributes(bool bNewAppliedDefaultAttributes);

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UArenaAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UArenaAttributeSet> AttributeSet;

	bool bGrantedStartupAbilities = false;
	bool bAppliedDefaultAttributes = false;
};
