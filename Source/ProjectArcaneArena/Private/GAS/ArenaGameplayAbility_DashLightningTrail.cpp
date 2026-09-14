#include "GAS/ArenaGameplayAbility_DashLightningTrail.h"

#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemComponent.h"
#include "Core/ArenaUpgradeDataAsset.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GAS/ArenaDashTrailArea.h"
#include "GAS/ArenaAbilityNetworkDebug.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffect.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogArenaDashBuild, Log, All);

// 建立服务器事件被动并提供原生 Trail Area 后备类，蓝图仍可覆盖表现子类。
UArenaGameplayAbility_DashLightningTrail::UArenaGameplayAbility_DashLightningTrail()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	TrailAreaClass = AArenaDashTrailArea::StaticClass();

	SetAssetTags(FGameplayTagContainer(ArenaGameplayTags::Ability_Passive_DashLightningTrail));
	ActivationBlockedTags.AddTag(ArenaGameplayTags::State_Dead);

	FAbilityTriggerData& DashEndTrigger = AbilityTriggers.AddDefaulted_GetRef();
	DashEndTrigger.TriggerTag = ArenaGameplayTags::Trigger_OnDashEnd;
	DashEndTrigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
}

// GAS 激活前确认事件归属、升级标签和路径 TargetData，避免无效事件启动被动实例。
bool UArenaGameplayAbility_DashLightningTrail::ShouldAbilityRespondToEvent(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* Payload) const
{
	if (!Super::ShouldAbilityRespondToEvent(ActorInfo, Payload))
	{
		return false;
	}

	const UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	FVector TrailStart = FVector::ZeroVector;
	FVector TrailEnd = FVector::ZeroVector;
	return ActorInfo
		&& ActorInfo->IsNetAuthority()
		&& Payload
		&& Payload->EventTag.MatchesTagExact(ArenaGameplayTags::Trigger_OnDashEnd)
		&& Payload->Instigator.Get() == ActorInfo->AvatarActor.Get()
		&& Payload->Target.Get() == ActorInfo->AvatarActor.Get()
		&& SourceASC
		&& SourceASC->HasMatchingGameplayTag(ArenaGameplayTags::Upgrade_Dash_LightningTrail)
		&& ExtractDashPath(Payload, TrailStart, TrailEnd);
}

// 服务器校验升级 SourceObject 后只生成一个 Trail Area，客户端不参与 Actor 或伤害创建。
void UArenaGameplayAbility_DashLightningTrail::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	AActor* SourceAvatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UWorld* World = SourceAvatar ? SourceAvatar->GetWorld() : nullptr;
	const UArenaUpgradeDataAsset* UpgradeData = Cast<UArenaUpgradeDataAsset>(GetCurrentSourceObject());
	const bool bValidUpgradeRoute = UpgradeData
		&& UpgradeData->UpgradeTags.HasTagExact(ArenaGameplayTags::Build_Dash)
		&& UpgradeData->UpgradeTags.HasTagExact(ArenaGameplayTags::Upgrade_Dash_LightningTrail)
		&& UpgradeData->TargetAbilityTag.MatchesTagExact(ArenaGameplayTags::Ability_Passive_DashLightningTrail)
		&& UpgradeData->TriggerEventTag.MatchesTagExact(ArenaGameplayTags::Trigger_OnDashEnd)
		&& UpgradeData->DamageTypeTag.MatchesTagExact(ArenaGameplayTags::Damage_Lightning);
	FVector TrailStart = FVector::ZeroVector;
	FVector TrailEnd = FVector::ZeroVector;
	const bool bValidPath = ExtractDashPath(TriggerEventData, TrailStart, TrailEnd);
	const float BaseDamage = bValidUpgradeRoute ? FMath::Max(UpgradeData->NumericValue, 0.0f) : 0.0f;

	if (!ActorInfo || !ActorInfo->IsNetAuthority() || !SourceASC || !SourceAvatar || !World
		|| !DamageEffectClass || !TrailAreaClass || !bValidPath || BaseDamage <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogArenaDashBuild, Warning,
			TEXT("DashLightningTrail activation on %s has invalid event, upgrade data, DamageEffectClass, TrailAreaClass, or path."),
			*GetNameSafe(SourceASC ? SourceASC->GetOwnerActor() : nullptr));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FVector TrailDirection = (TrailEnd - TrailStart).GetSafeNormal2D();
	const FVector TrailMidpoint = (TrailStart + TrailEnd) * 0.5f;
	const FTransform SpawnTransform(TrailDirection.Rotation(), TrailMidpoint);
	AArenaDashTrailArea* TrailArea = World->SpawnActorDeferred<AArenaDashTrailArea>(
		TrailAreaClass,
		SpawnTransform,
		SourceAvatar,
		Cast<APawn>(SourceAvatar),
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!TrailArea)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	TrailArea->InitializeTrail(
		SourceASC,
		SourceAvatar,
		UpgradeData,
		DamageEffectClass,
		BaseDamage,
		TrailStart,
		TrailEnd,
		TrailRadius,
		TrailDuration,
		DamageTickInterval);
	UGameplayStatics::FinishSpawningActor(TrailArea, SpawnTransform);
	if (ArenaAbilityNetworkDebug::IsAuditEnabled())
	{
		UE_LOG(LogArenaAbilityNet, Log, TEXT("[%llu] DashTrail Spawned Area=%s Source=%s Start=%s End=%s"),
			ArenaAbilityNetworkDebug::NextServerExecutionSequence(),
			*GetNameSafe(TrailArea),
			*GetNameSafe(SourceAvatar),
			*TrailStart.ToCompactString(),
			*TrailEnd.ToCompactString());
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

// 只接受一条内置 LocationInfo，并拒绝 NaN、无穷值和几乎没有位移的路径。
bool UArenaGameplayAbility_DashLightningTrail::ExtractDashPath(
	const FGameplayEventData* TriggerEventData,
	FVector& OutTrailStart,
	FVector& OutTrailEnd) const
{
	if (!TriggerEventData || TriggerEventData->TargetData.Num() != 1)
	{
		return false;
	}

	const FGameplayAbilityTargetData* RawData = TriggerEventData->TargetData.Get(0);
	if (!RawData || RawData->GetScriptStruct() != FGameplayAbilityTargetData_LocationInfo::StaticStruct())
	{
		return false;
	}

	const FGameplayAbilityTargetData_LocationInfo* LocationData =
		static_cast<const FGameplayAbilityTargetData_LocationInfo*>(RawData);
	OutTrailStart = LocationData->GetOrigin().GetLocation();
	OutTrailEnd = LocationData->GetEndPoint();
	return !OutTrailStart.ContainsNaN()
		&& !OutTrailEnd.ContainsNaN()
		&& FVector::DistSquared2D(OutTrailStart, OutTrailEnd) > FMath::Square(1.0f);
}
