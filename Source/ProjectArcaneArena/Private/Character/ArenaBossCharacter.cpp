#include "Character/ArenaBossCharacter.h"

#include "AI/ArenaBossAIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/ArenaLogCategories.h"
#include "GAS/ArenaAbilitySystemComponent.h"
#include "GAS/ArenaAttributeSet.h"
#include "GAS/ArenaGameplayEffect_BossEnrage.h"
#include "GAS/ArenaGameplayEffect_BossPlayerCountScaling.h"
#include "GAS/ArenaGameplayEffect_HealthRestore.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"

// Boss 沿用敌人 ASC 与死亡链路，并提供 Enrage、人数缩放和 Outro 尸体时长的原生默认。
AArenaBossCharacter::AArenaBossCharacter()
	: BossDisplayName(NSLOCTEXT("ArenaBossCharacter", "DefaultBossName", "悟空战将"))
{
	AIControllerClass = AArenaBossAIController::StaticClass();
	bShowWorldHealthBar = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
	EnrageEffectClass = UArenaGameplayEffect_BossEnrage::StaticClass();
	PlayerCountScalingEffectClass = UArenaGameplayEffect_BossPlayerCountScaling::StaticClass();
	DeathLifeSpan = 5.5f;
}

// 从复制的 ASC 阶段标签解析当前阶段，客户端和 HUD 不依赖服务器私有变量。
FGameplayTag AArenaBossCharacter::GetCurrentBossPhaseTag() const
{
	const UArenaAbilitySystemComponent* BossASC = GetArenaAbilitySystemComponent();
	if (!BossASC)
	{
		return FGameplayTag();
	}

	if (BossASC->HasMatchingGameplayTag(ArenaGameplayTags::Boss_Phase_Three))
	{
		return ArenaGameplayTags::Boss_Phase_Three;
	}
	if (BossASC->HasMatchingGameplayTag(ArenaGameplayTags::Boss_Phase_Two))
	{
		return ArenaGameplayTags::Boss_Phase_Two;
	}
	if (BossASC->HasMatchingGameplayTag(ArenaGameplayTags::Boss_Phase_One))
	{
		return ArenaGameplayTags::Boss_Phase_One;
	}
	return FGameplayTag();
}

// 服务器登记召唤物并添加明确的构筑事件资格；未进入集合的 Actor 不会被 Boss 生命周期接管。
bool AArenaBossCharacter::RegisterBossSummon(AArenaEnemyCharacter* SummonedEnemy)
{
	if (!HasAuthority() || !IsValid(SummonedEnemy) || SummonedEnemy == this)
	{
		return false;
	}

	const TWeakObjectPtr<AArenaEnemyCharacter> SummonKey(SummonedEnemy);
	if (ActiveBossSummons.Contains(SummonKey))
	{
		return true;
	}
	if (!CanAcceptBossSummons(1))
	{
		UE_LOG(
			LogArenaBoss,
			Warning,
			TEXT("Boss %s rejected summon %s because the active limit %d was reached."),
			*GetNameSafe(this),
			*GetNameSafe(SummonedEnemy),
			FMath::Max(MaxActiveSummons, 1));
		return false;
	}

	UArenaAbilitySystemComponent* SummonedASC = SummonedEnemy->GetArenaAbilitySystemComponent();
	if (!SummonedASC || SummonedASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead))
	{
		UE_LOG(
			LogArenaBoss,
			Warning,
			TEXT("Boss %s rejected summon %s because it has no living ASC."),
			*GetNameSafe(this),
			*GetNameSafe(SummonedEnemy));
		return false;
	}

	ActiveBossSummons.Add(SummonKey);
	SummonedEnemy->OnEnemyDeath.AddUniqueDynamic(this, &AArenaBossCharacter::HandleBossSummonDeath);
	SummonedEnemy->OnDestroyed.AddUniqueDynamic(this, &AArenaBossCharacter::HandleBossSummonDestroyed);
	AddBossSummonGameplayTags(SummonedEnemy);
	return true;
}

