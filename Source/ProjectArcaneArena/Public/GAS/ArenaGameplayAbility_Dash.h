#pragma once

#include "CoreMinimal.h"
#include "GAS/ArenaGameplayAbility.h"
#include "TimerManager.h"
#include "ArenaGameplayAbility_Dash.generated.h"

class ACharacter;
class UAbilitySystemComponent;
class UAnimMontage;

UCLASS(Blueprintable)
class PROJECTARCANEARENA_API UArenaGameplayAbility_Dash : public UArenaGameplayAbility
{
	GENERATED_BODY()

public:
	UArenaGameplayAbility_Dash();

protected:
	// 预测拥有者的冲刺移动和 Montage，最终冷却、状态和位置仍由服务器确认。
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	// Clears dash timers and tags for both natural completion and cancellation.
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Dash", meta = (ClampMin = "0.0"))
	float DashDistance = 500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Dash", meta = (ClampMin = "0.01"))
	float DashDuration = 0.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Dash|Animation")
	TObjectPtr<UAnimMontage> DashMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Dash|Animation", meta = (ClampMin = "0.01"))
	float DashMontagePlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Dash|Animation")
	FName DashMontageStartSection = NAME_None;

private:
	FVector ResolveDashDirection(AActor* AvatarActor) const;
	void PlayDashMontage();
	void ApplyDashStateTags(UAbilitySystemComponent* ASC);
	void RemoveDashStateTags();
	void FinishDash();
	void StopDashMovement(ACharacter* Character) const;

	FTimerHandle DashTimerHandle;
	TWeakObjectPtr<ACharacter> ActiveDashCharacter;
	TWeakObjectPtr<UAbilitySystemComponent> ActiveDashASC;
	bool bAppliedDashStateTags = false;
};
