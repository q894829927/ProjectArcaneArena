#include "Components/ArenaEnemyAffixComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AI/ArenaEnemyAIController.h"
#include "Character/ArenaBossCharacter.h"
#include "Character/ArenaEnemyCharacter.h"
#include "Character/ArenaPlayerCharacter.h"
#include "Core/ArenaGameState.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GAS/ArenaAbilitySystemComponent.h"
#include "GAS/ArenaAttributeSet.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogArenaEnemyAffix, Log, All);

// 组件复制单个词缀 DataAsset；所有伤害、属性和 Shield 仍由服务器 GAS 决定。
UArenaEnemyAffixComponent::UArenaEnemyAffixComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

// 保存服务器预生成描述，真正初始化由 Enemy 在默认属性之后显式调用。
void UArenaEnemyAffixComponent::ConfigureBeforeSpawn(
	UArenaEnemyAffixDataAsset* InAffixData,
	const FArenaEliteBaselineConfig& InBaselineConfig,
	TSubclassOf<UGameplayEffect> InBaselineEffectClass)
{
	AArenaEnemyCharacter* Enemy = GetEnemyOwner();
	if (!Enemy || !Enemy->HasAuthority() || bInitialized)
	{
		return;
	}
	ActiveAffixData = InAffixData;
	BaselineConfig = InBaselineConfig;
	BaselineEffectClass = InBaselineEffectClass;
}

// 按行为绑定服务器 Timer 或属性委托，并让 Listen Server 立即刷新本地 Badge。
bool UArenaEnemyAffixComponent::InitializeAfterDefaultAttributes()
{
	AArenaEnemyCharacter* Enemy = GetEnemyOwner();
	if (bInitialized || !Enemy || !Enemy->HasAuthority() || !ActiveAffixData)
	{
		return ActiveAffixData == nullptr || bInitialized;
	}

	FText ValidationError;
	if (!ActiveAffixData->IsRuntimeDefinitionValid(&ValidationError) || !BaselineEffectClass)
	{
		UE_LOG(LogArenaEnemyAffix, Error, TEXT("Elite %s has invalid configuration: %s"), *GetNameSafe(Enemy), *ValidationError.ToString());
		return false;
	}
	if (!ApplyEliteBaselineAndTags())
	{
		return false;
	}

	bInitialized = true;
	Enemy->RefreshEliteAffixPresentation();
	if (ActiveAffixData->Behavior != EArenaEnemyAffixBehavior::Frenzy)
	{
		AddLivingAffixCue();
	}

	if (ActiveAffixData->Behavior == EArenaEnemyAffixBehavior::ArcaneWarden)
	{
		const FArenaArcaneWardenAffixConfig& Config = ActiveAffixData->ArcaneWarden;
		GetWorld()->GetTimerManager().SetTimer(
			ArcaneWardenTimerHandle,
			this,
			&UArenaEnemyAffixComponent::ExecuteArcaneWardenPulse,
			FMath::Max(Config.PulseInterval, 0.1f),
			true,
			FMath::Max(Config.InitialDelay, 0.0f));
	}
	else if (ActiveAffixData->Behavior == EArenaEnemyAffixBehavior::Frenzy)
	{
		UArenaAbilitySystemComponent* ASC = GetOwnerASC();
		if (!ASC)
		{
			return false;
		}
		HealthChangedDelegateHandle = ASC->GetGameplayAttributeValueChangeDelegate(
			UArenaAttributeSet::GetHealthAttribute()).AddUObject(this, &UArenaEnemyAffixComponent::HandleOwnerHealthChanged);
	}
	return true;
}