// 召唤物离开活动集合时先解绑回调再移除身份标签，重复调用不会修改其他 Boss 的状态。
void AArenaBossCharacter::UnregisterBossSummon(AArenaEnemyCharacter* SummonedEnemy)
{
	if (!HasAuthority() || !SummonedEnemy)
	{
		return;
	}

	const TWeakObjectPtr<AArenaEnemyCharacter> SummonKey(SummonedEnemy);
	if (ActiveBossSummons.Remove(SummonKey) == 0)
	{
		return;
	}

	SummonedEnemy->OnEnemyDeath.RemoveDynamic(this, &AArenaBossCharacter::HandleBossSummonDeath);
	SummonedEnemy->OnDestroyed.RemoveDynamic(this, &AArenaBossCharacter::HandleBossSummonDestroyed);
	RemoveBossSummonGameplayTags(SummonedEnemy);
}

// Boss 生命周期结束时先清空集合和委托，再销毁召唤 Actor，避免销毁回调重入当前迭代。
void AArenaBossCharacter::DestroyAllBossSummons()
{
	if (!HasAuthority() || ActiveBossSummons.IsEmpty())
	{
		return;
	}

	TArray<TWeakObjectPtr<AArenaEnemyCharacter>> SummonsToDestroy;
	SummonsToDestroy.Reserve(ActiveBossSummons.Num());
	for (const TWeakObjectPtr<AArenaEnemyCharacter>& Summon : ActiveBossSummons)
	{
		SummonsToDestroy.Add(Summon);
	}
	ActiveBossSummons.Reset();

	for (const TWeakObjectPtr<AArenaEnemyCharacter>& Summon : SummonsToDestroy)
	{
		AArenaEnemyCharacter* SummonedEnemy = Summon.Get();
		if (!IsValid(SummonedEnemy))
		{
			continue;
		}

		SummonedEnemy->OnEnemyDeath.RemoveDynamic(this, &AArenaBossCharacter::HandleBossSummonDeath);
		SummonedEnemy->OnDestroyed.RemoveDynamic(this, &AArenaBossCharacter::HandleBossSummonDestroyed);
		RemoveBossSummonGameplayTags(SummonedEnemy);
		SummonedEnemy->Destroy();
	}
}

// 只统计仍有效且 ASC 未死亡的召唤物，死亡广播会在同一服务器帧进一步清理集合。
int32 AArenaBossCharacter::GetActiveSummonCount() const
{
	int32 ActiveCount = 0;
	for (const TWeakObjectPtr<AArenaEnemyCharacter>& Summon : ActiveBossSummons)
	{
		const AArenaEnemyCharacter* SummonedEnemy = Summon.Get();
		const UArenaAbilitySystemComponent* SummonedASC = SummonedEnemy
			? SummonedEnemy->GetArenaAbilitySystemComponent()
			: nullptr;
		if (IsValid(SummonedEnemy)
			&& SummonedASC
			&& !SummonedASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead))
		{
			++ActiveCount;
		}
	}
	return ActiveCount;
}

// 上限始终至少按一个处理，避免错误蓝图值让召唤能力永久失去可恢复状态。
int32 AArenaBossCharacter::GetRemainingSummonCapacity() const
{
	return FMath::Max(FMath::Max(MaxActiveSummons, 1) - GetActiveSummonCount(), 0);
}

// 部分生成是合法结果，因此只要请求为正且仍有一个空位就允许进入召唤流程。
bool AArenaBossCharacter::CanAcceptBossSummons(int32 RequestedCount) const
{
	return RequestedCount > 0 && GetRemainingSummonCapacity() > 0;
}

