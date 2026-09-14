#include "GAS/ArenaGameplayAbility_Dash.h"

#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Core/ArenaPlayerState.h"
#include "Engine/World.h"
#include "GAS/ArenaAbilityNetworkDebug.h"
#include "GAS/ArenaGameplayTags.h"
#include "GAS/Targeting/ArenaTargetActor_DashDirection.h"
#include "GAS/Targeting/ArenaTargetData_DashDirection.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
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

// 先解析不会穿墙或卡进 Pawn 的合法终点，再以固定速度启动两端一致的连续 RootMotion。
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

	float ResolvedTravelDistance = 0.0f;
	if (!ResolveDashTravelDistance(Character, DashDirection, ResolvedTravelDistance)
		|| ResolvedTravelDistance <= KINDA_SMALL_NUMBER)
	{
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
		return;
	}

	const float DashSpeed = DashDistance / DashDuration;
	ActiveDashDuration = ResolvedTravelDistance / DashSpeed;
	ActiveDashCharacter = Character;
	ActiveDashASC = ActorInfo->AbilitySystemComponent.Get();
	DashCollisionStartLocation = Character->GetActorLocation();
	EnablePawnPassThrough(Character);
	bSentDashEndEvent = false;
	if (ActorInfo->IsNetAuthority())
	{
		AuthorityDashStartLocation = DashCollisionStartLocation;
	}
	ApplyDashStateTags(ActiveDashASC.Get());
	PlayDashMontage();

	if (ActorInfo->IsNetAuthority() && ArenaAbilityNetworkDebug::IsAuditEnabled())
	{
		UE_LOG(LogArenaAbilityNet, Log,
			TEXT("[%llu] Dash Key=%d Handle=%s Direction=%s Distance=%.2f Duration=%.3f"),
			ArenaAbilityNetworkDebug::NextServerExecutionSequence(),
			GetCurrentActivationInfo().GetActivationPredictionKey().Current,
			*GetCurrentAbilitySpecHandle().ToString(),
			*DashDirection.ToCompactString(),
			ResolvedTravelDistance,
			ActiveDashDuration);
	}

	UAbilityTask_ApplyRootMotionConstantForce* DashMovementTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this,
		FName(TEXT("DashMovement")),
		DashDirection,
		DashSpeed,
		ActiveDashDuration,
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
	World->GetTimerManager().SetTimer(
		DashTimerHandle,
		this,
		&UArenaGameplayAbility_Dash::FinishDash,
		ActiveDashDuration,
		false);
}

// 清理位移与无敌计时器、RootMotion、临时碰撞、状态与 Cue；取消路径不会补发 Dash 完成事件。
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

	RestorePawnCollision();
	RemoveDashStateTags();
	ActiveTargetDataTask = nullptr;
	bConsumedTargetData = false;
	AuthorityDashStartLocation = FVector::ZeroVector;
	DashCollisionStartLocation = FVector::ZeroVector;
	ActiveDashDuration = 0.0f;
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

