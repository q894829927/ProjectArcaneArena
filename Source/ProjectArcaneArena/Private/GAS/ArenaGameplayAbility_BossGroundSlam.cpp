#include "GAS/ArenaGameplayAbility_BossGroundSlam.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/ArenaEnemyCharacter.h"
#include "Character/ArenaPlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GAS/ArenaGameplayEffect_BossGroundSlamCooldown.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffect.h"

// GroundSlam 复用敌人攻击生命周期，并要求任一 Boss.Phase 以覆盖三个阶段。
UArenaGameplayAbility_BossGroundSlam::UArenaGameplayAbility_BossGroundSlam()
{
	SetAssetTags(FGameplayTagContainer(ArenaGameplayTags::Ability_Enemy_Boss_GroundSlam));
	ActivationRequiredTags.AddTag(ArenaGameplayTags::Boss_Phase);
	ActivationBlockedTags.AddTag(ArenaGameplayTags::Cooldown_Enemy_Boss_GroundSlam);
	CooldownGameplayEffectClass = UArenaGameplayEffect_BossGroundSlamCooldown::StaticClass();
}

// 提交成功后才固定圆心并显示预警，配置错误或冷却失败不会留下表现。
void UArenaGameplayAbility_BossGroundSlam::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	AArenaEnemyCharacter* SourceEnemy = ActorInfo ? Cast<AArenaEnemyCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	GroundSlamLocation = ResolveGroundSlamLocation(SourceEnemy);
	bHasGroundSlamLocation = SourceEnemy != nullptr;

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive() || !SourceEnemy || !bHasGroundSlamLocation)
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

	AddTelegraphCue(ActorInfo->AbilitySystemComponent.Get(), SourceEnemy);
}

// 所有结束路径都清除预警状态，随后交还基类移除 State.Attacking 和 AbilityTask。
void UArenaGameplayAbility_BossGroundSlam::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	RemoveTelegraphCue(ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr);
	GroundSlamLocation = FVector::ZeroVector;
	bHasGroundSlamLocation = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// GroundSlam 必须同时拥有 Montage 与 GE_Damage，避免错误资产在 Commit 后空放技能。
bool UArenaGameplayAbility_BossGroundSlam::HasRequiredAttackConfiguration() const
{
	return Super::HasRequiredAttackConfiguration() && DamageEffectClass != nullptr && DamageRadius > 0.0f;
}

// 服务器在固定圆心结算范围伤害；目标移动只影响其是否仍位于半径内。
void UArenaGameplayAbility_BossGroundSlam::ExecuteAttack(
	AArenaEnemyCharacter* SourceEnemy,
	AActor* TargetActor,
	UAbilitySystemComponent* SourceASC,
	UAbilitySystemComponent* TargetASC)
{
	if (!SourceEnemy || !SourceASC || !DamageEffectClass || !bHasGroundSlamLocation || !SourceEnemy->HasAuthority())
	{
		return;
	}

	RemoveTelegraphCue(SourceASC);

	FGameplayCueParameters ImpactCueParameters;
	ImpactCueParameters.Instigator = SourceEnemy;
	ImpactCueParameters.EffectCauser = SourceEnemy;
	ImpactCueParameters.Location = GroundSlamLocation;
	ImpactCueParameters.RawMagnitude = DamageRadius;
	SourceASC->ExecuteGameplayCue(ArenaGameplayTags::GameplayCue_Ability_Boss_GroundSlam_Impact, ImpactCueParameters);

	TArray<FOverlapResult> OverlapResults;
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BossGroundSlam), false, SourceEnemy);
	if (!SourceEnemy->GetWorld()->OverlapMultiByObjectType(
		OverlapResults,
		GroundSlamLocation,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(DamageRadius),
		QueryParams))
	{
		return;
	}

	TSet<AArenaPlayerCharacter*> DamagedPlayers;
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AArenaPlayerCharacter* PlayerCharacter = Cast<AArenaPlayerCharacter>(OverlapResult.GetActor());
		if (!PlayerCharacter || DamagedPlayers.Contains(PlayerCharacter)
			|| FVector::DistSquared2D(PlayerCharacter->GetActorLocation(), GroundSlamLocation) > FMath::Square(DamageRadius))
		{
			continue;
		}

		UAbilitySystemComponent* PlayerASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(PlayerCharacter);
		if (!PlayerASC
			|| PlayerASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead)
			|| PlayerASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Invincible))
		{
			continue;
		}

		FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
		EffectContext.AddSourceObject(this);
		EffectContext.AddInstigator(SourceEnemy, SourceEnemy);
		EffectContext.AddOrigin(GroundSlamLocation);
		FGameplayEffectSpecHandle DamageSpecHandle = SourceASC->MakeOutgoingSpec(
			DamageEffectClass,
			GetAbilityLevel(),
			EffectContext);
		if (!DamageSpecHandle.IsValid())
		{
			continue;
		}

		FGameplayEffectSpec* DamageSpec = DamageSpecHandle.Data.Get();
		DamageSpec->SetSetByCallerMagnitude(ArenaGameplayTags::SetByCaller_Damage_Base, BaseDamage);
		DamageSpec->SetSetByCallerMagnitude(ArenaGameplayTags::SetByCaller_Damage_SkillMultiplier, SkillMultiplier);
		DamageSpec->AddDynamicAssetTag(ArenaGameplayTags::Damage_Physical);
		SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpec, PlayerASC);
		DamagedPlayers.Add(PlayerCharacter);
	}
}

// 胶囊底部比 Actor 原点更接近地面，可让预警和 Impact 在两种视角下保持一致。
FVector UArenaGameplayAbility_BossGroundSlam::ResolveGroundSlamLocation(const AArenaEnemyCharacter* SourceEnemy) const
{
	if (!SourceEnemy)
	{
		return FVector::ZeroVector;
	}

	float CapsuleHalfHeight = 0.0f;
	if (const UCapsuleComponent* CapsuleComponent = SourceEnemy->GetCapsuleComponent())
	{
		CapsuleHalfHeight = CapsuleComponent->GetScaledCapsuleHalfHeight();
	}
	return SourceEnemy->GetActorLocation() - FVector(0.0f, 0.0f, CapsuleHalfHeight);
}

// 预警 Cue 由服务器添加到 Boss ASC，客户端只播放复制到本地的持续表现。
void UArenaGameplayAbility_BossGroundSlam::AddTelegraphCue(UAbilitySystemComponent* SourceASC, AActor* SourceActor)
{
	if (!SourceASC || !SourceActor || bTelegraphActive)
	{
		return;
	}

	FGameplayCueParameters CueParameters;
	CueParameters.Instigator = SourceActor;
	CueParameters.EffectCauser = SourceActor;
	CueParameters.Location = GroundSlamLocation;
	CueParameters.RawMagnitude = DamageRadius;
	SourceASC->AddGameplayCue(ArenaGameplayTags::GameplayCue_Ability_Boss_GroundSlam_Telegraph, CueParameters);
	bTelegraphActive = true;
}

// Cue 移除必须可重复调用，命中、眩晕、死亡和 Montage 中断会共享本路径。
void UArenaGameplayAbility_BossGroundSlam::RemoveTelegraphCue(UAbilitySystemComponent* SourceASC)
{
	if (!bTelegraphActive)
	{
		return;
	}

	if (SourceASC)
	{
		SourceASC->RemoveGameplayCue(ArenaGameplayTags::GameplayCue_Ability_Boss_GroundSlam_Telegraph);
	}
	bTelegraphActive = false;
}
