#include "GAS/ArenaGameplayAbility_BasicAttack.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "Animation/AnimMontage.h"
#include "DrawDebugHelpers.h"
#include "GAS/ArenaAbilityNetworkDebug.h"
#include "GAS/ArenaGameplayTags.h"
#include "GAS/Targeting/ArenaTargetActor_MouseGround.h"
#include "GameplayEffect.h"

// 构造基础攻击技能，配置本地预测瞄准、输入标签和激活阻断标签。
UArenaGameplayAbility_BasicAttack::UArenaGameplayAbility_BasicAttack()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	NetworkAbilityId = EArenaNetworkAbilityId::BasicAttack;
	InputTag = ArenaGameplayTags::Ability_BasicAttack;
	DamageTypeTag = ArenaGameplayTags::Damage_Physical;
	TargetActorClass = AArenaTargetActor_MouseGround::StaticClass();

	// 主动技能分类由 ASC 的 Commit 钩子统一路由 OnAbilityCast，普攻不属于 EnergySkill。
	FGameplayTagContainer AbilityAssetTags(ArenaGameplayTags::Ability_BasicAttack);
	AbilityAssetTags.AddTag(ArenaGameplayTags::Ability_Type_PlayerActive);
	SetAssetTags(AbilityAssetTags);
	ActivationBlockedTags.AddTag(ArenaGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(ArenaGameplayTags::State_Stunned);
	ActivationBlockedTags.AddTag(ArenaGameplayTags::Cooldown_BasicAttack);
}

