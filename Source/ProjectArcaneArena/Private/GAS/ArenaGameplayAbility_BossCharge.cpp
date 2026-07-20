#include "GAS/ArenaGameplayAbility_BossCharge.h"

#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/ArenaEnemyCharacter.h"
#include "Character/ArenaPlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Core/ArenaGameState.h"
#include "Engine/HitResult.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/RootMotionSource.h"
#include "GAS/ArenaGameplayEffect_BossChargeCooldown.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffect.h"
#include "TimerManager.h"

namespace
{
	// Charge 运行期间持续确认仍处于战斗阶段，终局或升级阶段不允许残留位移和伤害。
	bool IsChargeCombatPhaseActive(const UWorld* World)
	{
		const AArenaGameState* ArenaGameState = World ? World->GetGameState<AArenaGameState>() : nullptr;
		return ArenaGameState && ArenaGameState->GetGamePhase() == EArenaGamePhase::Combat;
	}
}

// Charge 由服务器执行并使用独立冷却，BT 通过精确 Ability AssetTag 激活它。
UArenaGameplayAbility_BossCharge::UArenaGameplayAbility_BossCharge()
{
	SetAssetTags(FGameplayTagContainer(ArenaGameplayTags::Ability_Enemy_Boss_Charge));
	ActivationBlockedTags.AddTag(ArenaGameplayTags::Cooldown_Enemy_Boss_Charge);
	CooldownGameplayEffectClass = UArenaGameplayEffect_BossChargeCooldown::StaticClass();
}

// 通用入口负责校验和 Commit；Charge 随后固定世界方向，整个前摇不再追踪移动目标。
void UArenaGameplayAbility_BossCharge::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	const AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!AvatarActor || !IsChargeCombatPhaseActive(AvatarActor->GetWorld()))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

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

	LockedStartLocation = SourceEnemy->GetActorLocation();
	FVector LockedTargetLocation = TargetActor->GetActorLocation();
	LockedTargetLocation.Z = LockedStartLocation.Z;
	LockedDirection = (LockedTargetLocation - LockedStartLocation).GetSafeNormal2D();
	if (LockedDirection.IsNearlyZero())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const float TargetDistance = FVector::Dist2D(LockedStartLocation, LockedTargetLocation);
	LockedChargeDistance = FMath::Clamp(
		TargetDistance + FMath::Max(TargetOvershootDistance, 0.0f),
		1.0f,
		FMath::Max(MaximumChargeDistance, 1.0f));
	LockedEndLocation = LockedStartLocation + LockedDirection * LockedChargeDistance;
	ChargeSourceEnemy = SourceEnemy;
	LockedTargetActor = TargetActor;
	ChargeSourceASC = SourceASC;
	PreviousSweepLocation = LockedStartLocation;
	HitPlayers.Reset();
	bWallImpactExecuted = false;

	if (UCharacterMovementComponent* MovementComponent = SourceEnemy->GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
	}
	if (AController* Controller = SourceEnemy->GetController())
	{
		Controller->StopMovement();
	}
	SourceEnemy->SetActorRotation(LockedDirection.Rotation());
	AddTelegraphCue();

	if (TelegraphDuration <= KINDA_SMALL_NUMBER)
	{
		BeginChargeMovement();
	}
	else if (UWorld* World = SourceEnemy->GetWorld())
	{
		World->GetTimerManager().SetTimer(
			TelegraphTimerHandle,
			this,
			&UArenaGameplayAbility_BossCharge::BeginChargeMovement,
			TelegraphDuration,
			false);
	}
}