// 服务器冻结生成时人数并经由两个 Instant GE 依次扩大 MaxHealth、补满 Health，任何重复调用都不会叠加。
bool AArenaBossCharacter::InitializePlayerCountScaling(int32 ParticipatingPlayerCount)
{
	if (!HasAuthority())
	{
		UE_LOG(LogArenaBoss, Warning, TEXT("Boss %s rejected player-count scaling on a non-authority instance."), *GetNameSafe(this));
		return false;
	}
	if (bHasAttemptedPlayerCountScaling)
	{
		UE_LOG(
			LogArenaBoss,
			Warning,
			TEXT("Boss %s ignored duplicate player-count scaling; snapshot=%d multiplier=%.2f."),
			*GetNameSafe(this),
			ScalingPlayerCountSnapshot,
			AppliedHealthMultiplier);
		return false;
	}

	UArenaAbilitySystemComponent* BossASC = GetArenaAbilitySystemComponent();
	UArenaAttributeSet* BossAttributes = GetArenaAttributeSet();
	if (!BossASC || !BossAttributes || BossASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead))
	{
		UE_LOG(LogArenaBoss, Error, TEXT("Boss %s cannot initialize player-count scaling without a living ASC and AttributeSet."), *GetNameSafe(this));
		return false;
	}

	if (ParticipatingPlayerCount <= 0)
	{
		UE_LOG(LogArenaBoss, Warning, TEXT("Boss %s found no valid ArenaPlayerState; falling back to single-player scaling."), *GetNameSafe(this));
	}
	else if (ParticipatingPlayerCount > 2)
	{
		UE_LOG(
			LogArenaBoss,
			Warning,
			TEXT("Boss %s found %d players; the current two-player demo caps scaling at the two-player multiplier."),
			*GetNameSafe(this),
			ParticipatingPlayerCount);
	}

	ScalingPlayerCountSnapshot = FMath::Clamp(ParticipatingPlayerCount, 1, 2);
	AppliedHealthMultiplier = ScalingPlayerCountSnapshot >= 2
		? FMath::Max(TwoPlayerHealthMultiplier, 0.01f)
		: FMath::Max(SinglePlayerHealthMultiplier, 0.01f);
	bHasAttemptedPlayerCountScaling = true;

	const float InitialMaxHealth = BossAttributes->GetMaxHealth();
	if (InitialMaxHealth <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogArenaBoss, Error, TEXT("Boss %s has invalid initial MaxHealth %.2f; player-count scaling was not applied."),
			*GetNameSafe(this),
			InitialMaxHealth);
		return false;
	}

	const float TargetMaxHealth = InitialMaxHealth * AppliedHealthMultiplier;
	const float MaxHealthDelta = TargetMaxHealth - InitialMaxHealth;
	bool bMaxHealthApplied = true;
	bool bHealthRestored = true;
	{
		TGuardValue<bool> SuppressPhaseEvaluation(bSuppressBossPhaseEvaluation, true);
		if (!FMath::IsNearlyZero(MaxHealthDelta))
		{
			bMaxHealthApplied = ApplyPlayerCountMaxHealthDelta(
				MaxHealthDelta,
				PlayerCountScalingEffectClass);
		}
		if (bMaxHealthApplied)
		{
			bHealthRestored = RestoreHealthAfterPlayerCountScaling();
		}
	}

	const float FinalMaxHealth = BossAttributes->GetMaxHealth();
	const float FinalHealth = BossAttributes->GetHealth();
	const bool bReachedTargetMaxHealth = FMath::IsNearlyEqual(FinalMaxHealth, TargetMaxHealth, 0.1f);
	const bool bReachedFullHealth = FMath::IsNearlyEqual(FinalHealth, FinalMaxHealth, 0.1f);
	const bool bSucceeded = bMaxHealthApplied && bHealthRestored && bReachedTargetMaxHealth && bReachedFullHealth;
	if (!bSucceeded)
	{
		bool bRolledBackToBaseline = true;
		{
			TGuardValue<bool> SuppressPhaseEvaluation(bSuppressBossPhaseEvaluation, true);
			const float RollbackDelta = InitialMaxHealth - BossAttributes->GetMaxHealth();
			if (!FMath::IsNearlyZero(RollbackDelta))
			{
				bRolledBackToBaseline = ApplyPlayerCountMaxHealthDelta(
					RollbackDelta,
					UArenaGameplayEffect_BossPlayerCountScaling::StaticClass());
			}
			if (bRolledBackToBaseline)
			{
				bRolledBackToBaseline = RestoreHealthAfterPlayerCountScaling();
			}
		}
		AppliedHealthMultiplier = 1.0f;

		UE_LOG(
			LogArenaBoss,
			Error,
			TEXT("Boss %s player-count scaling failed. Players=%d Target=%.2f Health=%.2f MaxHealth=%.2f Rollback=%s."),
			*GetNameSafe(this),
			ScalingPlayerCountSnapshot,
			TargetMaxHealth,
			BossAttributes->GetHealth(),
			BossAttributes->GetMaxHealth(),
			bRolledBackToBaseline ? TEXT("Succeeded") : TEXT("Failed"));
		return false;
	}

	UE_LOG(
		LogArenaBoss,
		Log,
		TEXT("Boss %s initialized for %d player(s): MaxHealth %.2f -> %.2f (%.2fx), Health restored to %.2f."),
		*GetNameSafe(this),
		ScalingPlayerCountSnapshot,
		InitialMaxHealth,
		FinalMaxHealth,
		AppliedHealthMultiplier,
		FinalHealth);
	return true;
}

