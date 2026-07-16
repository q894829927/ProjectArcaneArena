#include "GAS/ArenaDashTrailArea.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/ArenaEnemyCharacter.h"
#include "Components/SceneComponent.h"
#include "Core/ArenaUpgradeDataAsset.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffect.h"
#include "GameplayCueManager.h"
#include "Net/UnrealNetwork.h"

// 创建静态复制路径 Actor；客户端不运行伤害查询或生命周期决策。
AArenaDashTrailArea::AArenaDashTrailArea()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;
}

// 保存服务器权威路径和数值快照，并在 FinishSpawning 前建立正确的世界变换。
void AArenaDashTrailArea::InitializeTrail(
	UAbilitySystemComponent* InSourceASC,
	AActor* InSourceActor,
	const UArenaUpgradeDataAsset* InUpgradeData,
	TSubclassOf<UGameplayEffect> InDamageEffectClass,
	float InBaseDamage,
	const FVector& InTrailStart,
	const FVector& InTrailEnd,
	float InTrailRadius,
	float InTrailDuration,
	float InDamageTickInterval)
{
	SourceAbilitySystemComponent = InSourceASC;
	SourceActor = InSourceActor;
	UpgradeData = const_cast<UArenaUpgradeDataAsset*>(InUpgradeData);
	DamageEffectClass = InDamageEffectClass;
	BaseDamage = FMath::Max(InBaseDamage, 0.0f);
	TrailStart = InTrailStart;
	TrailEnd = InTrailEnd;
	TrailRadius = FMath::Max(InTrailRadius, 0.0f);
	TrailDuration = FMath::Max(InTrailDuration, 0.01f);
	DamageTickInterval = FMath::Max(InDamageTickInterval, 0.01f);
	RefreshTrailTransform();
}

// 各端基于复制 Area 启动独立 Cue；服务器额外验证数据并运行周期伤害。
void AArenaDashTrailArea::BeginPlay()
{
	Super::BeginPlay();

	RefreshTrailTransform();
	NotifyTrailGeometryChanged();
	const bool bHasValidGeometry = TrailRadius > KINDA_SMALL_NUMBER
		&& FVector::DistSquared2D(TrailStart, TrailEnd) > FMath::Square(1.0f);
	if (HasAuthority())
	{
		SetLifeSpan(TrailDuration);
		if (!SourceAbilitySystemComponent.IsValid() || !SourceActor.IsValid() || !UpgradeData.IsValid()
			|| !DamageEffectClass || BaseDamage <= KINDA_SMALL_NUMBER || !bHasValidGeometry)
		{
			Destroy();
			return;
		}
	}
	else if (!bHasValidGeometry)
	{
		return;
	}

	AddTrailGameplayCue();
	if (!HasAuthority())
	{
		return;
	}

	MaxDamageTicks = FMath::Max(1, FMath::CeilToInt(TrailDuration / DamageTickInterval));
	ApplyDamageTick();
	if (DamageTicksApplied < MaxDamageTicks)
	{
		GetWorldTimerManager().SetTimer(
			DamageTickTimerHandle,
			this,
			&AArenaDashTrailArea::ApplyDamageTick,
			DamageTickInterval,
			true);
	}
}

// 成对结束计时和本 Area 的 GameplayCue，不影响同玩家的其他重叠路径。
void AArenaDashTrailArea::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(DamageTickTimerHandle);
	RemoveTrailGameplayCue();

	Super::EndPlay(EndPlayReason);
}

// 注册客户端表现所需的路径几何和生命周期复制字段。
void AArenaDashTrailArea::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AArenaDashTrailArea, TrailStart);
	DOREPLIFETIME(AArenaDashTrailArea, TrailEnd);
	DOREPLIFETIME(AArenaDashTrailArea, TrailRadius);
	DOREPLIFETIME(AArenaDashTrailArea, TrailDuration);
	DOREPLIFETIME(AArenaDashTrailArea, DamageTickInterval);
}

// 复制字段到达后刷新 Actor 方向，并在延迟到达时补启本 Area 的 Cue。
void AArenaDashTrailArea::OnRep_TrailGeometry()
{
	RefreshTrailTransform();
	NotifyTrailGeometryChanged();
	if (HasActorBegunPlay())
	{
		AddTrailGameplayCue();
	}
}

// 使用包围路径的旋转 Box 收集候选，再通过严格二维线段距离过滤和结算。
void AArenaDashTrailArea::ApplyDamageTick()
{
	if (!HasAuthority() || DamageTicksApplied >= MaxDamageTicks)
	{
		GetWorldTimerManager().ClearTimer(DamageTickTimerHandle);
		return;
	}

	UWorld* World = GetWorld();
	const float TrailLength = FVector::Dist2D(TrailStart, TrailEnd);
	if (!World || TrailLength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	TArray<FOverlapResult> OverlapResults;
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ArenaDashTrailArea), false, this);
	QueryParams.AddIgnoredActor(this);
	if (SourceActor.IsValid())
	{
		QueryParams.AddIgnoredActor(SourceActor.Get());
	}
	World->OverlapMultiByObjectType(
		OverlapResults,
		GetActorLocation(),
		GetActorQuat(),
		ObjectQueryParams,
		FCollisionShape::MakeBox(FVector(TrailLength * 0.5f + TrailRadius, TrailRadius, TrailRadius)),
		QueryParams);

	TSet<AActor*> DamagedActors;
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* TargetActor = OverlapResult.GetActor();
		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
		if (!TargetActor || DamagedActors.Contains(TargetActor) || !CanDamageTarget(TargetActor, TargetASC))
		{
			continue;
		}

		DamagedActors.Add(TargetActor);
		ApplyDamageToTarget(TargetASC);
	}

	++DamageTicksApplied;
	if (DamageTicksApplied >= MaxDamageTicks)
	{
		GetWorldTimerManager().ClearTimer(DamageTickTimerHandle);
	}
}

