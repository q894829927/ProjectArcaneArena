#include "GAS/ArenaGameplayAbility_Dash.h"

#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SceneComponent.h"
#include "Core/ArenaPlayerState.h"
#include "GAS/ArenaAbilityNetworkDebug.h"
#include "GAS/ArenaGameplayTags.h"
#include "GAS/Targeting/ArenaTargetActor_DashDirection.h"
#include "GAS/Targeting/ArenaTargetData_DashDirection.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "GameplayEffect.h"
#include "TimerManager.h"

// 配置本地预测冲刺、方向 TargetData 和仅服务器可结束的权威生命周期。
UArenaGameplayAbility_Dash::UArenaGameplayAbility_Dash()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	// 预测端可在本地停止位移与表现，但不得抢先结束服务器实例并吞掉 OnDashEnd。
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnlyTermination;
	NetworkAbilityId = EArenaNetworkAbilityId::Dash;
	InputTag = ArenaGameplayTags::Ability_Dash;
	DashDirectionTargetActorClass = AArenaTargetActor_DashDirection::StaticClass();

	// Dash 会产生通用玩家施放事件，但当前没有 Energy Cost，因此不标记为 EnergySkill。
	FGameplayTagContainer AbilityAssetTags(ArenaGameplayTags::Ability_Dash);
	AbilityAssetTags.AddTag(ArenaGameplayTags::Ability_Type_PlayerActive);
	SetAssetTags(AbilityAssetTags);
	ActivationBlockedTags.AddTag(ArenaGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(ArenaGameplayTags::State_Stunned);
	ActivationBlockedTags.AddTag(ArenaGameplayTags::State_Dashing);
	ActivationBlockedTags.AddTag(ArenaGameplayTags::Cooldown_Dash);
}

// 启动方向 TargetActor；客户端提交规范化方向，服务器等待并复用相同 TargetData。
void UArenaGameplayAbility_Dash::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid() || !ActorInfo->AbilitySystemComponent.IsValid()
		|| !Cast<ACharacter>(ActorInfo->AvatarActor.Get()) || !DashDirectionTargetActorClass
		|| DashDistance <= 0.0f || DashDuration <= KINDA_SMALL_NUMBER)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bConsumedTargetData = false;
	UAbilityTask_WaitTargetData* TargetDataTask = UAbilityTask_WaitTargetData::WaitTargetData(
		this,
		FName(TEXT("DashDirectionTargetData")),
		EGameplayTargetingConfirmation::Instant,
		DashDirectionTargetActorClass);
	if (!TargetDataTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveTargetDataTask = TargetDataTask;
	TargetDataTask->ValidData.AddDynamic(this, &UArenaGameplayAbility_Dash::OnDashTargetDataReady);
	TargetDataTask->Cancelled.AddDynamic(this, &UArenaGameplayAbility_Dash::OnDashTargetDataCancelled);
	TargetDataTask->ReadyForActivation();

	AGameplayAbilityTargetActor* SpawnedTargetActor = nullptr;
	if (TargetDataTask->BeginSpawningActor(this, DashDirectionTargetActorClass, SpawnedTargetActor))
	{
		TargetDataTask->FinishSpawningActor(this, SpawnedTargetActor);
	}
	else if (ActorInfo->IsLocallyControlled())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

// 消费一次方向数据并在两端 Commit 相同 Cost/Cooldown 后启动冲刺。
void UArenaGameplayAbility_Dash::OnDashTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData)
{
	ActiveTargetDataTask = nullptr;
	if (bConsumedTargetData)
	{
		return;
	}
	bConsumedTargetData = true;

	FVector DashDirection = FVector::ZeroVector;
	if (!ExtractAndValidateDashDirection(TargetData, DashDirection))
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!CommitAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo()))
	{
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
		return;
	}

	StartDashWithDirection(DashDirection);
}

// 取消目标采集时沿 GAS 正常取消路径结束，不触发 OnDashEnd。
void UArenaGameplayAbility_Dash::OnDashTargetDataCancelled(const FGameplayAbilityTargetDataHandle& TargetData)
{
	ActiveTargetDataTask = nullptr;
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
}

// 使用校验后的方向创建 RootMotion；服务器额外记录实际起点供完成事件使用。
void UArenaGameplayAbility_Dash::StartDashWithDirection(const FVector& DashDirection)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!Character || !World || DashDirection.IsNearlyZero())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
		return;
	}

	ActiveDashCharacter = Character;
	ActiveDashASC = ActorInfo->AbilitySystemComponent.Get();
	bSentDashEndEvent = false;
	if (ActorInfo->IsNetAuthority())
	{
		AuthorityDashStartLocation = Character->GetActorLocation();
	}
	ApplyDashStateTags(ActiveDashASC.Get());
	PlayDashMontage();

	if (ActorInfo->IsNetAuthority() && ArenaAbilityNetworkDebug::IsAuditEnabled())
	{
		UE_LOG(LogArenaAbilityNet, Log, TEXT("[%llu] Dash Key=%d Handle=%s Direction=%s"),
			ArenaAbilityNetworkDebug::NextServerExecutionSequence(),
			GetCurrentActivationInfo().GetActivationPredictionKey().Current,
			*GetCurrentAbilitySpecHandle().ToString(),
			*DashDirection.ToCompactString());
	}

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
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
		return;
	}

	DashMovementTask->ReadyForActivation();
	World->GetTimerManager().SetTimer(DashTimerHandle, this, &UArenaGameplayAbility_Dash::FinishDash, DashDuration, false);
}