// EndAbility 是唯一清理出口；所有正常完成、撞墙和取消路径均允许重复调用这些清理函数。
void UArenaGameplayAbility_BossCharge::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TelegraphTimerHandle);
		World->GetTimerManager().ClearTimer(SweepTimerHandle);
	}

	RemoveTelegraphCue();
	RemoveActiveCue();
	RestorePawnCollision();
	StopChargeMovement();
	bChargeMovementActive = false;
	HitPlayers.Reset();
	ChargeMovementTask = nullptr;
	ChargeMontageTask = nullptr;
	ChargeSourceEnemy.Reset();
	LockedTargetActor.Reset();
	ChargeSourceASC.Reset();
	LockedStartLocation = FVector::ZeroVector;
	LockedDirection = FVector::ZeroVector;
	LockedEndLocation = FVector::ZeroVector;
	PreviousSweepLocation = FVector::ZeroVector;
	LockedChargeDistance = 0.0f;
	bWallImpactExecuted = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// 防止缺少动画、伤害或位移数值的蓝图消耗冷却后进入无法结束的攻击状态。
bool UArenaGameplayAbility_BossCharge::HasRequiredAttackConfiguration() const
{
	return Super::HasRequiredAttackConfiguration()
		&& DamageEffectClass
		&& AttackRange > 0.0f
		&& MaximumChargeDistance > 0.0f
		&& ChargeSpeed > 0.0f
		&& HitRadius > 0.0f;
}

// 前摇只要求锁定目标仍存活；其后的位置、距离和视线变化不会改变已经承诺的冲锋直线。
void UArenaGameplayAbility_BossCharge::BeginChargeMovement()
{
	AArenaEnemyCharacter* SourceEnemy = ChargeSourceEnemy.Get();
	AActor* TargetActor = LockedTargetActor.Get();
	UAbilitySystemComponent* TargetASC = TargetActor
		? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor)
		: nullptr;
	if (!IsActive() || !SourceEnemy || SourceEnemy->IsDeadOrStunned()
		|| !IsChargeCombatPhaseActive(SourceEnemy->GetWorld())
		|| !TargetASC
		|| TargetASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead))
	{
		FinishCurrentAttack(true);
		return;
	}

	RemoveTelegraphCue();
	SourceEnemy->SetActorRotation(LockedDirection.Rotation());
	PreviousSweepLocation = SourceEnemy->GetActorLocation();
	EnablePawnPassThrough();
	AddActiveCue();
	bChargeMovementActive = true;

	ChargeMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		FName(TEXT("BossChargeMontage")),
		ChargeMontage,
		FMath::Max(ChargeMontagePlayRate, 0.01f),
		MontageStartSection,
		true,
		0.0f);
	if (!ChargeMontageTask)
	{
		FinishCurrentAttack(true);
		return;
	}
	ChargeMontageTask->OnInterrupted.AddDynamic(
		this,
		&UArenaGameplayAbility_BossCharge::HandleChargeMontageInterrupted);
	ChargeMontageTask->OnCancelled.AddDynamic(
		this,
		&UArenaGameplayAbility_BossCharge::HandleChargeMontageInterrupted);
	ChargeMontageTask->ReadyForActivation();
	if (!IsActive())
	{
		return;
	}

	const float RemainingChargeDistance = FMath::Clamp(
		FVector::DotProduct(
			LockedEndLocation - SourceEnemy->GetActorLocation(),
			LockedDirection),
		0.0f,
		FMath::Max(MaximumChargeDistance, 1.0f));
	if (RemainingChargeDistance <= KINDA_SMALL_NUMBER)
	{
		FinishCurrentAttack(false);
		return;
	}
	const float ChargeDuration = RemainingChargeDistance / FMath::Max(ChargeSpeed, 1.0f);
	ChargeMovementTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this,
		FName(TEXT("BossChargeMovement")),
		LockedDirection,
		ChargeSpeed,
		ChargeDuration,
		false,
		nullptr,
		ERootMotionFinishVelocityMode::SetVelocity,
		FVector::ZeroVector,
		0.0f,
		false);
	if (!ChargeMovementTask)
	{
		FinishCurrentAttack(true);
		return;
	}
	ChargeMovementTask->OnFinish.AddDynamic(
		this,
		&UArenaGameplayAbility_BossCharge::HandleChargeMovementFinished);
	ChargeMovementTask->ReadyForActivation();

	if (UWorld* World = SourceEnemy->GetWorld())
	{
		World->GetTimerManager().SetTimer(
			SweepTimerHandle,
			this,
			&UArenaGameplayAbility_BossCharge::SweepChargePath,
			0.02f,
			true);
	}
}

