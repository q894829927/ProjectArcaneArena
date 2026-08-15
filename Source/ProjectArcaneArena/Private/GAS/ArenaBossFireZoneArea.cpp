#include "GAS/ArenaBossFireZoneArea.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/ArenaPlayerCharacter.h"
#include "Components/SceneComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameplayCueManager.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"

// 火区本体只复制静态位置和表现参数，所有伤害查询与销毁决策保持服务器权威。
AArenaBossFireZoneArea::AArenaBossFireZoneArea()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;
}

// 保存服务器权威数值快照；多个 Area 各自持有独立 Timer 和伤害次数。
void AArenaBossFireZoneArea::InitializeFireZone(
	UAbilitySystemComponent* InSourceASC,
	AActor* InSourceActor,
	TSubclassOf<UGameplayEffect> InDamageEffectClass,
	float InBaseDamage,
	float InSkillMultiplier,
	float InZoneRadius,
	float InDamageHalfHeight,
	float InZoneDuration,
	float InDamageTickInterval)
{
	SourceAbilitySystemComponent = InSourceASC;
	SourceActor = InSourceActor;
	DamageEffectClass = InDamageEffectClass;
	BaseDamage = FMath::Max(InBaseDamage, 0.0f);
	SkillMultiplier = FMath::Max(InSkillMultiplier, 0.0f);
	ZoneRadius = FMath::Max(InZoneRadius, 0.0f);
	DamageHalfHeight = FMath::Max(InDamageHalfHeight, 0.0f);
	ZoneDuration = FMath::Max(InZoneDuration, 0.01f);
	DamageTickInterval = FMath::Max(InDamageTickInterval, 0.01f);
}

// 客户端只启动本地 Active Cue；服务器还会立即结算第一跳并启动后续周期 Timer。
void AArenaBossFireZoneArea::BeginPlay()
{
	Super::BeginPlay();

	NotifyZonePresentationChanged();
	const bool bHasValidPresentation = ZoneRadius > KINDA_SMALL_NUMBER && ZoneDuration > 0.0f;
	if (HasAuthority())
	{
		SetLifeSpan(ZoneDuration);
		if (!SourceAbilitySystemComponent.IsValid() || !SourceActor.IsValid() || !DamageEffectClass
			|| BaseDamage <= KINDA_SMALL_NUMBER || !bHasValidPresentation)
		{
			Destroy();
			return;
		}

		BindServerCleanupDelegates();
		if (IsActorBeingDestroyed())
		{
			return;
		}
	}
	else if (!bHasValidPresentation)
	{
		return;
	}

	AddZoneGameplayCue();
	if (!HasAuthority())
	{
		return;
	}

	MaxDamageTicks = FMath::Max(1, FMath::CeilToInt(ZoneDuration / DamageTickInterval));
	ApplyDamageTick();
	if (DamageTicksApplied < MaxDamageTicks)
	{
		GetWorldTimerManager().SetTimer(
			DamageTickTimerHandle,
			this,
			&AArenaBossFireZoneArea::ApplyDamageTick,
			DamageTickInterval,
			true);
	}
}

// 所有正常和异常销毁路径都共享幂等清理，重叠火区不会互相移除 Cue。
void AArenaBossFireZoneArea::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(DamageTickTimerHandle);
	UnbindServerCleanupDelegates();
	RemoveZoneGameplayCue();

	Super::EndPlay(EndPlayReason);
}

// 只复制客户端重建可视范围所需的数据，伤害配置始终留在服务器。
void AArenaBossFireZoneArea::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AArenaBossFireZoneArea, ZoneRadius);
	DOREPLIFETIME(AArenaBossFireZoneArea, ZoneDuration);
	DOREPLIFETIME(AArenaBossFireZoneArea, DamageTickInterval);
}

// 初始复制或后续参数变化时，用当前最终半径重建独立 Active Cue。
void AArenaBossFireZoneArea::OnRep_ZonePresentation()
{
	NotifyZonePresentationChanged();
	if (HasActorBegunPlay())
	{
		RemoveZoneGameplayCue();
		AddZoneGameplayCue();
	}
}

// 来源 Actor 离开世界时让服务器销毁关联火区。
void AArenaBossFireZoneArea::HandleSourceActorDestroyed(AActor* DestroyedActor)
{
	if (HasAuthority())
	{
		Destroy();
	}
}

// FireZone 只允许存在于 Combat，阶段切换时不等待下一次伤害 Tick。
void AArenaBossFireZoneArea::HandleGamePhaseChanged(EArenaGamePhase OldPhase, EArenaGamePhase NewPhase)
{
	if (HasAuthority() && NewPhase != EArenaGamePhase::Combat)
	{
		Destroy();
	}
}