// 清理计时器、RootMotion、状态与 Cue；取消路径不会补发 Dash 完成事件。
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
	ActiveTargetDataTask = nullptr;
	bConsumedTargetData = false;
	AuthorityDashStartLocation = FVector::ZeroVector;
	bSentDashEndEvent = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// 在预测端和服务器从同一 Upgrade DataAsset 层数计算 Cooldown Spec 的最终持续时间。
void UArenaGameplayAbility_Dash::ApplyCooldown(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	UGameplayEffect* CooldownEffect = GetCooldownGameplayEffect();
	if (!CooldownEffect)
	{
		return;
	}

	const float CooldownReduction = GetDashCooldownReduction(ActorInfo);
	if (CooldownReduction <= KINDA_SMALL_NUMBER)
	{
		Super::ApplyCooldown(Handle, ActorInfo, ActivationInfo);
		return;
	}

	FGameplayEffectSpecHandle CooldownSpecHandle = MakeOutgoingGameplayEffectSpec(
		Handle,
		ActorInfo,
		ActivationInfo,
		CooldownEffect->GetClass(),
		GetAbilityLevel(Handle, ActorInfo));
	if (!CooldownSpecHandle.IsValid())
	{
		return;
	}

	FGameplayEffectSpec* CooldownSpec = CooldownSpecHandle.Data.Get();
	const float BaseDuration = CooldownSpec->GetDuration();
	if (BaseDuration > 0.0f)
	{
		const float CooldownFloor = FMath::Min(MinimumCooldownDuration, BaseDuration);
		CooldownSpec->SetDuration(
			FMath::Max(BaseDuration * (1.0f - CooldownReduction), CooldownFloor),
			true);
	}
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, CooldownSpecHandle);
}

// 对客户端方向数据执行数量、类型、有限值和水平分量校验。
bool UArenaGameplayAbility_Dash::ExtractAndValidateDashDirection(
	const FGameplayAbilityTargetDataHandle& TargetData,
	FVector& OutDirection) const
{
	if (TargetData.Num() != 1)
	{
		return false;
	}

	const FGameplayAbilityTargetData* RawData = TargetData.Get(0);
	if (!RawData || RawData->GetScriptStruct() != FGameplayAbilityTargetData_DashDirection::StaticStruct())
	{
		return false;
	}

	const FGameplayAbilityTargetData_DashDirection* DirectionData =
		static_cast<const FGameplayAbilityTargetData_DashDirection*>(RawData);
	const FVector SubmittedDirection = DirectionData->Direction;
	if (SubmittedDirection.ContainsNaN() || SubmittedDirection.SizeSquared() < 0.25f
		|| FMath::Abs(SubmittedDirection.Z) > 0.1f)
	{
		return false;
	}

	OutDirection = SubmittedDirection.GetSafeNormal2D();
	return !OutDirection.IsNearlyZero();
}

// Montage 只负责两端表现，Dash 完成时机由服务器与预测端的位移计时器管理。
void UArenaGameplayAbility_Dash::PlayDashMontage()
{
	if (!DashMontage)
	{
		return;
	}

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		FName(TEXT("DashMontage")),
		DashMontage,
		DashMontagePlayRate,
		DashMontageStartSection,
		true,
		0.0f);
	if (MontageTask)
	{
		MontageTask->ReadyForActivation();
	}
}

// 添加本地预测标签，并由服务器复制权威标签与 Dash Active Cue。
void UArenaGameplayAbility_Dash::ApplyDashStateTags(UAbilitySystemComponent* ASC)
{
	if (!ASC || bAppliedDashStateTags)
	{
		return;
	}

	ASC->AddLooseGameplayTag(ArenaGameplayTags::State_Dashing);
	ASC->AddLooseGameplayTag(ArenaGameplayTags::State_Invincible);
	if (ASC->IsOwnerActorAuthoritative())
	{
		ASC->AddReplicatedLooseGameplayTag(ArenaGameplayTags::State_Dashing);
		ASC->AddReplicatedLooseGameplayTag(ArenaGameplayTags::State_Invincible);
		FGameplayCueParameters CueParameters;
		AActor* CueAvatar = ASC->GetAvatarActor();
		CueParameters.Instigator = CueAvatar;
		CueParameters.EffectCauser = CueAvatar;
		// 持续冲刺特效附着到角色根组件，避免继承 Manny Mesh 的导入旋转和相对位移。
		CueParameters.TargetAttachComponent = CueAvatar ? CueAvatar->GetRootComponent() : nullptr;
		ASC->AddGameplayCue(ArenaGameplayTags::GameplayCue_Ability_Dash_Active, CueParameters);
		bAddedDashGameplayCue = true;
		if (ArenaAbilityNetworkDebug::IsAuditEnabled())
		{
			UE_LOG(LogArenaAbilityNet, Log, TEXT("[%llu] DashCue Added Avatar=%s"),
				ArenaAbilityNetworkDebug::NextServerExecutionSequence(),
				*GetNameSafe(CueAvatar));
		}
	}
	bAppliedDashStateTags = true;
}