// 激活基础攻击，启动视角感知的即时 TargetData 采集。
void UArenaGameplayAbility_BasicAttack::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bConsumedTargetData = false;
	bServerAttackExecuted = false;

	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid() || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!TargetActorClass || !DamageEffectClass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilityTask_WaitTargetData* TargetDataTask = UAbilityTask_WaitTargetData::WaitTargetData(
		this,
		FName(TEXT("BasicAttackTargetData")),
		EGameplayTargetingConfirmation::Instant,
		TargetActorClass);
	if (!TargetDataTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveTargetDataTask = TargetDataTask;
	TargetDataTask->ValidData.AddDynamic(this, &UArenaGameplayAbility_BasicAttack::OnTargetDataReady);
	TargetDataTask->Cancelled.AddDynamic(this, &UArenaGameplayAbility_BasicAttack::OnTargetDataCancelled);
	TargetDataTask->ReadyForActivation();

	AGameplayAbilityTargetActor* SpawnedTargetActor = nullptr;
	const bool bSpawnedTargetActor = TargetDataTask->BeginSpawningActor(this, TargetActorClass, SpawnedTargetActor);
	if (bSpawnedTargetActor)
	{
		TargetDataTask->FinishSpawningActor(this, SpawnedTargetActor);
	}
	else if (ActorInfo->IsLocallyControlled())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

// TargetData 有效后提交预测成本，立即同步朝向、起手 Cue 和 Montage，再由服务器执行权威 Sweep。
void UArenaGameplayAbility_BasicAttack::OnTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData)
{
	ActiveTargetDataTask = nullptr;
	if (bConsumedTargetData)
	{
		return;
	}
	bConsumedTargetData = true;

	const FGameplayAbilitySpecHandle Handle = GetCurrentAbilitySpecHandle();
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const FGameplayAbilityActivationInfo ActivationInfo = GetCurrentActivationInfo();
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid() || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	FVector AimDirection = FVector::ZeroVector;
	if (!ExtractAimDirection(TargetData, AvatarActor, AimDirection))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 本地立即转身改善输入反馈；服务器收到相同 TargetData 后会设置权威旋转。
	if (ActorInfo->IsLocallyControlled())
	{
		AvatarActor->SetActorRotation(AimDirection.Rotation());
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AvatarActor->SetActorRotation(AimDirection.Rotation());
	ExecuteAttackActivationCue(AvatarActor, ActorInfo->AbilitySystemComponent.Get());
	PlayAttackMontage();
	if (ActorInfo->IsNetAuthority() && !bServerAttackExecuted)
	{
		bServerAttackExecuted = true;
		ExecuteServerAttack(AvatarActor, ActorInfo->AbilitySystemComponent.Get(), AimDirection);
	}

	if (!AttackMontage)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

// TargetData 取消时清理任务并取消当前普攻。
void UArenaGameplayAbility_BasicAttack::OnTargetDataCancelled(const FGameplayAbilityTargetDataHandle& TargetData)
{
	ActiveTargetDataTask = nullptr;
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
}

// 从客户端 Location TargetData 提取水平瞄准方向，拒绝无效或 NaN 输入。
bool UArenaGameplayAbility_BasicAttack::ExtractAimDirection(
	const FGameplayAbilityTargetDataHandle& TargetData,
	AActor* AvatarActor,
	FVector& OutAimDirection) const
{
	if (!AvatarActor)
	{
		return false;
	}

	const FGameplayAbilityTargetData* FirstTargetData = TargetData.Get(0);
	if (!FirstTargetData || !FirstTargetData->HasEndPoint())
	{
		return false;
	}

	const FVector TargetLocation = FirstTargetData->GetEndPoint();
	if (TargetLocation.ContainsNaN())
	{
		return false;
	}

	OutAimDirection = TargetLocation - AvatarActor->GetActorLocation();
	OutAimDirection.Z = 0.0f;
	OutAimDirection = OutAimDirection.GetSafeNormal();
	return !OutAimDirection.IsNearlyZero();
}

// 本地预测端即时播放，服务器端通过 ASC Montage 状态复制给其他客户端；Ability 结束后动画继续播完。
void UArenaGameplayAbility_BasicAttack::PlayAttackMontage()
{
	if (!AttackMontage)
	{
		return;
	}

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		FName(TEXT("PlayerBasicAttackMontage")),
		AttackMontage,
		FMath::Max(MontagePlayRate, 0.01f),
		MontageStartSection,
		true,
		0.0f);
	if (MontageTask)
	{
		ActiveMontageTask = MontageTask;
		MontageTask->OnCompleted.AddDynamic(this, &UArenaGameplayAbility_BasicAttack::HandleAttackMontageCompleted);
		MontageTask->OnBlendOut.AddDynamic(this, &UArenaGameplayAbility_BasicAttack::HandleAttackMontageCompleted);
		MontageTask->OnInterrupted.AddDynamic(this, &UArenaGameplayAbility_BasicAttack::HandleAttackMontageInterrupted);
		MontageTask->OnCancelled.AddDynamic(this, &UArenaGameplayAbility_BasicAttack::HandleAttackMontageInterrupted);
		MontageTask->ReadyForActivation();
	}
}

// 在客户端预测和服务器确认使用同一 PredictionKey 执行起手 Cue，拥有者立即听到且服务器确认不会重复播放。
void UArenaGameplayAbility_BasicAttack::ExecuteAttackActivationCue(
	AActor* AvatarActor,
	UAbilitySystemComponent* SourceASC) const
{
	if (!AvatarActor || !SourceASC)
	{
		return;
	}

	FGameplayCueParameters ActivationCueParameters;
	ActivationCueParameters.Instigator = AvatarActor;
	ActivationCueParameters.EffectCauser = AvatarActor;
	ActivationCueParameters.Location = AvatarActor->GetActorLocation();
	SourceASC->ExecuteGameplayCue(
		ArenaGameplayTags::GameplayCue_Ability_BasicAttack_Activate,
		ActivationCueParameters);
}

// 仅在服务器沿最终瞄准方向扫描目标，并把选中命中点写入 GE 上下文供伤害和 Cue 共用。
void UArenaGameplayAbility_BasicAttack::ExecuteServerAttack(
	AActor* AvatarActor,
	UAbilitySystemComponent* SourceASC,
	const FVector& AimDirection)
{
	if (!AvatarActor || !SourceASC || !DamageEffectClass)
	{
		return;
	}

	UWorld* World = AvatarActor->GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Start = AvatarActor->GetActorLocation();
	const FVector End = Start + AimDirection * AttackRange;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ArenaBasicAttack), false, AvatarActor);
	QueryParams.AddIgnoredActor(AvatarActor);

	TArray<FHitResult> HitResults;
	World->SweepMultiByChannel(
		HitResults,
		Start,
		End,
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(AttackRadius),
		QueryParams);

	AActor* BestTarget = nullptr;
	UAbilitySystemComponent* BestTargetASC = nullptr;
	FHitResult BestTargetHitResult;
	float BestDistanceSquared = TNumericLimits<float>::Max();

	for (const FHitResult& HitResult : HitResults)
	{
		AActor* HitActor = HitResult.GetActor();
		if (!HitActor || HitActor == AvatarActor)
		{
			continue;
		}

		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
		if (!TargetASC)
		{
			continue;
		}

		// 避免命中自身 ASC，玩家 ASC 位于 PlayerState 时仍需要这个保护。
		if (TargetASC == SourceASC)
		{
			continue;
		}

		// 死亡目标不再参与命中选择，防止重复受击和死亡反馈。
		if (TargetASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(Start, HitActor->GetActorLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestTarget = HitActor;
			BestTargetASC = TargetASC;
			BestTargetHitResult = HitResult;
		}
	}

	DrawAttackRangeDebug(World, Start, End, BestTarget != nullptr);

	if (BestTarget && BestTargetASC)
	{
		// 伤害数值以 SetByCaller 写入 GE Spec，实际计算由 ExecCalc_Damage 完成。
		FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
		EffectContext.AddSourceObject(this);
		// 保留权威 Sweep 的真实命中点，供伤害 GameplayCue 在目标表面准确生成。
		EffectContext.AddHitResult(BestTargetHitResult, true);
		FGameplayEffectSpecHandle DamageSpecHandle = SourceASC->MakeOutgoingSpec(DamageEffectClass, GetAbilityLevel(), EffectContext);

		if (DamageSpecHandle.IsValid())
		{
			FGameplayEffectSpec* DamageSpec = DamageSpecHandle.Data.Get();
			DamageSpec->SetSetByCallerMagnitude(ArenaGameplayTags::SetByCaller_Damage_Base, BaseDamage);
			DamageSpec->SetSetByCallerMagnitude(ArenaGameplayTags::SetByCaller_Damage_SkillMultiplier, SkillMultiplier);

			if (DamageTypeTag.IsValid())
			{
				DamageSpec->AddDynamicAssetTag(DamageTypeTag);
			}

			BestTargetASC->ApplyGameplayEffectSpecToSelf(*DamageSpec);
			if (ArenaAbilityNetworkDebug::IsAuditEnabled())
			{
				UE_LOG(LogArenaAbilityNet, Log, TEXT("[%llu] BasicAttack Key=%d Handle=%s Target=%s"),
					ArenaAbilityNetworkDebug::NextServerExecutionSequence(),
					GetCurrentActivationInfo().GetActivationPredictionKey().Current,
					*GetCurrentAbilitySpecHandle().ToString(),
					*GetNameSafe(BestTarget));
			}
		}
	}

}

