#pragma once

#include "CoreMinimal.h"
#include "Character/ArenaCharacterBase.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "ArenaEnemyCharacter.generated.h"

class AArenaEnemyCharacter;
class AArenaDamageNumberActor;
class UArenaAbilitySystemComponent;
class UArenaAttributeSet;
class UArenaEnemyHealthBarWidget;
class UGameplayEffect;
class UAbilitySystemComponent;
class UWidgetComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FArenaEnemyDeathSignature, AArenaEnemyCharacter*, Enemy);

UCLASS()
class PROJECTARCANEARENA_API AArenaEnemyCharacter : public AArenaCharacterBase
{
	GENERATED_BODY()

public:
	AArenaEnemyCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Enemy")
	FArenaEnemyDeathSignature OnEnemyDeath;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TSubclassOf<UGameplayEffect> DefaultAttributeEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Death", meta = (ClampMin = "0.0"))
	float DeathLifeSpan = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Feedback")
	TSubclassOf<AArenaDamageNumberActor> DamageNumberActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Feedback")
	FVector DamageNumberSpawnOffset = FVector(0.0f, 0.0f, 130.0f);

	UFUNCTION(BlueprintImplementableEvent, Category = "Arena|Enemy")
	void K2_OnDeathStarted();

	UFUNCTION(BlueprintImplementableEvent, Category = "Arena|Enemy")
	void K2_OnHealthChanged(float OldHealth, float NewHealth, float MaxHealth);

	UFUNCTION(BlueprintImplementableEvent, Category = "Arena|Enemy")
	void K2_OnDamaged(float DamageAmount, float NewHealth, float MaxHealth);

private:
	void InitializeAbilityActorInfo();
	void ApplyDefaultAttributes();
	void BindAbilitySystemDelegates();
	void UnbindAbilitySystemDelegates();
	void HandleDeadTagChanged(const FGameplayTag CallbackTag, int32 NewCount);
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void HandleDeath();
	void RefreshHealthBar();
	void SetHealthBarValues(float Health, float MaxHealth);
	void SpawnDamageNumber(float DamageAmount);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UArenaAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UArenaAttributeSet> AttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetComponent> HealthBarWidgetComponent;

	FDelegateHandle DeadTagDelegateHandle;
	FDelegateHandle HealthChangedDelegateHandle;

	bool bAppliedDefaultAttributes = false;
	bool bDeathHandled = false;
};