// 移除当前 Ability 实例添加的标签和持续 Cue，保持预测拒绝与取消可回滚。
void UArenaGameplayAbility_Dash::RemoveDashStateTags()
{
	UAbilitySystemComponent* ASC = ActiveDashASC.Get();
	if (ASC && bAppliedDashStateTags)
	{
		ASC->RemoveLooseGameplayTag(ArenaGameplayTags::State_Dashing);
		ASC->RemoveLooseGameplayTag(ArenaGameplayTags::State_Invincible);
		if (ASC->IsOwnerActorAuthoritative())
		{
			ASC->RemoveReplicatedLooseGameplayTag(ArenaGameplayTags::State_Dashing);
			ASC->RemoveReplicatedLooseGameplayTag(ArenaGameplayTags::State_Invincible);
			if (bAddedDashGameplayCue)
			{
				ASC->RemoveGameplayCue(ArenaGameplayTags::GameplayCue_Ability_Dash_Active);
				if (ArenaAbilityNetworkDebug::IsAuditEnabled())
				{
					UE_LOG(LogArenaAbilityNet, Log, TEXT("[%llu] DashCue Removed Avatar=%s"),
						ArenaAbilityNetworkDebug::NextServerExecutionSequence(),
						*GetNameSafe(ASC->GetAvatarActor()));
				}
			}
		}
	}

	bAppliedDashStateTags = false;
	bAddedDashGameplayCue = false;
	ActiveDashASC.Reset();
	ActiveDashCharacter.Reset();
}

// 正常结束先记录实际终点并发送服务器事件，再清理位移和 Ability 状态。
void UArenaGameplayAbility_Dash::FinishDash()
{
	if (ActiveDashCharacter.IsValid())
	{
		StopDashMovement(ActiveDashCharacter.Get());
	}
	SendDashEndEvent();

	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

// 服务器把实际冲刺起终点封装为 Location TargetData，使事件被动无需读取预测状态。
void UArenaGameplayAbility_Dash::SendDashEndEvent()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	ACharacter* Character = ActiveDashCharacter.Get();
	if (bSentDashEndEvent || !ActorInfo || !ActorInfo->IsNetAuthority() || !Character)
	{
		return;
	}
	bSentDashEndEvent = true;

	const FVector DashEndLocation = Character->GetActorLocation();
	FGameplayAbilityTargetData_LocationInfo* LocationData = new FGameplayAbilityTargetData_LocationInfo();
	LocationData->SourceLocation.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;
	LocationData->SourceLocation.LiteralTransform = FTransform(AuthorityDashStartLocation);
	LocationData->TargetLocation.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;
	LocationData->TargetLocation.LiteralTransform = FTransform(DashEndLocation);

	FGameplayEventData EventData;
	EventData.EventTag = ArenaGameplayTags::Trigger_OnDashEnd;
	EventData.Instigator = Character;
	EventData.Target = Character;
	EventData.EventMagnitude = FVector::Dist2D(AuthorityDashStartLocation, DashEndLocation);
	EventData.TargetData.Add(LocationData);
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		Character,
		ArenaGameplayTags::Trigger_OnDashEnd,
		EventData);
}

// 读取 Owner PlayerState 的数据驱动升级总值，并限制在不会把 Cooldown 降到零的范围。
float UArenaGameplayAbility_Dash::GetDashCooldownReduction(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const AArenaPlayerState* ArenaPlayerState = ActorInfo
		? Cast<AArenaPlayerState>(ActorInfo->OwnerActor.Get())
		: nullptr;
	const float Reduction = ArenaPlayerState
		? ArenaPlayerState->GetOwnedUpgradeNumericTotal(
			ArenaGameplayTags::Ability_Dash,
			FGameplayTag(),
			ArenaGameplayTags::Upgrade_Dash_Cooldown)
		: 0.0f;
	return FMath::Clamp(Reduction, 0.0f, 0.8f);
}

// 清除 RootMotion 结束后的剩余速度，保持服务端与预测端停止行为一致。
void UArenaGameplayAbility_Dash::StopDashMovement(ACharacter* Character) const
{
	if (Character && Character->GetCharacterMovement())
	{
		Character->GetCharacterMovement()->StopMovementImmediately();
	}
}