// 绘制基础攻击调试范围，帮助确认扫描方向和是否命中目标。
void UArenaGameplayAbility_BasicAttack::DrawAttackRangeDebug(UWorld* World, const FVector& Start, const FVector& End, bool bHitTarget) const
{
	if (!bDrawDebugAttackRange || !World)
	{
		return;
	}

	const FVector AttackVector = End - Start;
	const float AttackLength = AttackVector.Size();
	if (AttackLength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FColor RangeColor = (bHitTarget ? DebugAttackRangeHitColor : DebugAttackRangeMissColor).ToFColor(true);
	const FVector AttackDirection = AttackVector / AttackLength;
	const FVector CapsuleCenter = (Start + End) * 0.5f;
	const float CapsuleHalfHeight = AttackLength * 0.5f + AttackRadius;
	const FQuat CapsuleRotation = FRotationMatrix::MakeFromZ(AttackDirection).ToQuat();

	// Debug 胶囊和端点球用于确认 Ability 已触发但可能没有命中。
	DrawDebugCapsule(
		World,
		CapsuleCenter,
		CapsuleHalfHeight,
		AttackRadius,
		CapsuleRotation,
		RangeColor,
		false,
		DebugAttackRangeDuration,
		0,
		2.0f);

	DrawDebugLine(
		World,
		Start,
		End,
		FColor::White,
		false,
		DebugAttackRangeDuration,
		0,
		1.0f);

	DrawDebugSphere(
		World,
		End,
		AttackRadius,
		16,
		RangeColor,
		false,
		DebugAttackRangeDuration,
		0,
		1.5f);
}

void UArenaGameplayAbility_BasicAttack::HandleAttackMontageCompleted()
{
	if (IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}

void UArenaGameplayAbility_BasicAttack::HandleAttackMontageInterrupted()
{
	if (IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
	}
}

void UArenaGameplayAbility_BasicAttack::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	ActiveTargetDataTask = nullptr;
	ActiveMontageTask = nullptr;
	bConsumedTargetData = false;
	bServerAttackExecuted = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