// 使用 SetByCaller Instant GE 应用相对于普通敌人初始值的属性差额。
bool UArenaEnemyAffixComponent::ApplyEliteBaselineAndTags()
{
	UArenaAbilitySystemComponent* ASC = GetOwnerASC();
	const AArenaEnemyCharacter* Enemy = GetEnemyOwner();
	const UArenaAttributeSet* Attributes = Enemy ? Enemy->GetArenaAttributeSet() : nullptr;
	if (!ASC || !Attributes || !BaselineEffectClass || !ActiveAffixData)
	{
		return false;
	}

	const float HealthDelta = Attributes->GetMaxHealth() * FMath::Max(BaselineConfig.MaxHealthMultiplier - 1.0f, 0.0f);
	const float AttackPowerDelta = Attributes->GetAttackPower() * FMath::Max(BaselineConfig.AttackPowerMultiplier - 1.0f, 0.0f);
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(ActiveAffixData);
	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(BaselineEffectClass, 1.0f, Context);
	if (!SpecHandle.IsValid())
	{
		return false;
	}
	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	Spec->SetSetByCallerMagnitude(ArenaGameplayTags::SetByCaller_Elite_MaxHealthDelta, HealthDelta);
	Spec->SetSetByCallerMagnitude(ArenaGameplayTags::SetByCaller_Elite_HealthDelta, HealthDelta);
	Spec->SetSetByCallerMagnitude(ArenaGameplayTags::SetByCaller_Elite_AttackPowerDelta, AttackPowerDelta);
	Spec->SetSetByCallerMagnitude(ArenaGameplayTags::SetByCaller_Elite_DefenseBonus, FMath::Max(BaselineConfig.DefenseBonus, 0.0f));
	ASC->ApplyGameplayEffectSpecToSelf(*Spec);

	ASC->AddLooseGameplayTag(ArenaGameplayTags::Enemy_Elite);
	ASC->AddReplicatedLooseGameplayTag(ArenaGameplayTags::Enemy_Elite);
	ASC->AddLooseGameplayTag(ActiveAffixData->AffixTag);
	ASC->AddReplicatedLooseGameplayTag(ActiveAffixData->AffixTag);
	return true;
}

// 死亡时先停止可重复行为；Volatile 在服务端建立固定位置的延迟爆炸门槛。
bool UArenaEnemyAffixComponent::BeginOwnerDeath()
{
	AArenaEnemyCharacter* Enemy = GetEnemyOwner();
	if (!ActiveAffixData || !Enemy)
	{
		return false;
	}

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ArcaneWardenTimerHandle);
	}
	if (UArenaAbilitySystemComponent* ASC = GetOwnerASC(); ASC && HealthChangedDelegateHandle.IsValid())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UArenaAttributeSet::GetHealthAttribute()).Remove(HealthChangedDelegateHandle);
		HealthChangedDelegateHandle.Reset();
	}
	RemoveLivingAffixCue();
	Enemy->RefreshEliteAffixPresentation();

	if (!Enemy->HasAuthority() || ActiveAffixData->Behavior != EArenaEnemyAffixBehavior::Volatile || bVolatileDeathPending)
	{
		return false;
	}

	bVolatileDeathPending = true;
	VolatileExplosionLocation = Enemy->GetActorLocation();
	if (UArenaAbilitySystemComponent* ASC = GetOwnerASC())
	{
		FGameplayCueParameters Parameters;
		Parameters.Instigator = Enemy;
		Parameters.EffectCauser = Enemy;
		Parameters.SourceObject = ActiveAffixData;
		Parameters.Location = VolatileExplosionLocation;
		Parameters.RawMagnitude = ActiveAffixData->Volatile.ExplosionRadius;
		ASC->AddGameplayCue(ActiveAffixData->Volatile.TelegraphGameplayCueTag, Parameters);
	}
	GetWorld()->GetTimerManager().SetTimer(
		VolatileExplosionTimerHandle,
		this,
		&UArenaEnemyAffixComponent::ExecuteVolatileExplosion,
		FMath::Max(ActiveAffixData->Volatile.TelegraphDuration, 0.01f),
		false);
	return true;
}