// 添加本地预测冲刺标签，并由服务器复制权威状态与 Dash Active Cue；无敌帧使用独立窗口管理。
void UArenaGameplayAbility_Dash::ApplyDashStateTags(UAbilitySystemComponent* ASC)
{
	if (!ASC || bAppliedDashStateTags)
	{
		return;
	}

	ASC->AddLooseGameplayTag(ArenaGameplayTags::State_Dashing);
	if (ASC->IsOwnerActorAuthoritative())
	{
		ASC->AddReplicatedLooseGameplayTag(ArenaGameplayTags::State_Dashing);
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
	ApplyDashInvincibility(ASC);
}

// 移除当前 Ability 实例添加的冲刺标签、无敌帧和持续 Cue，保持预测拒绝与取消可回滚。
void UArenaGameplayAbility_Dash::RemoveDashStateTags()
{
	RemoveDashInvincibility();

	UAbilitySystemComponent* ASC = ActiveDashASC.Get();
	if (ASC && bAppliedDashStateTags)
	{
		ASC->RemoveLooseGameplayTag(ArenaGameplayTags::State_Dashing);
		if (ASC->IsOwnerActorAuthoritative())
		{
			ASC->RemoveReplicatedLooseGameplayTag(ArenaGameplayTags::State_Dashing);
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

// 无敌帧在预测端立即生效、服务器权威拒绝伤害；较短窗口会在冲刺结束前独立到期。
void UArenaGameplayAbility_Dash::ApplyDashInvincibility(UAbilitySystemComponent* ASC)
{
	if (!ASC || bAppliedDashInvincibilityTag || InvincibilityDuration <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	ASC->AddLooseGameplayTag(ArenaGameplayTags::State_Invincible);
	if (ASC->IsOwnerActorAuthoritative())
	{
		ASC->AddReplicatedLooseGameplayTag(ArenaGameplayTags::State_Invincible);
	}
	bAppliedDashInvincibilityTag = true;

	const float CurrentDashDuration = ActiveDashDuration > KINDA_SMALL_NUMBER
		? ActiveDashDuration
		: DashDuration;
	const float EffectiveDuration = FMath::Min(InvincibilityDuration, CurrentDashDuration);
	if (EffectiveDuration + KINDA_SMALL_NUMBER < CurrentDashDuration)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				InvincibilityTimerHandle,
				this,
				&UArenaGameplayAbility_Dash::RemoveDashInvincibility,
				EffectiveDuration,
				false);
		}
	}
}

// 清除独立无敌计时器并成对移除预测与复制标签，重复调用不会误减其他来源的无敌层数。
void UArenaGameplayAbility_Dash::RemoveDashInvincibility()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InvincibilityTimerHandle);
	}

	UAbilitySystemComponent* ASC = ActiveDashASC.Get();
	if (ASC && bAppliedDashInvincibilityTag)
	{
		ASC->RemoveLooseGameplayTag(ArenaGameplayTags::State_Invincible);
		if (ASC->IsOwnerActorAuthoritative())
		{
			ASC->RemoveReplicatedLooseGameplayTag(ArenaGameplayTags::State_Invincible);
		}
	}
	bAppliedDashInvincibilityTag = false;
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

// 使用世界障碍扫描和终点胶囊检查，在冲刺开始前决定唯一的合法移动距离。
bool UArenaGameplayAbility_Dash::ResolveDashTravelDistance(
	const ACharacter* Character,
	const FVector& DashDirection,
	float& OutTravelDistance) const
{
	OutTravelDistance = 0.0f;
	const UCapsuleComponent* CapsuleComponent = Character ? Character->GetCapsuleComponent() : nullptr;
	const FVector HorizontalDirection = DashDirection.GetSafeNormal2D();
	if (!Character || !CapsuleComponent || HorizontalDirection.IsNearlyZero()
		|| DashDistance <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float SearchExtension = FMath::Max(EndpointPawnPassThroughSearchDistance, 0.0f);
	const float ProbeDistance = DashDistance + SearchExtension;
	const float MaximumWorldDistance = FindWorldLimitedDashDistance(
		Character,
		CapsuleComponent,
		HorizontalDirection,
		ProbeDistance);
	const float PreferredDistance = FMath::Min(DashDistance, MaximumWorldDistance);
	if (PreferredDistance <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	return FindSafeDashEndpointDistance(
		Character,
		CapsuleComponent,
		HorizontalDirection,
		PreferredDistance,
		MaximumWorldDistance,
		OutTravelDistance);
}

// 忽略所有 Pawn 后扫描角色胶囊，使墙体在 RootMotion 启动前直接缩短本次冲刺。
float UArenaGameplayAbility_Dash::FindWorldLimitedDashDistance(
	const ACharacter* Character,
	const UCapsuleComponent* CapsuleComponent,
	const FVector& DashDirection,
	float ProbeDistance) const
{
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!World || !CapsuleComponent || ProbeDistance <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	const float CapsuleRadius = FMath::Max(CapsuleComponent->GetScaledCapsuleRadius() - 1.0f, 1.0f);
	const float CapsuleHalfHeight = FMath::Max(
		CapsuleComponent->GetScaledCapsuleHalfHeight() - 1.0f,
		CapsuleRadius);
	const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ArenaDashWorldPrecompute), false, Character);
	QueryParams.bFindInitialOverlaps = false;
	for (TActorIterator<APawn> PawnIt(World); PawnIt; ++PawnIt)
	{
		QueryParams.AddIgnoredActor(*PawnIt);
	}
	FCollisionResponseParams ResponseParams(CapsuleComponent->GetCollisionResponseToChannels());
	ResponseParams.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);

	const FVector StartLocation = Character->GetActorLocation();
	const FVector EndLocation = StartLocation + DashDirection * ProbeDistance;
	FHitResult BlockingHit;
	if (!World->SweepSingleByChannel(
		BlockingHit,
		StartLocation,
		EndLocation,
		CapsuleComponent->GetComponentQuat(),
		CapsuleComponent->GetCollisionObjectType(),
		CapsuleShape,
		QueryParams,
		ResponseParams))
	{
		return ProbeDistance;
	}

	const float HitDistance = ProbeDistance * FMath::Clamp(BlockingHit.Time, 0.0f, 1.0f);
	return FMath::Max(HitDistance - FMath::Max(ObstacleClearance, 0.0f), 0.0f);
}