// RootMotion 完成前补做最后一段 Sweep，随后按正常完成结束且不产生终点 Impact。
void UArenaGameplayAbility_BossCharge::HandleChargeMovementFinished()
{
	if (!IsActive())
	{
		return;
	}
	SweepChargePath();
	FinishCurrentAttack(false);
}

// 动画异常终止会取消仍在运行的 RootMotion 和伤害 Sweep，不留下迟到命中。
void UArenaGameplayAbility_BossCharge::HandleChargeMontageInterrupted()
{
	FinishCurrentAttack(true);
}

// Pawn 已临时改为 Overlap；可行走斜坡只负责抬升角色，真正的墙体或不可行走表面才终止 Charge。
void UArenaGameplayAbility_BossCharge::HandleChargeBlockingHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	AArenaEnemyCharacter* SourceEnemy = ChargeSourceEnemy.Get();
	if (!IsActive() || !bChargeMovementActive || bWallImpactExecuted
		|| !SourceEnemy || !Hit.bBlockingHit || OtherActor == SourceEnemy
		|| Cast<AArenaPlayerCharacter>(OtherActor))
	{
		return;
	}
	if (const UCharacterMovementComponent* MovementComponent = SourceEnemy->GetCharacterMovement();
		MovementComponent && MovementComponent->IsWalkable(Hit))
	{
		return;
	}

	bWallImpactExecuted = true;
	const FVector ImpactLocation = Hit.ImpactPoint.IsNearlyZero()
		? SourceEnemy->GetActorLocation()
		: FVector(Hit.ImpactPoint);
	ExecuteImpactCue(ImpactLocation, FVector(Hit.ImpactNormal));
	FinishCurrentAttack(false);
}

// 高频短线段 Sweep 覆盖高速移动的帧间空隙，并持续检查 Stun、死亡和终局取消条件。
void UArenaGameplayAbility_BossCharge::SweepChargePath()
{
	AArenaEnemyCharacter* SourceEnemy = ChargeSourceEnemy.Get();
	if (!IsActive() || !bChargeMovementActive || !SourceEnemy)
	{
		return;
	}
	if (SourceEnemy->IsDeadOrStunned() || !IsChargeCombatPhaseActive(SourceEnemy->GetWorld()))
	{
		FinishCurrentAttack(true);
		return;
	}

	const FVector CurrentLocation = SourceEnemy->GetActorLocation();
	TArray<FHitResult> HitResults;
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BossChargePlayers), false, SourceEnemy);
	SourceEnemy->GetWorld()->SweepMultiByObjectType(
		HitResults,
		PreviousSweepLocation,
		CurrentLocation,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(HitRadius),
		QueryParams);

	for (const FHitResult& HitResult : HitResults)
	{
		AArenaPlayerCharacter* PlayerCharacter = Cast<AArenaPlayerCharacter>(HitResult.GetActor());
		const TWeakObjectPtr<AArenaPlayerCharacter> PlayerKey(PlayerCharacter);
		if (!PlayerCharacter || HitPlayers.Contains(PlayerKey))
		{
			continue;
		}

		UAbilitySystemComponent* PlayerASC =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(PlayerCharacter);
		if (!PlayerASC || PlayerASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead))
		{
			continue;
		}

		HitPlayers.Add(PlayerKey);
		const FVector ImpactLocation = HitResult.ImpactPoint.IsNearlyZero()
			? PlayerCharacter->GetActorLocation()
			: FVector(HitResult.ImpactPoint);
		ApplyChargeDamage(PlayerCharacter, ImpactLocation);
	}

	PreviousSweepLocation = CurrentLocation;
}