// 对范围内每名合法玩家创建独立 Spec，Secondary 防止玩家被动递归扩散。
void UArenaEnemyAffixComponent::ExecuteVolatileExplosion()
{
	AArenaEnemyCharacter* Enemy = GetEnemyOwner();
	UArenaAbilitySystemComponent* SourceASC = GetOwnerASC();
	const AArenaGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AArenaGameState>() : nullptr;
	if (!Enemy || !Enemy->HasAuthority() || !SourceASC || !ActiveAffixData || !GameState
		|| GameState->GetGamePhase() != EArenaGamePhase::Combat)
	{
		CompleteDeferredVolatileDeath();
		return;
	}

	const FArenaVolatileAffixConfig& Config = ActiveAffixData->Volatile;
	SourceASC->RemoveGameplayCue(Config.TelegraphGameplayCueTag);
	FGameplayCueParameters CueParameters;
	CueParameters.Instigator = Enemy;
	CueParameters.EffectCauser = Enemy;
	CueParameters.SourceObject = ActiveAffixData;
	CueParameters.Location = VolatileExplosionLocation;
	CueParameters.RawMagnitude = Config.ExplosionRadius;
	SourceASC->ExecuteGameplayCue(ActiveAffixData->TriggerGameplayCueTag, CueParameters);

	TArray<FOverlapResult> Results;
	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ArenaVolatileElite), false, Enemy);
	GetWorld()->OverlapMultiByObjectType(
		Results,
		VolatileExplosionLocation,
		FQuat::Identity,
		ObjectQuery,
		FCollisionShape::MakeSphere(Config.ExplosionRadius),
		QueryParams);

	TSet<AArenaPlayerCharacter*> DamagedPlayers;
	for (const FOverlapResult& Result : Results)
	{
		AArenaPlayerCharacter* Player = Cast<AArenaPlayerCharacter>(Result.GetActor());
		if (!Player || DamagedPlayers.Contains(Player)
			|| FVector::DistSquared2D(Player->GetActorLocation(), VolatileExplosionLocation) > FMath::Square(Config.ExplosionRadius)
			|| !HasWorldLineOfSightTo(Player))
		{
			continue;
		}
		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Player);
		if (!TargetASC || TargetASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead)
			|| TargetASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Invincible))
		{
			continue;
		}

		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
		Context.AddInstigator(Enemy, Enemy);
		Context.AddSourceObject(ActiveAffixData);
		Context.AddOrigin(VolatileExplosionLocation);
		FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(Config.DamageEffectClass, 1.0f, Context);
		if (!SpecHandle.IsValid())
		{
			continue;
		}
		FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
		Spec->SetSetByCallerMagnitude(ArenaGameplayTags::SetByCaller_Damage_Base, Config.BaseDamage);
		Spec->SetSetByCallerMagnitude(ArenaGameplayTags::SetByCaller_Damage_SkillMultiplier, 1.0f);
		Spec->AddDynamicAssetTag(ArenaGameplayTags::Damage_Physical);
		Spec->AddDynamicAssetTag(ArenaGameplayTags::Damage_Secondary);
		SourceASC->ApplyGameplayEffectSpecToTarget(*Spec, TargetASC);
		DamagedPlayers.Add(Player);
	}
	CompleteDeferredVolatileDeath();
}

// 爆炸或阶段取消后只调用一次 Enemy 的最终死亡广播。
void UArenaEnemyAffixComponent::CompleteDeferredVolatileDeath()
{
	if (!bVolatileDeathPending)
	{
		return;
	}
	bVolatileDeathPending = false;
	if (UArenaAbilitySystemComponent* ASC = GetOwnerASC(); ASC && ActiveAffixData)
	{
		ASC->RemoveGameplayCue(ActiveAffixData->Volatile.TelegraphGameplayCueTag);
	}
	if (AArenaEnemyCharacter* Enemy = GetEnemyOwner())
	{
		Enemy->FinalizeDeferredEnemyDeath();
	}
}

