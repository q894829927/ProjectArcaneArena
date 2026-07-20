#include "GAS/ArenaGameplayAbility_EnemyAttackBase.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/ArenaEnemyCharacter.h"
#include "GameFramework/Controller.h"
#include "GAS/ArenaGameplayTags.h"

// 初始化服务器权威的敌人攻击基类，并统一阻断死亡、眩晕和并行攻击。
UArenaGameplayAbility_EnemyAttackBase::UArenaGameplayAbility_EnemyAttackBase()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	ActivationBlockedTags.AddTag(ArenaGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(ArenaGameplayTags::State_Stunned);
	ActivationBlockedTags.AddTag(ArenaGameplayTags::State_Attacking);
}

// 将派生攻击的权威视线或弹道检查公开给 AI 决策，确保停步条件与 Ability 激活一致。
bool UArenaGameplayAbility_EnemyAttackBase::HasAttackPathForAI(
	AArenaEnemyCharacter* SourceEnemy,
	AActor* TargetActor) const
{
	return HasAttackLineOfSight(SourceEnemy, TargetActor);
}

// 在服务器一次性完成通用攻击前置流程，派生 Ability 可在成功后接管自己的预警、位移和命中生命周期。
bool UArenaGameplayAbility_EnemyAttackBase::BeginServerAttack(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	AArenaEnemyCharacter*& OutSourceEnemy,
	AActor*& OutTargetActor,
	UAbilitySystemComponent*& OutSourceASC)
{
	OutSourceEnemy = ActorInfo ? Cast<AArenaEnemyCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	OutSourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	OutTargetActor = OutSourceEnemy ? OutSourceEnemy->GetCombatTarget() : nullptr;
	UAbilitySystemComponent* TargetASC = nullptr;
	if (!OutSourceEnemy || !OutSourceASC || !HasRequiredAttackConfiguration()
		|| !IsAttackTargetValid(OutSourceEnemy, OutTargetActor, TargetASC, true)
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		return false;
	}

	ActiveSourceEnemy = OutSourceEnemy;
	ActiveTargetActor = OutTargetActor;
	ActiveSourceASC = OutSourceASC;
	bProcessedRelease = false;
	bMontageCompleted = false;
	ApplyAttackStateTag();
	return true;
}

// 校验并锁定 AI 当前目标，提交后启动可复制 Montage，并按配置使用固定时间或动作结束点释放。
void UArenaGameplayAbility_EnemyAttackBase::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	AArenaEnemyCharacter* SourceEnemy = nullptr;
	AActor* TargetActor = nullptr;
	UAbilitySystemComponent* SourceASC = nullptr;
	if (!BeginServerAttack(
		Handle,
		ActorInfo,
		ActivationInfo,
		SourceEnemy,
		TargetActor,
		SourceASC))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FGameplayTag ActivationCueTag = GetAttackActivationCueTag();
	if (ActivationCueTag.IsValid())
	{
		FGameplayCueParameters CueParameters;
		CueParameters.Instigator = SourceEnemy;
		CueParameters.EffectCauser = SourceEnemy;
		CueParameters.Location = SourceEnemy->GetActorLocation();
		SourceASC->ExecuteGameplayCue(ActivationCueTag, CueParameters);
	}

	const bool bReleaseOnMontageBlendOut = ShouldReleaseOnMontageBlendOut();
	const float ReleaseDelay = FMath::Max(GetAttackReleaseDelay(), 0.0f);
	UAbilityTask_WaitDelay* ReleaseDelayTask = !bReleaseOnMontageBlendOut && ReleaseDelay > 0.0f
		? UAbilityTask_WaitDelay::WaitDelay(this, ReleaseDelay)
		: nullptr;
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		FName(TEXT("EnemyPrimaryAttackMontage")),
		GetAttackMontage(),
		FMath::Max(GetAttackMontagePlayRate(), 0.01f),
		GetAttackMontageStartSection(),
		true,
		0.0f);
	if (!MontageTask || (!bReleaseOnMontageBlendOut && ReleaseDelay > 0.0f && !ReleaseDelayTask))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ReleaseDelayTask)
	{
		ReleaseDelayTask->OnFinish.AddDynamic(this, &UArenaGameplayAbility_EnemyAttackBase::HandleReleaseDelayFinished);
	}
	MontageTask->OnBlendOut.AddDynamic(this, &UArenaGameplayAbility_EnemyAttackBase::HandleMontageBlendOut);
	MontageTask->OnCompleted.AddDynamic(this, &UArenaGameplayAbility_EnemyAttackBase::HandleMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UArenaGameplayAbility_EnemyAttackBase::HandleMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UArenaGameplayAbility_EnemyAttackBase::HandleMontageInterrupted);

	if (bReleaseOnMontageBlendOut)
	{
		// 动作结束同步攻击只启动 Montage，发射由 OnBlendOut 精确触发。
		MontageTask->ReadyForActivation();
	}
	else if (ReleaseDelayTask)
	{
		// 延迟攻击先启动权威计时，Montage 失败时 EndAbility 会销毁尚未到期的任务。
		ReleaseDelayTask->ReadyForActivation();
		MontageTask->ReadyForActivation();
	}
	else
	{
		// 零延迟攻击先确认 Montage 成功启动，再在同一帧兑现 Projectile 或命中逻辑。
		MontageTask->ReadyForActivation();
		if (IsActive())
		{
			HandleReleaseDelayFinished();
		}
	}
}