// 每个 Area 独立绑定同一 Boss 的死亡和终局事件，允许多个重叠区域同时正确清理。
void AArenaBossFireZoneArea::BindServerCleanupDelegates()
{
	if (!HasAuthority())
	{
		return;
	}

	if (UAbilitySystemComponent* SourceASC = SourceAbilitySystemComponent.Get())
	{
		SourceDeadTagDelegateHandle = SourceASC->RegisterGameplayTagEvent(
			ArenaGameplayTags::State_Dead,
			EGameplayTagEventType::NewOrRemoved).AddUObject(
				this,
				&AArenaBossFireZoneArea::HandleSourceDeadTagChanged);
		if (SourceASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead))
		{
			Destroy();
			return;
		}
	}

	if (AActor* Source = SourceActor.Get())
	{
		Source->OnDestroyed.AddUniqueDynamic(this, &AArenaBossFireZoneArea::HandleSourceActorDestroyed);
	}

	AArenaGameState* ArenaGameState = GetWorld() ? GetWorld()->GetGameState<AArenaGameState>() : nullptr;
	if (ArenaGameState)
	{
		BoundGameState = ArenaGameState;
		ArenaGameState->OnGamePhaseChanged.AddUniqueDynamic(this, &AArenaBossFireZoneArea::HandleGamePhaseChanged);
		if (ArenaGameState->GetGamePhase() != EArenaGamePhase::Combat)
		{
			Destroy();
		}
	}
}

// 解绑保存的弱引用，避免来源或 GameState 已开始销毁时重新查询错误对象。
void AArenaBossFireZoneArea::UnbindServerCleanupDelegates()
{
	if (UAbilitySystemComponent* SourceASC = SourceAbilitySystemComponent.Get())
	{
		if (SourceDeadTagDelegateHandle.IsValid())
		{
			SourceASC->RegisterGameplayTagEvent(
				ArenaGameplayTags::State_Dead,
				EGameplayTagEventType::NewOrRemoved).Remove(SourceDeadTagDelegateHandle);
		}
	}
	SourceDeadTagDelegateHandle.Reset();

	if (AActor* Source = SourceActor.Get())
	{
		Source->OnDestroyed.RemoveDynamic(this, &AArenaBossFireZoneArea::HandleSourceActorDestroyed);
	}
	if (AArenaGameState* ArenaGameState = BoundGameState.Get())
	{
		ArenaGameState->OnGamePhaseChanged.RemoveDynamic(this, &AArenaBossFireZoneArea::HandleGamePhaseChanged);
	}
	BoundGameState.Reset();
}

// Boss 死亡标签出现时立即停止后续伤害；移除死亡标签不会恢复旧火区。
void AArenaBossFireZoneArea::HandleSourceDeadTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	if (HasAuthority() && CallbackTag == ArenaGameplayTags::State_Dead && NewCount > 0)
	{
		Destroy();
	}
}

// 每次权威 Tick 先确认伤害来源仍有效，再收集圆柱范围内玩家并执行一次去重结算。
void AArenaBossFireZoneArea::ApplyDamageTick()
{
	if (!HasAuthority() || DamageTicksApplied >= MaxDamageTicks)
	{
		GetWorldTimerManager().ClearTimer(DamageTickTimerHandle);
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AActor* DamageSourceActor = SourceActor.Get();
	UAbilitySystemComponent* SourceASC = SourceAbilitySystemComponent.Get();
	if (!DamageSourceActor || !SourceASC || !DamageEffectClass
		|| SourceASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead))
	{
		Destroy();
		return;
	}

	TArray<FOverlapResult> OverlapResults;
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ArenaBossFireZoneArea), false, this);
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(DamageSourceActor);
	World->OverlapMultiByObjectType(
		OverlapResults,
		GetActorLocation(),
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeBox(FVector(ZoneRadius, ZoneRadius, FMath::Max(DamageHalfHeight, 1.0f))),
		QueryParams);

	TArray<AArenaPlayerCharacter*, TInlineAllocator<4>> DamagedPlayers;
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AArenaPlayerCharacter* PlayerCharacter = Cast<AArenaPlayerCharacter>(OverlapResult.GetActor());
		UAbilitySystemComponent* TargetASC = PlayerCharacter
			? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(PlayerCharacter)
			: nullptr;
		if (!PlayerCharacter || DamagedPlayers.Contains(PlayerCharacter) || !CanDamagePlayer(PlayerCharacter, TargetASC))
		{
			continue;
		}

		DamagedPlayers.Add(PlayerCharacter);
		ApplyDamageToTarget(TargetASC);
	}

	++DamageTicksApplied;
	if (DamageTicksApplied >= MaxDamageTicks)
	{
		GetWorldTimerManager().ClearTimer(DamageTickTimerHandle);
	}
}