// 构造携带 MaxHealth 增量的 Instant Spec；Instant Handle 用 WasSuccessfullyApplied 判断而不是 IsValid。
bool AArenaBossCharacter::ApplyPlayerCountMaxHealthDelta(
	float MaxHealthDelta,
	TSubclassOf<UGameplayEffect> ScalingEffectClass)
{
	UArenaAbilitySystemComponent* BossASC = GetArenaAbilitySystemComponent();
	if (!BossASC || !ScalingEffectClass)
	{
		UE_LOG(LogArenaBoss, Error, TEXT("Boss %s has no ASC or scaling GameplayEffect class."), *GetNameSafe(this));
		return false;
	}

	FGameplayEffectContextHandle EffectContext = BossASC->MakeEffectContext();
	EffectContext.AddInstigator(this, this);
	EffectContext.AddSourceObject(this);
	FGameplayEffectSpecHandle SpecHandle = BossASC->MakeOutgoingSpec(
		ScalingEffectClass,
		1.0f,
		EffectContext);
	if (!SpecHandle.IsValid())
	{
		UE_LOG(
			LogArenaBoss,
			Error,
			TEXT("Boss %s failed to create player-count scaling effect %s."),
			*GetNameSafe(this),
			*GetNameSafe(ScalingEffectClass.Get()));
		return false;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(ArenaGameplayTags::SetByCaller_Boss_MaxHealthDelta, MaxHealthDelta);
	const FActiveGameplayEffectHandle AppliedHandle = BossASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	return AppliedHandle.WasSuccessfullyApplied();
}

// 复用通用 HealthRestore GE 写入缺失生命值，确保恢复仍经过 AttributeSet 的 Healing Meta 路径。
bool AArenaBossCharacter::RestoreHealthAfterPlayerCountScaling()
{
	UArenaAbilitySystemComponent* BossASC = GetArenaAbilitySystemComponent();
	const UArenaAttributeSet* BossAttributes = GetArenaAttributeSet();
	if (!BossASC || !BossAttributes)
	{
		return false;
	}

	const float MissingHealth = FMath::Max(BossAttributes->GetMaxHealth() - BossAttributes->GetHealth(), 0.0f);
	if (MissingHealth <= KINDA_SMALL_NUMBER)
	{
		return true;
	}

	FGameplayEffectContextHandle EffectContext = BossASC->MakeEffectContext();
	EffectContext.AddInstigator(this, this);
	EffectContext.AddSourceObject(this);
	FGameplayEffectSpecHandle SpecHandle = BossASC->MakeOutgoingSpec(
		UArenaGameplayEffect_HealthRestore::StaticClass(),
		1.0f,
		EffectContext);
	if (!SpecHandle.IsValid())
	{
		UE_LOG(LogArenaBoss, Error, TEXT("Boss %s failed to create the Health restore effect after scaling."), *GetNameSafe(this));
		return false;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(ArenaGameplayTags::SetByCaller_Recovery_Health, MissingHealth);
	const FActiveGameplayEffectHandle AppliedHandle = BossASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	return AppliedHandle.WasSuccessfullyApplied();
}

// 等待敌人基类完成 ASC、属性和 StartupAbilities 初始化后，仅在已处于 Combat 时建立阶段状态机。
void AArenaBossCharacter::BeginPlay()
{
	Super::BeginPlay();
	OnEnemyDeath.AddUniqueDynamic(this, &AArenaBossCharacter::HandleBossPhaseDeath);
	if (!HasAuthority())
	{
		return;
	}

	BindBossPhaseDelegates();
	const AArenaGameState* ArenaGameState = BoundBossGameState.Get();
	if (ArenaGameState && ArenaGameState->GetGamePhase() == EArenaGamePhase::Combat)
	{
		InitializeBossPhaseState();
	}
}

// 销毁路径先清理召唤物与 GAS 阶段状态，再解除委托，避免子 Actor、Cue 或回调遗留到下一张地图。
void AArenaBossCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	OnEnemyDeath.RemoveDynamic(this, &AArenaBossCharacter::HandleBossPhaseDeath);
	if (HasAuthority())
	{
		DestroyAllBossSummons();
		CleanupBossPhaseState();
		UnbindBossPhaseDelegates();
	}

	Super::EndPlay(EndPlayReason);
}

// 注册 Boss 专属 Health、死亡和 GameState 阶段观察，避免复用敌人私有死亡实现。
void AArenaBossCharacter::BindBossPhaseDelegates()
{
	UnbindBossPhaseDelegates();

	if (UArenaAbilitySystemComponent* BossASC = GetArenaAbilitySystemComponent())
	{
		BossPhaseHealthChangedDelegateHandle = BossASC->GetGameplayAttributeValueChangeDelegate(
			UArenaAttributeSet::GetHealthAttribute()).AddUObject(
				this,
				&AArenaBossCharacter::HandleBossPhaseHealthChanged);
	}

	if (AArenaGameState* ArenaGameState = GetWorld() ? GetWorld()->GetGameState<AArenaGameState>() : nullptr)
	{
		BoundBossGameState = ArenaGameState;
		ArenaGameState->OnGamePhaseChanged.AddUniqueDynamic(
			this,
			&AArenaBossCharacter::HandleBossGamePhaseChanged);
	}
}

// 使用保存的 ASC 和 GameState 引用对称解除委托，防止 Actor 销毁后的迟到回调。
void AArenaBossCharacter::UnbindBossPhaseDelegates()
{
	if (UArenaAbilitySystemComponent* BossASC = GetArenaAbilitySystemComponent())
	{
		if (BossPhaseHealthChangedDelegateHandle.IsValid())
		{
			BossASC->GetGameplayAttributeValueChangeDelegate(
				UArenaAttributeSet::GetHealthAttribute()).Remove(BossPhaseHealthChangedDelegateHandle);
		}
	}
	BossPhaseHealthChangedDelegateHandle.Reset();

	if (AArenaGameState* ArenaGameState = BoundBossGameState.Get())
	{
		ArenaGameState->OnGamePhaseChanged.RemoveDynamic(
			this,
			&AArenaBossCharacter::HandleBossGamePhaseChanged);
	}
	BoundBossGameState.Reset();
}

// 初始化只添加 Phase 1，不播放转换 Cue；重复调用不会叠加标签计数。
void AArenaBossCharacter::InitializeBossPhaseState()
{
	UArenaAbilitySystemComponent* BossASC = GetArenaAbilitySystemComponent();
	if (!BossASC
		|| BossASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead)
		|| GetCurrentBossPhaseTag().IsValid())
	{
		return;
	}

	BossASC->AddLooseGameplayTag(ArenaGameplayTags::Boss_Phase_One);
	BossASC->AddReplicatedLooseGameplayTag(ArenaGameplayTags::Boss_Phase_One);
}