// 清理由本次攻击持有的 Tag 与弱引用，AbilityTask 由 GAS 在结束时一并销毁。
void UArenaGameplayAbility_EnemyAttackBase::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	RemoveAttackStateTag();
	ActiveSourceEnemy.Reset();
	ActiveTargetActor.Reset();
	ActiveSourceASC.Reset();
	bProcessedRelease = false;
	bMontageCompleted = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// 基类至少要求有效 Montage；派生类继续检查伤害 GE 或 Projectile Class。
bool UArenaGameplayAbility_EnemyAttackBase::HasRequiredAttackConfiguration() const
{
	return GetAttackMontage() != nullptr;
}

// 默认使用 AIController 视线判定，保持既有近战攻击的墙体与目标遮挡行为。
bool UArenaGameplayAbility_EnemyAttackBase::HasAttackLineOfSight(
	AArenaEnemyCharacter* SourceEnemy,
	AActor* TargetActor) const
{
	const AController* SourceController = SourceEnemy ? SourceEnemy->GetController() : nullptr;
	return SourceController && TargetActor && SourceController->LineOfSightTo(TargetActor);
}

// 提供空的派生释放扩展点，实际敌人攻击必须覆写该函数。
void UArenaGameplayAbility_EnemyAttackBase::ExecuteAttack(
	AArenaEnemyCharacter* SourceEnemy,
	AActor* TargetActor,
	UAbilitySystemComponent* SourceASC,
	UAbilitySystemComponent* TargetASC)
{
}

// 校验权威来源与存活目标，并在需要时应用派生攻击的最小/最大距离、容差和视线规则。
bool UArenaGameplayAbility_EnemyAttackBase::IsAttackTargetValid(
	AArenaEnemyCharacter* SourceEnemy,
	AActor* TargetActor,
	UAbilitySystemComponent*& OutTargetASC,
	bool bCheckRangeAndLineOfSight) const
{
	OutTargetASC = TargetActor ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor) : nullptr;
	const UAbilitySystemComponent* SourceASC = SourceEnemy ? SourceEnemy->GetAbilitySystemComponent() : nullptr;
	return SourceEnemy
		&& SourceEnemy->HasAuthority()
		&& SourceASC
		&& IsValid(TargetActor)
		&& OutTargetASC
		&& OutTargetASC != SourceASC
		&& !OutTargetASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead)
		&& (!bCheckRangeAndLineOfSight
			|| ([this, SourceEnemy, TargetActor]()
			{
				const float Distance = FVector::Dist2D(
					SourceEnemy->GetActorLocation(),
					TargetActor->GetActorLocation());
				const float Tolerance = FMath::Max(GetAttackRangeTolerance(), 0.0f);
				const float MinimumRange = FMath::Max(GetMinimumAttackRange() - Tolerance, 0.0f);
				const float MaximumRange = FMath::Max(GetAttackRange(), 0.0f) + Tolerance;
				return Distance >= MinimumRange
					&& Distance <= MaximumRange
					&& HasAttackLineOfSight(SourceEnemy, TargetActor);
			}()));
}