// 每个玩家独立创建 GE_Damage Spec；无敌、护盾、Defense 和 Crit 继续由现有权威伤害管线处理。
void UArenaGameplayAbility_BossCharge::ApplyChargeDamage(
	AArenaPlayerCharacter* PlayerCharacter,
	const FVector& ImpactLocation)
{
	AArenaEnemyCharacter* SourceEnemy = ChargeSourceEnemy.Get();
	UAbilitySystemComponent* SourceASC = ChargeSourceASC.Get();
	UAbilitySystemComponent* TargetASC = PlayerCharacter
		? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(PlayerCharacter)
		: nullptr;
	if (!SourceEnemy || !SourceASC || !TargetASC || !DamageEffectClass)
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);
	EffectContext.AddInstigator(SourceEnemy, SourceEnemy);
	EffectContext.AddOrigin(ImpactLocation);
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
	DamageSpec->SetSetByCallerMagnitude(
		ArenaGameplayTags::SetByCaller_Damage_SkillMultiplier,
		SkillMultiplier);
	DamageSpec->AddDynamicAssetTag(ArenaGameplayTags::Damage_Physical);
	SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpec, TargetASC);
	ExecuteImpactCue(ImpactLocation, -LockedDirection);
}

// Telegraph 使用 Boss 胶囊底部作为世界起点，Normal 和 RawMagnitude 分别传递锁定方向与长度。
void UArenaGameplayAbility_BossCharge::AddTelegraphCue()
{
	AArenaEnemyCharacter* SourceEnemy = ChargeSourceEnemy.Get();
	UAbilitySystemComponent* SourceASC = ChargeSourceASC.Get();
	if (!SourceEnemy || !SourceASC || bTelegraphCueActive)
	{
		return;
	}

	FVector TelegraphLocation = LockedStartLocation;
	if (const UCapsuleComponent* CapsuleComponent = SourceEnemy->GetCapsuleComponent())
	{
		TelegraphLocation.Z -= CapsuleComponent->GetScaledCapsuleHalfHeight();
	}
	FGameplayCueParameters CueParameters;
	CueParameters.Instigator = SourceEnemy;
	CueParameters.EffectCauser = SourceEnemy;
	CueParameters.Location = TelegraphLocation;
	CueParameters.Normal = LockedDirection;
	CueParameters.RawMagnitude = LockedChargeDistance;
	SourceASC->AddGameplayCue(ArenaGameplayTags::GameplayCue_Ability_Boss_Charge_Telegraph, CueParameters);
	bTelegraphCueActive = true;
}

// 预警移除可由正常兑现和任意取消路径重复调用。
void UArenaGameplayAbility_BossCharge::RemoveTelegraphCue()
{
	if (bTelegraphCueActive)
	{
		if (UAbilitySystemComponent* SourceASC = ChargeSourceASC.Get())
		{
			SourceASC->RemoveGameplayCue(ArenaGameplayTags::GameplayCue_Ability_Boss_Charge_Telegraph);
		}
		bTelegraphCueActive = false;
	}
}

// Active Cue 只在真实位移开始后添加，并以 Boss 作为表现目标供蓝图附着。
void UArenaGameplayAbility_BossCharge::AddActiveCue()
{
	AArenaEnemyCharacter* SourceEnemy = ChargeSourceEnemy.Get();
	UAbilitySystemComponent* SourceASC = ChargeSourceASC.Get();
	if (!SourceEnemy || !SourceASC || bActiveCueActive)
	{
		return;
	}

	FGameplayCueParameters CueParameters;
	CueParameters.Instigator = SourceEnemy;
	CueParameters.EffectCauser = SourceEnemy;
	CueParameters.Location = SourceEnemy->GetActorLocation();
	CueParameters.Normal = LockedDirection;
	SourceASC->AddGameplayCue(ArenaGameplayTags::GameplayCue_Ability_Boss_Charge_Active, CueParameters);
	bActiveCueActive = true;
}

