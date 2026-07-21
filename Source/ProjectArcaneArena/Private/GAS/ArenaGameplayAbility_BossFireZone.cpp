#include "GAS/ArenaGameplayAbility_BossFireZone.h"

#include "AbilitySystemComponent.h"
#include "Character/ArenaEnemyCharacter.h"
#include "Core/ArenaGameState.h"
#include "Core/ArenaLogCategories.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GAS/ArenaBossFireZoneArea.h"
#include "GAS/ArenaGameplayEffect_BossFireZoneCooldown.h"
#include "GAS/ArenaGameplayTags.h"
#include "Kismet/GameplayStatics.h"

// FireZone 复用敌人攻击基类的 Commit、Montage、State.Attacking 和取消生命周期。
UArenaGameplayAbility_BossFireZone::UArenaGameplayAbility_BossFireZone()
{
	SetAssetTags(FGameplayTagContainer(ArenaGameplayTags::Ability_Enemy_Boss_FireZone));
	ActivationBlockedTags.AddTag(ArenaGameplayTags::Cooldown_Enemy_Boss_FireZone);
	ActivationBlockedTags.AddTag(ArenaGameplayTags::State_Casting);
	CooldownGameplayEffectClass = UArenaGameplayEffect_BossFireZoneCooldown::StaticClass();
}

// 地面解析失败时在 Commit 前结束；提交成功后固定位置并显示一秒预警。
void UArenaGameplayAbility_BossFireZone::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	AArenaEnemyCharacter* SourceEnemy = ActorInfo
		? Cast<AArenaEnemyCharacter>(ActorInfo->AvatarActor.Get())
		: nullptr;
	const AArenaGameState* ArenaGameState = SourceEnemy && SourceEnemy->GetWorld()
		? SourceEnemy->GetWorld()->GetGameState<AArenaGameState>()
		: nullptr;
	AActor* TargetActor = SourceEnemy ? SourceEnemy->GetCombatTarget() : nullptr;
	if (!SourceEnemy || !SourceEnemy->HasAuthority() || !ArenaGameState
		|| ArenaGameState->GetGamePhase() != EArenaGamePhase::Combat)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bHasFireZoneLocation = ResolveFireZoneLocation(SourceEnemy, TargetActor, FireZoneLocation);
	if (!bHasFireZoneLocation)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive() || !SourceEnemy)
	{
		return;
	}

	if (UCharacterMovementComponent* MovementComponent = SourceEnemy->GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
	}
	if (AController* Controller = SourceEnemy->GetController())
	{
		Controller->StopMovement();
	}

	UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	AddCastingStateTag(SourceASC);
	AddTelegraphCue(SourceASC, SourceEnemy);
}

// 所有结束路径先清理 FireZone 专属状态，再由基类移除 State.Attacking 和销毁 AbilityTask。
void UArenaGameplayAbility_BossFireZone::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	RemoveTelegraphCue(SourceASC);
	RemoveCastingStateTag(SourceASC);
	FireZoneLocation = FVector::ZeroVector;
	bHasFireZoneLocation = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// BT 与 Commit 共用视线和地面判定，墙后或无地面目标会回退到 Chase。
bool UArenaGameplayAbility_BossFireZone::HasAttackLineOfSight(
	AArenaEnemyCharacter* SourceEnemy,
	AActor* TargetActor) const
{
	FVector ResolvedLocation = FVector::ZeroVector;
	return Super::HasAttackLineOfSight(SourceEnemy, TargetActor)
		&& ResolveFireZoneLocation(SourceEnemy, TargetActor, ResolvedLocation);
}

// 缺失动画、伤害 GE、Area Class 或有效范围时拒绝 Commit，避免只消耗冷却不生成火区。
bool UArenaGameplayAbility_BossFireZone::HasRequiredAttackConfiguration() const
{
	return Super::HasRequiredAttackConfiguration()
		&& DamageEffectClass
		&& FireZoneAreaClass
		&& ZoneRadius > KINDA_SMALL_NUMBER
		&& ZoneDuration > 0.0f
		&& DamageTickInterval > 0.0f;
}