// 只允许圆柱范围内的存活、非无敌玩家受伤，避免隔层和 Dash 期间刷新命中表现。
bool AArenaBossFireZoneArea::CanDamagePlayer(
	const AArenaPlayerCharacter* PlayerCharacter,
	UAbilitySystemComponent* TargetASC) const
{
	if (!PlayerCharacter || !TargetASC || TargetASC == SourceAbilitySystemComponent.Get()
		|| TargetASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead)
		|| TargetASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Invincible))
	{
		return false;
	}

	const FVector Delta = PlayerCharacter->GetActorLocation() - GetActorLocation();
	return FVector2D(Delta.X, Delta.Y).SizeSquared() <= FMath::Square(ZoneRadius)
		&& FMath::Abs(Delta.Z) <= DamageHalfHeight;
}

// 每个玩家使用独立 Damage.Fire Spec，使 Shield、Defense、Crit、死亡与 Fire Hit Cue 保持标准行为。
void AArenaBossFireZoneArea::ApplyDamageToTarget(UAbilitySystemComponent* TargetASC)
{
	UAbilitySystemComponent* SourceASC = SourceAbilitySystemComponent.Get();
	AActor* DamageSourceActor = SourceActor.Get();
	if (!SourceASC || !DamageSourceActor || !TargetASC || !DamageEffectClass)
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddInstigator(DamageSourceActor, this);
	EffectContext.AddSourceObject(this);
	EffectContext.AddOrigin(GetActorLocation());
	FGameplayEffectSpecHandle DamageSpecHandle = SourceASC->MakeOutgoingSpec(
		DamageEffectClass,
		1.0f,
		EffectContext);
	if (!DamageSpecHandle.IsValid())
	{
		return;
	}

	FGameplayEffectSpec* DamageSpec = DamageSpecHandle.Data.Get();
	DamageSpec->SetSetByCallerMagnitude(ArenaGameplayTags::SetByCaller_Damage_Base, BaseDamage);
	DamageSpec->SetSetByCallerMagnitude(ArenaGameplayTags::SetByCaller_Damage_SkillMultiplier, SkillMultiplier);
	DamageSpec->AddDynamicAssetTag(ArenaGameplayTags::Damage_Fire);
	SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpec, TargetASC);
}

// 每个复制 Area 在当前世界独立添加 Cue，避免 RemoveGameplayCue(Tag) 清掉其他重叠火区。
void AArenaBossFireZoneArea::AddZoneGameplayCue()
{
	if (bAddedZoneGameplayCue || ZoneRadius <= KINDA_SMALL_NUMBER || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	FGameplayCueParameters CueParameters;
	CueParameters.Instigator = SourceActor.IsValid() ? SourceActor.Get() : GetInstigator();
	CueParameters.EffectCauser = this;
	CueParameters.Location = GetActorLocation();
	CueParameters.RawMagnitude = ZoneRadius;
	UGameplayCueManager::AddGameplayCue_NonReplicated(
		this,
		ArenaGameplayTags::GameplayCue_Ability_Boss_FireZone_Active,
		CueParameters);
	bAddedZoneGameplayCue = true;
}

// 使用相同 Area Target 和半径参数发送 Removed，只回收当前火区的 Cue Actor。
void AArenaBossFireZoneArea::RemoveZoneGameplayCue()
{
	if (!bAddedZoneGameplayCue)
	{
		return;
	}

	FGameplayCueParameters CueParameters;
	CueParameters.Instigator = SourceActor.IsValid() ? SourceActor.Get() : GetInstigator();
	CueParameters.EffectCauser = this;
	CueParameters.Location = GetActorLocation();
	CueParameters.RawMagnitude = ZoneRadius;
	UGameplayCueManager::RemoveGameplayCue_NonReplicated(
		this,
		ArenaGameplayTags::GameplayCue_Ability_Boss_FireZone_Active,
		CueParameters);
	bAddedZoneGameplayCue = false;
}

// 蓝图扩展只接收复制后的最终数值，不参与服务器伤害判定。
void AArenaBossFireZoneArea::NotifyZonePresentationChanged()
{
	K2_OnZonePresentationChanged(ZoneRadius, ZoneDuration);
}