// 只允许路径半径内的存活敌人受伤，并由现有 Invincible 规则阻止无效状态刷新。
bool AArenaDashTrailArea::CanDamageTarget(AActor* TargetActor, UAbilitySystemComponent* TargetASC) const
{
	const AArenaEnemyCharacter* Enemy = Cast<AArenaEnemyCharacter>(TargetActor);
	if (!Enemy || !TargetASC || TargetActor == SourceActor.Get()
		|| TargetASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead)
		|| TargetASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Invincible))
	{
		return false;
	}

	FVector FlatStart = TrailStart;
	FVector FlatEnd = TrailEnd;
	FVector FlatTarget = TargetActor->GetActorLocation();
	FlatStart.Z = 0.0f;
	FlatEnd.Z = 0.0f;
	FlatTarget.Z = 0.0f;
	return FMath::PointDistToSegment(FlatTarget, FlatStart, FlatEnd) <= TrailRadius;
}

// 每个目标使用独立 Lightning Spec，使 Crit、Defense、Shield、Shocked 和事件路由保持标准行为。
void AArenaDashTrailArea::ApplyDamageToTarget(UAbilitySystemComponent* TargetASC) const
{
	UAbilitySystemComponent* SourceASC = SourceAbilitySystemComponent.Get();
	if (!SourceASC || !TargetASC || !DamageEffectClass || !UpgradeData.IsValid())
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddInstigator(SourceActor.Get(), const_cast<AArenaDashTrailArea*>(this));
	EffectContext.AddSourceObject(const_cast<UArenaUpgradeDataAsset*>(UpgradeData.Get()));
	FGameplayEffectSpecHandle DamageSpecHandle = SourceASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, EffectContext);
	if (!DamageSpecHandle.IsValid())
	{
		return;
	}

	FGameplayEffectSpec* DamageSpec = DamageSpecHandle.Data.Get();
	DamageSpec->SetSetByCallerMagnitude(ArenaGameplayTags::SetByCaller_Damage_Base, BaseDamage);
	DamageSpec->SetSetByCallerMagnitude(ArenaGameplayTags::SetByCaller_Damage_SkillMultiplier, 1.0f);
	DamageSpec->AddDynamicAssetTag(ArenaGameplayTags::Damage_Lightning);
	SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpec, TargetASC);
}

// 让 Actor 的 X 轴沿 Dash 路径，GameplayCue 的 Normal 也使用相同方向。
void AArenaDashTrailArea::RefreshTrailTransform()
{
	const FVector TrailDirection = (TrailEnd - TrailStart).GetSafeNormal2D();
	SetActorLocation((TrailStart + TrailEnd) * 0.5f);
	if (!TrailDirection.IsNearlyZero())
	{
		SetActorRotation(TrailDirection.Rotation());
	}
}

// 将纯表现扩展点集中到一个事件，重复 OnRep 只需按当前最终值重建表现。
void AArenaDashTrailArea::NotifyTrailGeometryChanged()
{
	K2_OnTrailGeometryChanged(TrailStart, TrailEnd, TrailRadius, TrailDuration);
}

// 每个复制 Area 在当前世界独立驱动 Cue，避免 ASC RemoveGameplayCue(Tag) 批量移除重叠路径。
void AArenaDashTrailArea::AddTrailGameplayCue()
{
	if (bAddedTrailGameplayCue || TrailRadius <= KINDA_SMALL_NUMBER
		|| FVector::DistSquared2D(TrailStart, TrailEnd) <= FMath::Square(1.0f))
	{
		return;
	}

	FGameplayCueParameters CueParameters;
	CueParameters.Instigator = SourceActor.IsValid() ? SourceActor.Get() : GetInstigator();
	CueParameters.EffectCauser = this;
	CueParameters.SourceObject = UpgradeData.Get();
	CueParameters.Location = GetActorLocation() + FVector(0.0f, 0.0f, 10.0f);
	CueParameters.Normal = (TrailEnd - TrailStart).GetSafeNormal2D();
	CueParameters.RawMagnitude = FVector::Dist2D(TrailStart, TrailEnd);
	UGameplayCueManager::AddGameplayCue_NonReplicated(
		this,
		ArenaGameplayTags::GameplayCue_Ability_Dash_Trail,
		CueParameters);
	bAddedTrailGameplayCue = true;
}

// 使用相同 Area Target 和路径参数发送 Removed，只回收这条路径的 Cue Actor。
void AArenaDashTrailArea::RemoveTrailGameplayCue()
{
	if (!bAddedTrailGameplayCue)
	{
		return;
	}

	FGameplayCueParameters CueParameters;
	CueParameters.Instigator = SourceActor.IsValid() ? SourceActor.Get() : GetInstigator();
	CueParameters.EffectCauser = this;
	CueParameters.SourceObject = UpgradeData.Get();
	CueParameters.Location = GetActorLocation() + FVector(0.0f, 0.0f, 10.0f);
	CueParameters.Normal = (TrailEnd - TrailStart).GetSafeNormal2D();
	CueParameters.RawMagnitude = FVector::Dist2D(TrailStart, TrailEnd);
	UGameplayCueManager::RemoveGameplayCue_NonReplicated(
		this,
		ArenaGameplayTags::GameplayCue_Ability_Dash_Trail,
		CueParameters);
	bAddedTrailGameplayCue = false;
}
