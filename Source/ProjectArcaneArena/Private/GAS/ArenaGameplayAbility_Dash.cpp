#include "GAS/ArenaGameplayAbility_Dash.h"

#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "Character/ArenaPlayerCharacter.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "TimerManager.h"

// 构造可预测冲刺技能，让拥有者即时播放表现，同时保留服务器确认和位置校正。
UArenaGameplayAbility_Dash::UArenaGameplayAbility_Dash()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InputTag = ArenaGameplayTags::Ability_Dash;

	SetAssetTags(FGameplayTagContainer(ArenaGameplayTags::Ability_Dash));
	ActivationBlockedTags.AddTag(ArenaGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(ArenaGameplayTags::State_Stunned);
	ActivationBlockedTags.AddTag(ArenaGameplayTags::State_Dashing);
	ActivationBlockedTags.AddTag(ArenaGameplayTags::Cooldown_Dash);
}

// 激活冲刺：客户端预测 Montage/RootMotion，服务器执行同一路径并拥有最终冷却、无敌和位置结果。
void UArenaGameplayAbility_Dash::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid() || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	if (!Character || DashDistance <= 0.0f || DashDuration <= KINDA_SMALL_NUMBER)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UWorld* World = Character->GetWorld();
	if (!World)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FVector DashDirection = ResolveDashDirection(Character);
	if (DashDirection.IsNearlyZero())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveDashCharacter = Character;
	ActiveDashASC = ActorInfo->AbilitySystemComponent.Get();
	ApplyDashStateTags(ActiveDashASC.Get());
	PlayDashMontage();

	const float DashSpeed = DashDistance / DashDuration;
	UAbilityTask_ApplyRootMotionConstantForce* DashMovementTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this,
		FName(TEXT("DashMovement")),
		DashDirection,
		DashSpeed,
		DashDuration,
		false,
		nullptr,
		ERootMotionFinishVelocityMode::SetVelocity,
		FVector::ZeroVector,
		0.0f,
		false);
	if (!DashMovementTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	DashMovementTask->ReadyForActivation();

	World->GetTimerManager().SetTimer(
		DashTimerHandle,
		this,
		&UArenaGameplayAbility_Dash::FinishDash,
		DashDuration,
		false);
}

// 结束冲刺技能时清理定时器、停止移动并移除冲刺状态标签。
void UArenaGameplayAbility_Dash::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DashTimerHandle);
	}

	if (ActiveDashCharacter.IsValid())
	{
		StopDashMovement(ActiveDashCharacter.Get());
	}

	RemoveDashStateTags();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// 解析冲刺方向，优先使用移动输入，缺省时回退到角色朝向。
FVector UArenaGameplayAbility_Dash::ResolveDashDirection(AActor* AvatarActor) const
{
	if (!AvatarActor)
	{
		return FVector::ZeroVector;
	}

	FVector DashDirection = FVector::ZeroVector;
	if (const ACharacter* Character = Cast<ACharacter>(AvatarActor))
	{
		if (const UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement())
		{
			DashDirection = MovementComponent->GetCurrentAcceleration();
		}
	}

	if (const AArenaPlayerCharacter* PlayerCharacter = Cast<AArenaPlayerCharacter>(AvatarActor))
	{
		if (DashDirection.IsNearlyZero())
		{
			DashDirection = PlayerCharacter->GetLastMovementInputDirection();
		}
	}

	if (DashDirection.IsNearlyZero())
	{
		DashDirection = AvatarActor->GetActorForwardVector();
	}

	DashDirection.Z = 0.0f;
	return DashDirection.GetSafeNormal();
}

// 在预测端和服务器播放同一 Montage，GAS 使用 PredictionKey 避免拥有者重复播放。
void UArenaGameplayAbility_Dash::PlayDashMontage()
{
	if (!DashMontage)
	{
		return;
	}

	// Dash Montage 只负责表现，预测与服务器校正位移由 RootMotion 任务控制。
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		FName(TEXT("DashMontage")),
		DashMontage,
		DashMontagePlayRate,
		DashMontageStartSection,
		false,
		0.0f);
	if (MontageTask)
	{
		MontageTask->ReadyForActivation();
	}
}

// 两端添加本地预测标签，仅服务器写 replicated loose tags 供其他客户端观察。
void UArenaGameplayAbility_Dash::ApplyDashStateTags(UAbilitySystemComponent* ASC)
{
	if (!ASC || bAppliedDashStateTags)
	{
		return;
	}

	// Ability 拥有的状态标签让无敌时间严格绑定在当前冲刺窗口。
	ASC->AddLooseGameplayTag(ArenaGameplayTags::State_Dashing);
	ASC->AddLooseGameplayTag(ArenaGameplayTags::State_Invincible);
	if (ASC->IsOwnerActorAuthoritative())
	{
		ASC->AddReplicatedLooseGameplayTag(ArenaGameplayTags::State_Dashing);
		ASC->AddReplicatedLooseGameplayTag(ArenaGameplayTags::State_Invincible);
	}
	bAppliedDashStateTags = true;
}

// 两端移除本地标签，仅服务器移除 replicated loose tags，并清理缓存引用。
void UArenaGameplayAbility_Dash::RemoveDashStateTags()
{
	UAbilitySystemComponent* ASC = ActiveDashASC.Get();
	if (!ASC || !bAppliedDashStateTags)
	{
		return;
	}

	ASC->RemoveLooseGameplayTag(ArenaGameplayTags::State_Dashing);
	ASC->RemoveLooseGameplayTag(ArenaGameplayTags::State_Invincible);
	if (ASC->IsOwnerActorAuthoritative())
	{
		ASC->RemoveReplicatedLooseGameplayTag(ArenaGameplayTags::State_Dashing);
		ASC->RemoveReplicatedLooseGameplayTag(ArenaGameplayTags::State_Invincible);
	}

	bAppliedDashStateTags = false;
	ActiveDashASC.Reset();
	ActiveDashCharacter.Reset();
}

// 定时器回调：结束 RootMotion 位移并正常结束 Ability。
void UArenaGameplayAbility_Dash::FinishDash()
{
	if (ActiveDashCharacter.IsValid())
	{
		StopDashMovement(ActiveDashCharacter.Get());
	}

	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

// 停止角色当前移动，避免冲刺结束后残留速度。
void UArenaGameplayAbility_Dash::StopDashMovement(ACharacter* Character) const
{
	if (!Character)
	{
		return;
	}

	if (UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
	}
}