// 仅在 Combat 使用当前 MaxHealth 计算目标阶段，并保持阶段只向前推进。
void AArenaBossCharacter::EvaluateBossPhase(float NewHealth)
{
	UArenaAbilitySystemComponent* BossASC = GetArenaAbilitySystemComponent();
	const UArenaAttributeSet* BossAttributes = GetArenaAttributeSet();
	const AArenaGameState* ArenaGameState = BoundBossGameState.Get();
	if (!BossASC || !BossAttributes || NewHealth <= 0.0f
		|| !ArenaGameState
		|| ArenaGameState->GetGamePhase() != EArenaGamePhase::Combat
		|| BossASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead))
	{
		return;
	}

	const float MaxHealth = BossAttributes->GetMaxHealth();
	if (MaxHealth <= 0.0f)
	{
		return;
	}

	const float SafePhaseThreeRatio = FMath::Clamp(PhaseThreeHealthRatio, 0.0f, 1.0f);
	const float SafePhaseTwoRatio = FMath::Clamp(
		FMath::Max(PhaseTwoHealthRatio, SafePhaseThreeRatio),
		0.0f,
		1.0f);
	const float HealthRatio = FMath::Clamp(NewHealth / MaxHealth, 0.0f, 1.0f);

	FGameplayTag DesiredPhaseTag = ArenaGameplayTags::Boss_Phase_One;
	int32 DesiredPhaseNumber = 1;
	if (HealthRatio <= SafePhaseThreeRatio)
	{
		DesiredPhaseTag = ArenaGameplayTags::Boss_Phase_Three;
		DesiredPhaseNumber = 3;
	}
	else if (HealthRatio <= SafePhaseTwoRatio)
	{
		DesiredPhaseTag = ArenaGameplayTags::Boss_Phase_Two;
		DesiredPhaseNumber = 2;
	}

	const int32 CurrentPhaseNumber = GetBossPhaseNumber(GetCurrentBossPhaseTag());
	if (DesiredPhaseNumber > CurrentPhaseNumber)
	{
		AdvanceToBossPhase(DesiredPhaseTag, DesiredPhaseNumber);
	}
}