// 固定预警兑现时不再读取原目标状态，只由服务器生成一个初始化完整的复制 Area。
void UArenaGameplayAbility_BossFireZone::ExecuteAttack(
	AArenaEnemyCharacter* SourceEnemy,
	AActor* TargetActor,
	UAbilitySystemComponent* SourceASC,
	UAbilitySystemComponent* TargetASC)
{
	RemoveTelegraphCue(SourceASC);
	RemoveCastingStateTag(SourceASC);

	if (!SourceEnemy || !SourceEnemy->HasAuthority() || !SourceASC || !bHasFireZoneLocation
		|| !DamageEffectClass || !FireZoneAreaClass)
	{
		return;
	}

	UWorld* World = SourceEnemy->GetWorld();
	if (!World)
	{
		return;
	}

	const FTransform SpawnTransform(FRotator::ZeroRotator, FireZoneLocation);
	AArenaBossFireZoneArea* FireZoneArea = World->SpawnActorDeferred<AArenaBossFireZoneArea>(
		FireZoneAreaClass,
		SpawnTransform,
		SourceEnemy,
		SourceEnemy,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!FireZoneArea)
	{
		UE_LOG(LogArenaBoss, Error, TEXT("Boss %s failed to spawn FireZone area class %s."),
			*GetNameSafe(SourceEnemy), *GetNameSafe(FireZoneAreaClass.Get()));
		return;
	}

	FireZoneArea->InitializeFireZone(
		SourceASC,
		SourceEnemy,
		DamageEffectClass,
		BaseDamage,
		SkillMultiplier,
		ZoneRadius,
		DamageHalfHeight,
		ZoneDuration,
		DamageTickInterval);
	UGameplayStatics::FinishSpawningActor(FireZoneArea, SpawnTransform);
}

// 从目标中心稍上方向下追踪，忽略双方 Pawn，命中点作为后续不再变化的世界落点。
bool UArenaGameplayAbility_BossFireZone::ResolveFireZoneLocation(
	const AArenaEnemyCharacter* SourceEnemy,
	const AActor* TargetActor,
	FVector& OutLocation) const
{
	OutLocation = FVector::ZeroVector;
	UWorld* World = SourceEnemy ? SourceEnemy->GetWorld() : nullptr;
	if (!World || !TargetActor)
	{
		return false;
	}

	const FVector TraceStart = TargetActor->GetActorLocation()
		+ FVector(0.0f, 0.0f, FMath::Max(GroundTraceStartHeight, 0.0f));
	const FVector TraceEnd = TargetActor->GetActorLocation()
		- FVector(0.0f, 0.0f, FMath::Max(GroundTraceDistance, 1.0f));
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BossFireZoneGroundTrace), false, SourceEnemy);
	QueryParams.AddIgnoredActor(SourceEnemy);
	QueryParams.AddIgnoredActor(TargetActor);

	FHitResult GroundHit;
	if (!World->LineTraceSingleByChannel(
		GroundHit,
		TraceStart,
		TraceEnd,
		ECC_Visibility,
		QueryParams)
		|| !GroundHit.bBlockingHit)
	{
		return false;
	}

	OutLocation = GroundHit.ImpactPoint;
	return !OutLocation.ContainsNaN();
}

// Telegraph 使用 Boss ASC 复制，但 Cue Actor 根据 Location 固定在世界中而不附着 Boss。
void UArenaGameplayAbility_BossFireZone::AddTelegraphCue(
	UAbilitySystemComponent* SourceASC,
	AActor* SourceActor)
{
	if (!SourceASC || !SourceActor || !bHasFireZoneLocation || bTelegraphActive)
	{
		return;
	}

	FGameplayCueParameters CueParameters;
	CueParameters.Instigator = SourceActor;
	CueParameters.EffectCauser = SourceActor;
	CueParameters.Location = FireZoneLocation;
	CueParameters.RawMagnitude = ZoneRadius;
	SourceASC->AddGameplayCue(ArenaGameplayTags::GameplayCue_Ability_Boss_FireZone_Telegraph, CueParameters);
	bTelegraphActive = true;
}

// 释放、Stun、死亡、终局和 Montage 中断都调用本幂等路径。
void UArenaGameplayAbility_BossFireZone::RemoveTelegraphCue(UAbilitySystemComponent* SourceASC)
{
	if (!bTelegraphActive)
	{
		return;
	}

	if (SourceASC)
	{
		SourceASC->RemoveGameplayCue(ArenaGameplayTags::GameplayCue_Ability_Boss_FireZone_Telegraph);
	}
	bTelegraphActive = false;
}

// Casting 与基类 Attacking 同时存在于前摇，复制标签供客户端调试和表现观察。
void UArenaGameplayAbility_BossFireZone::AddCastingStateTag(UAbilitySystemComponent* SourceASC)
{
	if (!SourceASC || bAppliedCastingStateTag)
	{
		return;
	}

	SourceASC->AddLooseGameplayTag(ArenaGameplayTags::State_Casting);
	SourceASC->AddReplicatedLooseGameplayTag(ArenaGameplayTags::State_Casting);
	bAppliedCastingStateTag = true;
}

// Area 生成后立即移除 Casting；Montage 后摇期间仍由 State.Attacking 阻止寻路和并行攻击。
void UArenaGameplayAbility_BossFireZone::RemoveCastingStateTag(UAbilitySystemComponent* SourceASC)
{
	if (SourceASC && bAppliedCastingStateTag)
	{
		SourceASC->RemoveLooseGameplayTag(ArenaGameplayTags::State_Casting);
		SourceASC->RemoveReplicatedLooseGameplayTag(ArenaGameplayTags::State_Casting);
	}
	bAppliedCastingStateTag = false;
}