// Active Cue 与 RootMotion 同生命周期，正常结束和取消都从此处成对移除。
void UArenaGameplayAbility_BossCharge::RemoveActiveCue()
{
	if (bActiveCueActive)
	{
		if (UAbilitySystemComponent* SourceASC = ChargeSourceASC.Get())
		{
			SourceASC->RemoveGameplayCue(ArenaGameplayTags::GameplayCue_Ability_Boss_Charge_Active);
		}
		bActiveCueActive = false;
	}
}

// Impact 是服务器确认的一次性表现，玩家接触和墙体碰撞均携带准确世界位置与法线。
void UArenaGameplayAbility_BossCharge::ExecuteImpactCue(
	const FVector& ImpactLocation,
	const FVector& ImpactNormal)
{
	AArenaEnemyCharacter* SourceEnemy = ChargeSourceEnemy.Get();
	UAbilitySystemComponent* SourceASC = ChargeSourceASC.Get();
	if (!SourceEnemy || !SourceASC)
	{
		return;
	}

	FGameplayCueParameters CueParameters;
	CueParameters.Instigator = SourceEnemy;
	CueParameters.EffectCauser = SourceEnemy;
	CueParameters.Location = ImpactLocation;
	CueParameters.Normal = ImpactNormal.GetSafeNormal();
	SourceASC->ExecuteGameplayCue(ArenaGameplayTags::GameplayCue_Ability_Boss_Charge_Impact, CueParameters);
}

// 玩家胶囊变为 Overlap 后仍能被 Sweep 检出，但不会阻止 Boss 继续冲向锁定终点。
void UArenaGameplayAbility_BossCharge::EnablePawnPassThrough()
{
	AArenaEnemyCharacter* SourceEnemy = ChargeSourceEnemy.Get();
	UCapsuleComponent* CapsuleComponent = SourceEnemy ? SourceEnemy->GetCapsuleComponent() : nullptr;
	if (!CapsuleComponent || bPawnCollisionChanged)
	{
		return;
	}

	PreviousPawnCollisionResponse = CapsuleComponent->GetCollisionResponseToChannel(ECC_Pawn);
	CapsuleComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CapsuleComponent->OnComponentHit.AddDynamic(
		this,
		&UArenaGameplayAbility_BossCharge::HandleChargeBlockingHit);
	bPawnCollisionChanged = true;
}

// 解绑前先恢复原始响应，避免技能取消后 Boss 永久穿过普通 Pawn。
void UArenaGameplayAbility_BossCharge::RestorePawnCollision()
{
	AArenaEnemyCharacter* SourceEnemy = ChargeSourceEnemy.Get();
	UCapsuleComponent* CapsuleComponent = SourceEnemy ? SourceEnemy->GetCapsuleComponent() : nullptr;
	if (!CapsuleComponent || !bPawnCollisionChanged)
	{
		bPawnCollisionChanged = false;
		return;
	}

	CapsuleComponent->OnComponentHit.RemoveDynamic(
		this,
		&UArenaGameplayAbility_BossCharge::HandleChargeBlockingHit);
	CapsuleComponent->SetCollisionResponseToChannel(ECC_Pawn, PreviousPawnCollisionResponse);
	bPawnCollisionChanged = false;
}

// 清零 CharacterMovement 速度，RootMotion Task 会随 Ability 结束销毁并移除对应 RootMotionSource。
void UArenaGameplayAbility_BossCharge::StopChargeMovement()
{
	if (AArenaEnemyCharacter* SourceEnemy = ChargeSourceEnemy.Get())
	{
		if (UCharacterMovementComponent* MovementComponent = SourceEnemy->GetCharacterMovement())
		{
			MovementComponent->StopMovementImmediately();
		}
		if (AController* Controller = SourceEnemy->GetController())
		{
			Controller->StopMovement();
		}
	}
}