// 新阶段先到达 ASC，再移除旧阶段，避免父标签 Boss.Phase 在客户端短暂归零。
void AArenaBossCharacter::AdvanceToBossPhase(const FGameplayTag& NewPhaseTag, int32 NewPhaseNumber)
{
	UArenaAbilitySystemComponent* BossASC = GetArenaAbilitySystemComponent();
	if (!BossASC || !NewPhaseTag.IsValid())
	{
		return;
	}

	const FGameplayTag OldPhaseTag = GetCurrentBossPhaseTag();
	if (OldPhaseTag == NewPhaseTag)
	{
		return;
	}

	BossASC->AddLooseGameplayTag(NewPhaseTag);
	BossASC->AddReplicatedLooseGameplayTag(NewPhaseTag);
	if (OldPhaseTag.IsValid())
	{
		BossASC->RemoveLooseGameplayTag(OldPhaseTag);
		BossASC->RemoveReplicatedLooseGameplayTag(OldPhaseTag);
	}

	if (NewPhaseNumber == 3)
	{
		ApplyEnrageEffect();
	}
	ExecutePhaseTransitionCue(NewPhaseNumber);

	UE_LOG(
		LogArenaBoss,
		Log,
		TEXT("Boss %s advanced directly to Phase %d at %.1f / %.1f Health."),
		*GetNameSafe(this),
		NewPhaseNumber,
		GetArenaAttributeSet() ? GetArenaAttributeSet()->GetHealth() : 0.0f,
		GetArenaAttributeSet() ? GetArenaAttributeSet()->GetMaxHealth() : 0.0f);
}

// Phase 3 仅应用一次 Infinite GE，属性倍率、Enraged 标签与持续 Cue 共享同一生命周期。
void AArenaBossCharacter::ApplyEnrageEffect()
{
	UArenaAbilitySystemComponent* BossASC = GetArenaAbilitySystemComponent();
	if (!BossASC || EnrageEffectHandle.IsValid())
	{
		return;
	}
	if (!EnrageEffectClass)
	{
		UE_LOG(LogArenaBoss, Error, TEXT("Boss %s has no EnrageEffectClass."), *GetNameSafe(this));
		return;
	}

	FGameplayEffectContextHandle EffectContext = BossASC->MakeEffectContext();
	EffectContext.AddInstigator(this, this);
	EffectContext.AddSourceObject(this);
	const FGameplayEffectSpecHandle SpecHandle = BossASC->MakeOutgoingSpec(
		EnrageEffectClass,
		1.0f,
		EffectContext);
	if (SpecHandle.IsValid())
	{
		EnrageEffectHandle = BossASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}

	if (!EnrageEffectHandle.IsValid())
	{
		UE_LOG(
			LogArenaBoss,
			Error,
			TEXT("Boss %s failed to apply Enrage effect %s."),
			*GetNameSafe(this),
			*GetNameSafe(EnrageEffectClass.Get()));
	}
}

