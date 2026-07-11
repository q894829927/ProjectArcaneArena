#include "GAS/ArenaGameplayAbility_EnemyMeleeAttack.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Character/ArenaEnemyCharacter.h"
#include "GameFramework/Controller.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffect.h"

// 构造服务器权威的敌人近战技能，冷却由蓝图 GameplayEffect 配置。
UArenaGameplayAbility_EnemyMeleeAttack::UArenaGameplayAbility_EnemyMeleeAttack()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	SetAssetTags(FGameplayTagContainer(ArenaGameplayTags::Ability_Enemy_MeleeAttack));
	ActivationBlockedTags.AddTag(ArenaGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(ArenaGameplayTags::State_Stunned);
	ActivationBlockedTags.AddTag(ArenaGameplayTags::State_Attacking);
	ActivationBlockedTags.AddTag(ArenaGameplayTags::Cooldown_Enemy_MeleeAttack);
}

// 校验目标并提交攻击，Montage 负责复制表现，服务器 Delay 负责稳定命中时机。
void UArenaGameplayAbility_EnemyMeleeAttack::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	AArenaEnemyCharacter* SourceEnemy = ActorInfo ? Cast<AArenaEnemyCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	AActor* TargetActor = SourceEnemy ? SourceEnemy->GetCombatTarget() : nullptr;
	UAbilitySystemComponent* TargetASC = nullptr;
	if (!SourceEnemy || !SourceASC || !AttackMontage || !DamageEffectClass
		|| !IsAttackTargetValid(SourceEnemy, TargetActor, TargetASC))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveSourceEnemy = SourceEnemy;
	ActiveTargetActor = TargetActor;
	ActiveSourceASC = SourceASC;
	bProcessedHit = false;
	ApplyAttackStateTag();

	UAbilityTask_WaitDelay* HitDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(HitDelay, 0.0f));
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		FName(TEXT("EnemyMeleeAttackMontage")),
		AttackMontage,
		FMath::Max(MontagePlayRate, 0.01f),
		MontageStartSection,
		true,
		0.0f);
	if (!HitDelayTask || !MontageTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	HitDelayTask->OnFinish.AddDynamic(this, &UArenaGameplayAbility_EnemyMeleeAttack::HandleHitDelayFinished);
	MontageTask->OnCompleted.AddDynamic(this, &UArenaGameplayAbility_EnemyMeleeAttack::HandleMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UArenaGameplayAbility_EnemyMeleeAttack::HandleMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UArenaGameplayAbility_EnemyMeleeAttack::HandleMontageInterrupted);

	// 先启动 Delay，再启动 Montage；Montage 启动失败时取消 Ability 会自动销毁 Delay，避免迟到伤害。
	HitDelayTask->ReadyForActivation();
	MontageTask->ReadyForActivation();
}

// 无论正常完成还是死亡/眩晕取消，都必须移除 loose/replicated loose 攻击标签。
void UArenaGameplayAbility_EnemyMeleeAttack::EndAbility(
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
	bProcessedHit = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// 激活和命中时共用同一套服务器校验，前摇期间离开范围或失去视线会打空。
bool UArenaGameplayAbility_EnemyMeleeAttack::IsAttackTargetValid(
	AArenaEnemyCharacter* SourceEnemy,
	AActor* TargetActor,
	UAbilitySystemComponent*& OutTargetASC) const
{
	OutTargetASC = TargetActor ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor) : nullptr;
	const UAbilitySystemComponent* SourceASC = SourceEnemy ? SourceEnemy->GetAbilitySystemComponent() : nullptr;
	const AController* SourceController = SourceEnemy ? SourceEnemy->GetController() : nullptr;
	return SourceEnemy
		&& SourceEnemy->HasAuthority()
		&& SourceASC
		&& IsValid(TargetActor)
		&& OutTargetASC
		&& OutTargetASC != SourceASC
		&& !OutTargetASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead)
		&& FVector::Dist2D(SourceEnemy->GetActorLocation(), TargetActor->GetActorLocation()) <= AttackRange + RangeTolerance
		&& SourceController
		&& SourceController->LineOfSightTo(TargetActor);
}

// 命中窗口只处理一次；重新构造 Damage Spec 以捕获命中时的最新攻击与防御属性。
void UArenaGameplayAbility_EnemyMeleeAttack::HandleHitDelayFinished()
{
	if (bProcessedHit)
	{
		return;
	}
	bProcessedHit = true;

	AArenaEnemyCharacter* SourceEnemy = ActiveSourceEnemy.Get();
	AActor* TargetActor = ActiveTargetActor.Get();
	UAbilitySystemComponent* SourceASC = ActiveSourceASC.Get();
	UAbilitySystemComponent* TargetASC = nullptr;
	if (!SourceASC || !IsAttackTargetValid(SourceEnemy, TargetActor, TargetASC))
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);
	EffectContext.AddInstigator(SourceEnemy, SourceEnemy);
	FGameplayEffectSpecHandle DamageSpecHandle = SourceASC->MakeOutgoingSpec(
		DamageEffectClass,
		GetAbilityLevel(),
		EffectContext);
	if (!DamageSpecHandle.IsValid())
	{
		return;
	}

	FGameplayEffectSpec* DamageSpec = DamageSpecHandle.Data.Get();
	DamageSpec->SetSetByCallerMagnitude(ArenaGameplayTags::SetByCaller_Damage_Base, BaseDamage);
	DamageSpec->SetSetByCallerMagnitude(ArenaGameplayTags::SetByCaller_Damage_SkillMultiplier, SkillMultiplier);
	DamageSpec->AddDynamicAssetTag(ArenaGameplayTags::Damage_Physical);
	SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpec, TargetASC);
}

void UArenaGameplayAbility_EnemyMeleeAttack::HandleMontageCompleted()
{
	FinishCurrentAttack(false);
}

void UArenaGameplayAbility_EnemyMeleeAttack::HandleMontageInterrupted()
{
	FinishCurrentAttack(true);
}

void UArenaGameplayAbility_EnemyMeleeAttack::FinishCurrentAttack(bool bWasCancelled)
{
	if (IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, bWasCancelled);
	}
}

void UArenaGameplayAbility_EnemyMeleeAttack::ApplyAttackStateTag()
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

void UArenaGameplayAbility_EnemyMeleeAttack::RemoveAttackStateTag()
{
	UAbilitySystemComponent* SourceASC = ActiveSourceASC.Get();
	if (SourceASC && bAppliedAttackStateTag)
	{
		SourceASC->RemoveLooseGameplayTag(ArenaGameplayTags::State_Attacking);
		SourceASC->RemoveReplicatedLooseGameplayTag(ArenaGameplayTags::State_Attacking);
	}
	bAppliedAttackStateTag = false;
}
