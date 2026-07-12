#include "GAS/ArenaLightningStormArea.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AArenaLightningStormArea::AArenaLightningStormArea()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	AreaComponent = CreateDefaultSubobject<USphereComponent>(TEXT("AreaComponent"));
	AreaComponent->InitSphereRadius(StormRadius);
	AreaComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AreaComponent->SetGenerateOverlapEvents(false);
	RootComponent = AreaComponent;
}

void AArenaLightningStormArea::InitializeStorm(
	UAbilitySystemComponent* InSourceASC,
	AActor* InSourceActor,
	TSubclassOf<UGameplayEffect> InDamageEffectClass,
	FGameplayTag InDamageTypeTag,
	float InBaseDamage,
	float InSkillMultiplier,
	float InStormRadius,
	float InStormDuration,
	float InDamageTickInterval)
{
	SourceAbilitySystemComponent = InSourceASC;
	SourceActor = InSourceActor;
	DamageEffectClass = InDamageEffectClass;
	DamageTypeTag = InDamageTypeTag;
	BaseDamage = FMath::Max(InBaseDamage, 0.0f);
	SkillMultiplier = FMath::Max(InSkillMultiplier, 0.0f);
	StormRadius = FMath::Max(InStormRadius, 0.0f);
	StormDuration = FMath::Max(InStormDuration, 0.01f);
	DamageTickInterval = FMath::Max(InDamageTickInterval, 0.01f);
	RefreshAreaRadius();
}

void AArenaLightningStormArea::BeginPlay()
{
	Super::BeginPlay();

	RefreshAreaRadius();
	DrawDebugDamageRadius();

	if (!HasAuthority())
	{
		return;
	}

	SetLifeSpan(StormDuration);

	if (!SourceAbilitySystemComponent.IsValid() || !DamageEffectClass || StormRadius <= 0.0f)
	{
		Destroy();
		return;
	}

	FGameplayCueParameters CueParameters;
	CueParameters.Instigator = SourceActor.Get();
	CueParameters.EffectCauser = this;
	CueParameters.Location = GetActorLocation();
	SourceAbilitySystemComponent->AddGameplayCue(
		ArenaGameplayTags::GameplayCue_Ability_LightningStorm_Active,
		CueParameters);
	bAddedActiveGameplayCue = true;

	MaxDamageTicks = FMath::Max(1, FMath::CeilToInt(StormDuration / DamageTickInterval));
	ApplyDamageTick();

	if (DamageTicksApplied < MaxDamageTicks)
	{
		GetWorldTimerManager().SetTimer(
			DamageTickTimerHandle,
			this,
			&AArenaLightningStormArea::ApplyDamageTick,
			DamageTickInterval,
			true);
	}
}

void AArenaLightningStormArea::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(DamageTickTimerHandle);
	if (HasAuthority() && bAddedActiveGameplayCue && SourceAbilitySystemComponent.IsValid())
	{
		SourceAbilitySystemComponent->RemoveGameplayCue(
			ArenaGameplayTags::GameplayCue_Ability_LightningStorm_Active);
	}
	bAddedActiveGameplayCue = false;

	Super::EndPlay(EndPlayReason);
}

void AArenaLightningStormArea::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AArenaLightningStormArea, StormRadius);
	DOREPLIFETIME(AArenaLightningStormArea, StormDuration);
	DOREPLIFETIME(AArenaLightningStormArea, DamageTickInterval);
}

void AArenaLightningStormArea::OnRep_StormRadius()
{
	RefreshAreaRadius();
}

void AArenaLightningStormArea::ApplyDamageTick()
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

	TArray<FOverlapResult> OverlapResults;
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ArenaLightningStormArea), false, this);
	QueryParams.AddIgnoredActor(this);
	if (SourceActor.IsValid())
	{
		QueryParams.AddIgnoredActor(SourceActor.Get());
	}

	World->OverlapMultiByObjectType(
		OverlapResults,
		GetActorLocation(),
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(StormRadius),
		QueryParams);

	TSet<AActor*> DamagedActors;
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* TargetActor = OverlapResult.GetActor();
		if (!TargetActor || DamagedActors.Contains(TargetActor))
		{
			continue;
		}

		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
		if (!CanDamageTarget(TargetActor, TargetASC))
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

bool AArenaLightningStormArea::CanDamageTarget(AActor* TargetActor, UAbilitySystemComponent* TargetASC) const
{
	if (!TargetActor || TargetActor == this || TargetActor == SourceActor.Get() || !TargetASC)
	{
		return false;
	}

	// Overlap 查询用于快速收集候选目标；再按角色中心做二维圆形校验，避免 Capsule 边缘在圈外仍被命中。
	const FVector TargetOffset = TargetActor->GetActorLocation() - GetActorLocation();
	if (TargetOffset.SizeSquared2D() > FMath::Square(StormRadius))
	{
		return false;
	}

	if (SourceAbilitySystemComponent.IsValid() && TargetASC == SourceAbilitySystemComponent.Get())
	{
		return false;
	}

	if (TargetASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead))
	{
		return false;
	}

	return true;
}

void AArenaLightningStormArea::ApplyDamageToTarget(UAbilitySystemComponent* TargetASC)
{
	UAbilitySystemComponent* SourceASC = SourceAbilitySystemComponent.Get();
	if (!SourceASC || !TargetASC || !DamageEffectClass)
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);
	EffectContext.AddInstigator(SourceActor.Get(), this);

	FGameplayEffectSpecHandle DamageSpecHandle = SourceASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, EffectContext);
	if (!DamageSpecHandle.IsValid())
	{
		return;
	}

	FGameplayEffectSpec* DamageSpec = DamageSpecHandle.Data.Get();
	DamageSpec->SetSetByCallerMagnitude(ArenaGameplayTags::SetByCaller_Damage_Base, BaseDamage);
	DamageSpec->SetSetByCallerMagnitude(ArenaGameplayTags::SetByCaller_Damage_SkillMultiplier, SkillMultiplier);

	if (DamageTypeTag.IsValid())
	{
		DamageSpec->AddDynamicAssetTag(DamageTypeTag);
	}

	SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpec, TargetASC);
}

void AArenaLightningStormArea::RefreshAreaRadius() const
{
	if (AreaComponent)
	{
		AreaComponent->SetSphereRadius(StormRadius, true);
	}
}

// 在开发构建中绘制与服务器二维伤害判定一致的地面圆环。
void AArenaLightningStormArea::DrawDebugDamageRadius() const
{
#if ENABLE_DRAW_DEBUG
	UWorld* World = GetWorld();
	if (!bDrawDebugRadius || !World || GetNetMode() == NM_DedicatedServer || StormRadius <= 0.0f)
	{
		return;
	}

	const FVector CircleCenter = GetActorLocation() + FVector(0.0f, 0.0f, 10.0f);
	DrawDebugCircle(
		World,
		CircleCenter,
		StormRadius,
		64,
		DebugRadiusColor,
		false,
		StormDuration,
		0,
		DebugRadiusThickness,
		FVector::ForwardVector,
		FVector::RightVector,
		false);
#endif
}