// 优先在理想终点前方寻找穿敌后的安全位置，空间不足时才选择敌人前方的最近合法位置。
bool UArenaGameplayAbility_Dash::FindSafeDashEndpointDistance(
	const ACharacter* Character,
	const UCapsuleComponent* CapsuleComponent,
	const FVector& DashDirection,
	float PreferredDistance,
	float MaximumDistance,
	float& OutSafeDistance) const
{
	OutSafeDistance = 0.0f;
	if (!Character || !CapsuleComponent || PreferredDistance <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector StartLocation = Character->GetActorLocation();
	const auto IsDistanceSafe = [this, Character, CapsuleComponent, &StartLocation, &DashDirection](float Distance)
	{
		return !IsDashCapsuleBlockedAt(
			Character,
			CapsuleComponent,
			StartLocation + DashDirection * Distance);
	};

	if (IsDistanceSafe(PreferredDistance))
	{
		OutSafeDistance = PreferredDistance;
		return true;
	}

	const float SearchStep = FMath::Max(CapsuleComponent->GetScaledCapsuleRadius() * 0.25f, 5.0f);
	const float ForwardSearchDistance = FMath::Max(MaximumDistance - PreferredDistance, 0.0f);
	const int32 ForwardSteps = FMath::CeilToInt(ForwardSearchDistance / SearchStep);
	for (int32 StepIndex = 1; StepIndex <= ForwardSteps; ++StepIndex)
	{
		const float CandidateDistance = FMath::Min(
			PreferredDistance + SearchStep * StepIndex,
			MaximumDistance);
		if (IsDistanceSafe(CandidateDistance))
		{
			OutSafeDistance = CandidateDistance;
			return true;
		}
	}

	const int32 BackwardSteps = FMath::CeilToInt(PreferredDistance / SearchStep);
	for (int32 StepIndex = 1; StepIndex < BackwardSteps; ++StepIndex)
	{
		const float CandidateDistance = PreferredDistance - SearchStep * StepIndex;
		if (CandidateDistance > KINDA_SMALL_NUMBER && IsDistanceSafe(CandidateDistance))
		{
			OutSafeDistance = CandidateDistance;
			return true;
		}
	}

	return false;
}

// 保存原始 Pawn 响应后切换为重叠，使预测端和服务器都能穿过敌人且不影响墙体阻挡。
void UArenaGameplayAbility_Dash::EnablePawnPassThrough(ACharacter* Character)
{
	UCapsuleComponent* CapsuleComponent = Character ? Character->GetCapsuleComponent() : nullptr;
	if (!CapsuleComponent || bPawnCollisionChanged)
	{
		return;
	}

	PreviousPawnCollisionResponse = CapsuleComponent->GetCollisionResponseToChannel(ECC_Pawn);
	CapsuleComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	bPawnCollisionChanged = true;
}

// 处理冲刺期间动态物体闯入或客户端预测差异，正常路径不应依赖这里产生位置回退。
void UArenaGameplayAbility_Dash::ResolveDashEndOverlap()
{
	ACharacter* Character = ActiveDashCharacter.Get();
	UCapsuleComponent* CapsuleComponent = Character ? Character->GetCapsuleComponent() : nullptr;
	if (!Character || !CapsuleComponent || !bPawnCollisionChanged || PreviousPawnCollisionResponse != ECR_Block
		|| DashCollisionStartLocation.ContainsNaN())
	{
		return;
	}

	const FVector CurrentLocation = Character->GetActorLocation();
	if (!IsDashCapsuleBlockedAt(Character, CapsuleComponent, CurrentLocation))
	{
		return;
	}

	const float SearchDistance = FVector::Dist(CurrentLocation, DashCollisionStartLocation);
	const float SearchStep = FMath::Max(CapsuleComponent->GetScaledCapsuleRadius() * 0.5f, 10.0f);
	const int32 SearchSteps = FMath::Clamp(FMath::CeilToInt(SearchDistance / SearchStep), 1, 64);
	for (int32 StepIndex = 1; StepIndex <= SearchSteps; ++StepIndex)
	{
		const float Alpha = static_cast<float>(StepIndex) / static_cast<float>(SearchSteps);
		const FVector CandidateLocation = FMath::Lerp(CurrentLocation, DashCollisionStartLocation, Alpha);
		if (!IsDashCapsuleBlockedAt(Character, CapsuleComponent, CandidateLocation))
		{
			Character->SetActorLocation(CandidateLocation, false, nullptr, ETeleportType::TeleportPhysics);
			return;
		}
	}

	UE_LOG(LogArenaAbilityNet, Warning, TEXT("Dash could not find a safe collision restore location for %s."),
		*GetNameSafe(Character));
}

// 使用角色对象通道查询收缩后的完整胶囊，既检查 Pawn，也检查会阻挡角色的世界几何体。
bool UArenaGameplayAbility_Dash::IsDashCapsuleBlockedAt(
	const ACharacter* Character,
	const UCapsuleComponent* CapsuleComponent,
	const FVector& CandidateLocation) const
{
	const UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!World || !CapsuleComponent)
	{
		return true;
	}

	const float CapsuleRadius = FMath::Max(CapsuleComponent->GetScaledCapsuleRadius() - 1.0f, 1.0f);
	const float CapsuleHalfHeight = FMath::Max(CapsuleComponent->GetScaledCapsuleHalfHeight() - 1.0f, CapsuleRadius);
	const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ArenaDashCollisionRestore), false, Character);
	return World->OverlapBlockingTestByChannel(
		CandidateLocation,
		CapsuleComponent->GetComponentQuat(),
		CapsuleComponent->GetCollisionObjectType(),
		CapsuleShape,
		QueryParams);
}

// 恢复 Pawn 响应前执行一次异常保险检查，正常终点已在 RootMotion 启动前解析完成。
void UArenaGameplayAbility_Dash::RestorePawnCollision()
{
	ResolveDashEndOverlap();

	ACharacter* Character = ActiveDashCharacter.Get();
	UCapsuleComponent* CapsuleComponent = Character ? Character->GetCapsuleComponent() : nullptr;
	if (CapsuleComponent && bPawnCollisionChanged)
	{
		CapsuleComponent->SetCollisionResponseToChannel(ECC_Pawn, PreviousPawnCollisionResponse);
	}

	bPawnCollisionChanged = false;
	PreviousPawnCollisionResponse = ECR_Block;
}