// 清理时先移除 Enrage ActiveGE，再移除阶段叶标签，所有调用均可安全重复。
void AArenaBossCharacter::CleanupBossPhaseState()
{
	UArenaAbilitySystemComponent* BossASC = GetArenaAbilitySystemComponent();
	if (!BossASC)
	{
		EnrageEffectHandle.Invalidate();
		return;
	}

	if (EnrageEffectHandle.IsValid())
	{
		BossASC->RemoveActiveGameplayEffect(EnrageEffectHandle);
		EnrageEffectHandle.Invalidate();
	}

	const FGameplayTag PhaseTags[] = {
		ArenaGameplayTags::Boss_Phase_One,
		ArenaGameplayTags::Boss_Phase_Two,
		ArenaGameplayTags::Boss_Phase_Three
	};
	for (const FGameplayTag& PhaseTag : PhaseTags)
	{
		if (BossASC->HasMatchingGameplayTag(PhaseTag))
		{
			BossASC->RemoveLooseGameplayTag(PhaseTag);
			BossASC->RemoveReplicatedLooseGameplayTag(PhaseTag);
		}
	}
}

// 阶段转换为一次性权威 Cue，RawMagnitude 只编码最终阶段而不回放被跨过的阈值。
void AArenaBossCharacter::ExecutePhaseTransitionCue(int32 NewPhaseNumber)
{
	UArenaAbilitySystemComponent* BossASC = GetArenaAbilitySystemComponent();
	if (!BossASC || NewPhaseNumber <= 1)
	{
		return;
	}

	FGameplayCueParameters CueParameters;
	CueParameters.Instigator = this;
	CueParameters.EffectCauser = this;
	CueParameters.Location = GetActorLocation();
	CueParameters.RawMagnitude = static_cast<float>(NewPhaseNumber);
	BossASC->ExecuteGameplayCue(ArenaGameplayTags::GameplayCue_Boss_Phase_Transition, CueParameters);
}

// 阶段序号只用于服务器单向比较，权威阶段本身仍由 ASC Tag 表示和复制。
int32 AArenaBossCharacter::GetBossPhaseNumber(const FGameplayTag& PhaseTag)
{
	if (PhaseTag == ArenaGameplayTags::Boss_Phase_Three)
	{
		return 3;
	}
	if (PhaseTag == ArenaGameplayTags::Boss_Phase_Two)
	{
		return 2;
	}
	if (PhaseTag == ArenaGameplayTags::Boss_Phase_One)
	{
		return 1;
	}
	return 0;
}

// Health Delegate 只在服务器 Combat 推进阶段；人数缩放窗口会抑制中间比例，治疗也不会回退已有阶段。
void AArenaBossCharacter::HandleBossPhaseHealthChanged(const FOnAttributeChangeData& Data)
{
	if (HasAuthority() && !bSuppressBossPhaseEvaluation)
	{
		EvaluateBossPhase(Data.NewValue);
	}
}

// 各端死亡广播播放一次 Montage；服务器同时清理阶段/召唤物并执行唯一死亡 Cue。
void AArenaBossCharacter::HandleBossPhaseDeath(AArenaEnemyCharacter* Enemy)
{
	if (Enemy != this)
	{
		return;
	}

	if (!bHasPlayedBossDeathPresentation)
	{
		bHasPlayedBossDeathPresentation = true;
		if (DeathMontage)
		{
			if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
			{
				AnimInstance->Montage_Play(DeathMontage, FMath::Max(DeathMontagePlayRate, 0.01f));
			}
		}
	}

	if (HasAuthority())
	{
		DestroyAllBossSummons();
		CleanupBossPhaseState();

		if (!bHasExecutedBossDeathCue)
		{
			bHasExecutedBossDeathCue = true;
			if (UArenaAbilitySystemComponent* BossASC = GetArenaAbilitySystemComponent())
			{
				FGameplayCueParameters CueParameters;
				CueParameters.Location = GetActorLocation();
				CueParameters.Instigator = this;
				CueParameters.EffectCauser = this;
				BossASC->ExecuteGameplayCue(ArenaGameplayTags::GameplayCue_Boss_Death, CueParameters);
			}
		}
	}
}