// 世界 Visibility 只让场景遮挡，忽略敌方单位避免队列误挡爆炸。
bool UArenaEnemyAffixComponent::HasWorldLineOfSightTo(const AActor* TargetActor) const
{
	if (!GetWorld() || !TargetActor)
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ArenaVolatileEliteLOS), false, GetOwner());
	for (AArenaEnemyCharacter* Enemy : TActorRange<AArenaEnemyCharacter>(GetWorld()))
	{
		Params.AddIgnoredActor(Enemy);
	}
	FHitResult Hit;
	const FVector TargetLocation = TargetActor->GetActorLocation() + FVector(0.0f, 0.0f, 45.0f);
	const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, VolatileExplosionLocation, TargetLocation, ECC_Visibility, Params);
	return !bBlocked || Hit.GetActor() == TargetActor;
}

// 护阵仅补足同一 WaveManager 管理、非 Boss/召唤、仍存活的其他敌人。
void UArenaEnemyAffixComponent::ExecuteArcaneWardenPulse()
{
	AArenaEnemyCharacter* OwnerEnemy = GetEnemyOwner();
	UArenaAbilitySystemComponent* SourceASC = GetOwnerASC();
	if (!OwnerEnemy || !OwnerEnemy->HasAuthority() || !SourceASC || !ActiveAffixData
		|| OwnerEnemy->IsDeadOrStunned())
	{
		return;
	}

	const FArenaArcaneWardenAffixConfig& Config = ActiveAffixData->ArcaneWarden;
	bool bAppliedShield = false;
	for (AArenaEnemyCharacter* Candidate : TActorRange<AArenaEnemyCharacter>(GetWorld()))
	{
		if (!Candidate || Candidate == OwnerEnemy || Candidate->GetOwner() != OwnerEnemy->GetOwner()
			|| Cast<AArenaBossCharacter>(Candidate))
		{
			continue;
		}
		UAbilitySystemComponent* TargetASC = Candidate->GetAbilitySystemComponent();
		const UArenaAttributeSet* TargetAttributes = Candidate->GetArenaAttributeSet();
		if (!TargetASC || !TargetAttributes
			|| TargetASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead)
			|| TargetASC->HasMatchingGameplayTag(ArenaGameplayTags::Enemy_Summoned)
			|| FVector::DistSquared2D(Candidate->GetActorLocation(), OwnerEnemy->GetActorLocation()) > FMath::Square(Config.Radius))
		{
			continue;
		}
		const float MissingShield = FMath::Max(Config.ShieldCap - TargetAttributes->GetShield(), 0.0f);
		if (MissingShield <= KINDA_SMALL_NUMBER)
		{
			continue;
		}
		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
		Context.AddInstigator(OwnerEnemy, OwnerEnemy);
		Context.AddSourceObject(ActiveAffixData);
		FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(Config.ShieldEffectClass, 1.0f, Context);
		if (!SpecHandle.IsValid())
		{
			continue;
		}
		SpecHandle.Data->SetSetByCallerMagnitude(ArenaGameplayTags::SetByCaller_Shield_Amount, MissingShield);
		SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
		bAppliedShield = true;
	}

	if (bAppliedShield)
	{
		FGameplayCueParameters Parameters;
		Parameters.Instigator = OwnerEnemy;
		Parameters.EffectCauser = OwnerEnemy;
		Parameters.SourceObject = ActiveAffixData;
		Parameters.Location = OwnerEnemy->GetActorLocation();
		Parameters.RawMagnitude = Config.Radius;
		SourceASC->ExecuteGameplayCue(ActiveAffixData->TriggerGameplayCueTag, Parameters);
	}
}