// 到达权威释放时机后只处理一次；远程可跳过距离复验以兑现已经完成的施法。
void UArenaGameplayAbility_EnemyAttackBase::HandleReleaseDelayFinished()
{
	if (bProcessedRelease)
	{
		return;
	}
	bProcessedRelease = true;

	AArenaEnemyCharacter* SourceEnemy = ActiveSourceEnemy.Get();
	AActor* TargetActor = ActiveTargetActor.Get();
	UAbilitySystemComponent* SourceASC = ActiveSourceASC.Get();
	UAbilitySystemComponent* TargetASC = nullptr;
	if (SourceASC && IsAttackTargetValid(
		SourceEnemy,
		TargetActor,
		TargetASC,
		ShouldRevalidateRangeAndLineOfSightAtRelease()))
	{
		ExecuteAttack(SourceEnemy, TargetActor, SourceASC, TargetASC);
	}

	if (bMontageCompleted)
	{
		FinishCurrentAttack(false);
	}
}

// 正常 Montage 完成时提供 BlendOut 遗漏保护，并在释放已经处理后结束 Ability。
void UArenaGameplayAbility_EnemyAttackBase::HandleMontageCompleted()
{
	if (ShouldReleaseOnMontageBlendOut() && !bProcessedRelease)
	{
		HandleReleaseDelayFinished();
	}

	bMontageCompleted = true;
	if (bProcessedRelease)
	{
		FinishCurrentAttack(false);
	}
}

// 在 Montage 动作结束并开始 BlendOut 的同一帧释放，避免把姿势混合时间误当成施法后摇。
void UArenaGameplayAbility_EnemyAttackBase::HandleMontageBlendOut()
{
	if (ShouldReleaseOnMontageBlendOut() && IsActive())
	{
		HandleReleaseDelayFinished();
	}
}

// 动画打断表示攻击取消，立即结束并让 AbilityTask 阻止尚未发生的释放。
void UArenaGameplayAbility_EnemyAttackBase::HandleMontageInterrupted()
{
	FinishCurrentAttack(true);
}

// 只结束当前仍活跃的实例，避免 Montage 与 Delay 在同帧重复结束 Ability。
void UArenaGameplayAbility_EnemyAttackBase::FinishCurrentAttack(bool bWasCancelled)
{
	if (IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, bWasCancelled);
	}
}

// 添加本地和复制 loose Tag，使服务器 AI 停止寻路且客户端可观察攻击状态。
void UArenaGameplayAbility_EnemyAttackBase::ApplyAttackStateTag()
{
	UAbilitySystemComponent* SourceASC = ActiveSourceASC.Get();
	if (!SourceASC || bAppliedAttackStateTag)
	{
		return;
	}

	SourceASC->AddLooseGameplayTag(ArenaGameplayTags::State_Attacking);
	SourceASC->AddReplicatedLooseGameplayTag(ArenaGameplayTags::State_Attacking);
	bAppliedAttackStateTag = true;
}

// 对称移除本次攻击添加的两类 loose Tag，死亡、眩晕和正常结束都走此路径。
void UArenaGameplayAbility_EnemyAttackBase::RemoveAttackStateTag()
{
	UAbilitySystemComponent* SourceASC = ActiveSourceASC.Get();
	if (SourceASC && bAppliedAttackStateTag)
	{
		SourceASC->RemoveLooseGameplayTag(ArenaGameplayTags::State_Attacking);
		SourceASC->RemoveReplicatedLooseGameplayTag(ArenaGameplayTags::State_Attacking);
	}
	bAppliedAttackStateTag = false;
}