// 离开 Combat 后清除召唤物和阶段状态；同一 Boss 若重新进入 Combat，则从 Phase 1 重新初始化。
void AArenaBossCharacter::HandleBossGamePhaseChanged(
	EArenaGamePhase OldPhase,
	EArenaGamePhase NewPhase)
{
	if (!HasAuthority())
	{
		return;
	}

	const UArenaAbilitySystemComponent* BossASC = GetArenaAbilitySystemComponent();
	if (NewPhase == EArenaGamePhase::Combat
		&& BossASC
		&& !BossASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead))
	{
		InitializeBossPhaseState();
	}
	else if (NewPhase != EArenaGamePhase::Combat)
	{
		DestroyAllBossSummons();
		CleanupBossPhaseState();
	}
}

// 召唤物死亡不通知 WaveManager，只从 Boss 私有集合移除并立即释放一个召唤容量。
void AArenaBossCharacter::HandleBossSummonDeath(AArenaEnemyCharacter* Enemy)
{
	UnregisterBossSummon(Enemy);
}

// 外部 Destroy 路径与正常死亡共用同一注销入口，保证委托和资格标签不会残留。
void AArenaBossCharacter::HandleBossSummonDestroyed(AActor* DestroyedActor)
{
	UnregisterBossSummon(Cast<AArenaEnemyCharacter>(DestroyedActor));
}

// 默认召唤物显式允许 OnKill 与 OnCrit，后续变体可通过移除资格叶标签改变奖励规则。
void AArenaBossCharacter::AddBossSummonGameplayTags(AArenaEnemyCharacter* SummonedEnemy) const
{
	UArenaAbilitySystemComponent* SummonedASC = SummonedEnemy
		? SummonedEnemy->GetArenaAbilitySystemComponent()
		: nullptr;
	if (!HasAuthority() || !SummonedASC)
	{
		return;
	}

	const FGameplayTag SummonTags[] = {
		ArenaGameplayTags::Enemy_Summoned,
		ArenaGameplayTags::Enemy_Summoned_Trigger_OnKill,
		ArenaGameplayTags::Enemy_Summoned_Trigger_OnCrit
	};
	for (const FGameplayTag& SummonTag : SummonTags)
	{
		SummonedASC->AddLooseGameplayTag(SummonTag);
		SummonedASC->AddReplicatedLooseGameplayTag(SummonTag);
	}
}

// 仅移除本 Boss 注册时添加的三项 loose tag，敌人自身其他构筑和状态标签保持不变。
void AArenaBossCharacter::RemoveBossSummonGameplayTags(AArenaEnemyCharacter* SummonedEnemy) const
{
	UArenaAbilitySystemComponent* SummonedASC = SummonedEnemy
		? SummonedEnemy->GetArenaAbilitySystemComponent()
		: nullptr;
	if (!HasAuthority() || !SummonedASC)
	{
		return;
	}

	const FGameplayTag SummonTags[] = {
		ArenaGameplayTags::Enemy_Summoned,
		ArenaGameplayTags::Enemy_Summoned_Trigger_OnKill,
		ArenaGameplayTags::Enemy_Summoned_Trigger_OnCrit
	};
	for (const FGameplayTag& SummonTag : SummonTags)
	{
		if (SummonedASC->HasMatchingGameplayTag(SummonTag))
		{
			SummonedASC->RemoveLooseGameplayTag(SummonTag);
			SummonedASC->RemoveReplicatedLooseGameplayTag(SummonTag);
		}
	}
}