// 狂暴跨过阈值后永久锁存，治疗不会移除且不会重复叠加。
void UArenaEnemyAffixComponent::HandleOwnerHealthChanged(const FOnAttributeChangeData& Data)
{
	AArenaEnemyCharacter* Enemy = GetEnemyOwner();
	UArenaAbilitySystemComponent* ASC = GetOwnerASC();
	const UArenaAttributeSet* Attributes = Enemy ? Enemy->GetArenaAttributeSet() : nullptr;
	if (bFrenzyTriggered || !Enemy || !Enemy->HasAuthority() || !ASC || !Attributes || !ActiveAffixData
		|| Data.NewValue <= 0.0f || Data.NewValue >= Attributes->GetMaxHealth() * ActiveAffixData->Frenzy.HealthThreshold)
	{
		return;
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(ActiveAffixData);
	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(ActiveAffixData->Frenzy.FrenzyEffectClass, 1.0f, Context);
	if (!SpecHandle.IsValid())
	{
		return;
	}
	ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	bFrenzyTriggered = true;
	FGameplayCueParameters Parameters;
	Parameters.Instigator = Enemy;
	Parameters.EffectCauser = Enemy;
	Parameters.SourceObject = ActiveAffixData;
	Parameters.Location = Enemy->GetActorLocation();
	ASC->ExecuteGameplayCue(ActiveAffixData->TriggerGameplayCueTag, Parameters);
}

// Living Cue 与 DataAsset 一一对应，死亡和 EndPlay 都通过同一路径移除。
void UArenaEnemyAffixComponent::AddLivingAffixCue()
{
	UArenaAbilitySystemComponent* ASC = GetOwnerASC();
	if (!ASC || !ActiveAffixData || bLivingCueActive)
	{
		return;
	}
	FGameplayCueParameters Parameters;
	Parameters.Instigator = GetOwner();
	Parameters.EffectCauser = GetOwner();
	Parameters.SourceObject = ActiveAffixData;
	ASC->AddGameplayCue(ActiveAffixData->ActiveGameplayCueTag, Parameters);
	bLivingCueActive = true;
}

void UArenaEnemyAffixComponent::RemoveLivingAffixCue()
{
	if (!bLivingCueActive)
	{
		return;
	}
	if (UArenaAbilitySystemComponent* ASC = GetOwnerASC(); ASC && ActiveAffixData)
	{
		ASC->RemoveGameplayCue(ActiveAffixData->ActiveGameplayCueTag);
	}
	bLivingCueActive = false;
}

// 世界退出或初始化失败只清理自身资源，不补触发死亡爆炸。
void UArenaEnemyAffixComponent::CancelAffixRuntime()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ArcaneWardenTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(VolatileExplosionTimerHandle);
	}
	if (UArenaAbilitySystemComponent* ASC = GetOwnerASC())
	{
		if (HealthChangedDelegateHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UArenaAttributeSet::GetHealthAttribute()).Remove(HealthChangedDelegateHandle);
			HealthChangedDelegateHandle.Reset();
		}
		if (ActiveAffixData && bVolatileDeathPending)
		{
			ASC->RemoveGameplayCue(ActiveAffixData->Volatile.TelegraphGameplayCueTag);
		}
	}
	RemoveLivingAffixCue();
	bVolatileDeathPending = false;
}

// 客户端只从复制 DataAsset 刷新文本和颜色，不运行任何词缀规则。
void UArenaEnemyAffixComponent::OnRep_ActiveAffixData()
{
	if (AArenaEnemyCharacter* Enemy = GetEnemyOwner())
	{
		Enemy->RefreshEliteAffixPresentation();
	}
}

// 复制唯一 ActiveAffixData 指针，具体属性、Tag 与 Cue 继续由 ASC 复制。
void UArenaEnemyAffixComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UArenaEnemyAffixComponent, ActiveAffixData);
}

// 组件退出世界时统一取消 Timer、属性委托和持续 Cue，且不补触发易爆结算。
void UArenaEnemyAffixComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelAffixRuntime();
	Super::EndPlay(EndPlayReason);
}

// 将组件 Owner 收窄为项目敌人类型，所有行为都复用该入口。
AArenaEnemyCharacter* UArenaEnemyAffixComponent::GetEnemyOwner() const
{
	return Cast<AArenaEnemyCharacter>(GetOwner());
}

// 从敌人读取项目 ASC，组件不保存第二份 GAS 所有权引用。
UArenaAbilitySystemComponent* UArenaEnemyAffixComponent::GetOwnerASC() const
{
	const AArenaEnemyCharacter* Enemy = GetEnemyOwner();
	return Enemy ? Enemy->GetArenaAbilitySystemComponent() : nullptr;
}
