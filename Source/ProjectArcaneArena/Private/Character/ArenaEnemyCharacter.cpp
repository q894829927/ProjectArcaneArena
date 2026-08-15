#include "Character/ArenaEnemyCharacter.h"

#include "AI/ArenaEnemyAIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/ArenaHitReactionComponent.h"
#include "Components/ArenaEnemyAffixComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "GAS/ArenaAbilitySystemComponent.h"
#include "GAS/ArenaAttributeSet.h"
#include "GAS/ArenaGameplayTags.h"
#include "GAS/ArenaGameplayAbility_EnemyAttackBase.h"
#include "GAS/ArenaGameplayAbility_EnemyMeleeAttack.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "UI/ArenaDamageNumberActor.h"
#include "UI/ArenaEnemyHealthBarWidget.h"
#include "UI/ArenaEnemyAffixBadgeWidget.h"

// 构造敌人角色，创建敌人专属 ASC、AttributeSet 和头顶血条组件。
AArenaEnemyCharacter::AArenaEnemyCharacter()
{
	// 敌人 ASC 跟随敌人实例，适合短生命周期 AI；复制模式用 Minimal 降低非拥有者开销。
	AbilitySystemComponent = CreateDefaultSubobject<UArenaAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UArenaAttributeSet>(TEXT("AttributeSet"));
	// 敌人属性需要被 ASC 管理，伤害、死亡和血条都从这里读取。
	AbilitySystemComponent->AddAttributeSetSubobject(AttributeSet.Get());

	HealthBarWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarWidget"));
	HealthBarWidgetComponent->SetupAttachment(RootComponent);
	HealthBarWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBarWidgetComponent->SetDrawSize(FVector2D(120.0f, 16.0f));
	HealthBarWidgetComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
	HealthBarWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HealthBarWidgetComponent->SetGenerateOverlapEvents(false);

	EnemyAffixComponent = CreateDefaultSubobject<UArenaEnemyAffixComponent>(TEXT("EnemyAffixComponent"));
	EliteAffixBadgeWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("EliteAffixBadgeWidget"));
	EliteAffixBadgeWidgetComponent->SetupAttachment(RootComponent);
	EliteAffixBadgeWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	EliteAffixBadgeWidgetComponent->SetWidgetClass(UArenaEnemyAffixBadgeWidget::StaticClass());
	EliteAffixBadgeWidgetComponent->SetDrawSize(FVector2D(200.0f, 28.0f));
	EliteAffixBadgeWidgetComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 145.0f));
	EliteAffixBadgeWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EliteAffixBadgeWidgetComponent->SetGenerateOverlapEvents(false);
	EliteAffixBadgeWidgetComponent->SetVisibility(false);

	GetCharacterMovement()->MaxWalkSpeed = 350.0f;
	AIControllerClass = AArenaEnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

// 返回敌人自身持有的 ASC，供伤害、标签和 AI 技能系统访问。
UAbilitySystemComponent* AArenaEnemyCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

// 仅服务器保存 AI 当前目标，客户端不依赖该临时决策状态。
void AArenaEnemyCharacter::SetCombatTarget(AActor* NewCombatTarget)
{
	if (HasAuthority())
	{
		CombatTarget = NewCombatTarget;
	}
}

// 使用 StartupAbilities 中第一个 EnemyAttackBase 子类作为主攻击，激活资格继续交给 GAS 判断。
bool AArenaEnemyCharacter::TryActivatePrimaryAttack()
{
	if (!HasAuthority() || !AbilitySystemComponent || IsDeadOrStunned())
	{
		return false;
	}

	const TSubclassOf<UGameplayAbility> PrimaryAttackClass = FindPrimaryAttackAbilityClass();
	return PrimaryAttackClass
		&& AbilitySystemComponent->TryActivateAbilityByClass(PrimaryAttackClass);
}

// 从主攻击 CDO 读取决策距离，避免 AI 与具体近战或远程 Ability 的数值分叉。
float AArenaEnemyCharacter::GetPrimaryAttackRange() const
{
	const TSubclassOf<UGameplayAbility> PrimaryAttackClass = FindPrimaryAttackAbilityClass();
	const UArenaGameplayAbility_EnemyAttackBase* PrimaryAttackCDO = PrimaryAttackClass
		? Cast<UArenaGameplayAbility_EnemyAttackBase>(PrimaryAttackClass.GetDefaultObject())
		: nullptr;
	return PrimaryAttackCDO ? FMath::Max(PrimaryAttackCDO->GetAttackRange(), 0.0f) : 0.0f;
}

// 通过主攻击 CDO 执行与 Ability 激活相同的视线或弹道检查，供 AI 决定是否停步。
bool AArenaEnemyCharacter::HasPrimaryAttackPath(AActor* TargetActor)
{
	const TSubclassOf<UGameplayAbility> PrimaryAttackClass = FindPrimaryAttackAbilityClass();
	const UArenaGameplayAbility_EnemyAttackBase* PrimaryAttackCDO = PrimaryAttackClass
		? Cast<UArenaGameplayAbility_EnemyAttackBase>(PrimaryAttackClass.GetDefaultObject())
		: nullptr;
	return PrimaryAttackCDO && PrimaryAttackCDO->HasAttackPathForAI(this, TargetActor);
}

// 取消所有正在运行的 EnemyAttackBase Spec，确保多技能 Boss 在死亡、眩晕或目标失效时完整收尾。
void AArenaEnemyCharacter::CancelPrimaryAttack()
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	TArray<FGameplayAbilitySpecHandle> ActiveAttackHandles;
	for (const FGameplayAbilitySpec& AbilitySpec : AbilitySystemComponent->GetActivatableAbilities())
	{
		const UGameplayAbility* AbilityCDO = AbilitySpec.Ability.Get();
		if (AbilitySpec.IsActive()
			&& AbilityCDO
			&& AbilityCDO->GetClass()->IsChildOf(UArenaGameplayAbility_EnemyAttackBase::StaticClass()))
		{
			ActiveAttackHandles.Add(AbilitySpec.Handle);
		}
	}

	for (const FGameplayAbilitySpecHandle& AttackHandle : ActiveAttackHandles)
	{
		AbilitySystemComponent->CancelAbilityHandle(AttackHandle);
	}
}

// 通过 AbilityTag 请求 ASC 激活近战技能，冷却和状态阻断继续由 GAS 判断。
bool AArenaEnemyCharacter::TryActivateMeleeAttack()
{
	if (!HasAuthority() || !AbilitySystemComponent || IsDeadOrStunned())
	{
		return false;
	}

	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(ArenaGameplayTags::Ability_Enemy_MeleeAttack);
	return AbilitySystemComponent->TryActivateAbilitiesByTag(AbilityTags);
}

// 从启动技能 CDO 读取攻击距离，避免 AI 追击距离与 Ability 默认值分叉。
float AArenaEnemyCharacter::GetMeleeAttackRange() const
{
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : StartupAbilities)
	{
		if (const UArenaGameplayAbility_EnemyMeleeAttack* AbilityCDO = Cast<UArenaGameplayAbility_EnemyMeleeAttack>(AbilityClass.GetDefaultObject()))
		{
			return AbilityCDO->GetAttackRange();
		}
	}

	return 170.0f;
}

// 按数组顺序选择首个通用敌人攻击类，使蓝图只需替换 StartupAbility 即可切换战斗类型。
TSubclassOf<UGameplayAbility> AArenaEnemyCharacter::FindPrimaryAttackAbilityClass() const
{
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : StartupAbilities)
	{
		if (AbilityClass && AbilityClass->IsChildOf(UArenaGameplayAbility_EnemyAttackBase::StaticClass()))
		{
			return AbilityClass;
		}
	}

	return nullptr;
}

bool AArenaEnemyCharacter::IsDeadOrStunned() const
{
	return !AbilitySystemComponent
		|| AbilitySystemComponent->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead)
		|| AbilitySystemComponent->HasMatchingGameplayTag(ArenaGameplayTags::State_Stunned);
}

// 攻击状态来自 ASC Tag，AI 和表现层不保存重复布尔状态。
bool AArenaEnemyCharacter::IsAttacking() const
{
	return AbilitySystemComponent
		&& AbilitySystemComponent->HasMatchingGameplayTag(ArenaGameplayTags::State_Attacking);
}

// BeginPlay 固定相机碰撞后，按默认属性、精英强化、词缀行为、启动技能的顺序完成服务端 GAS 初始化。
void AArenaEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 第三人称 SpringArm 只应被世界障碍物压缩，敌人 Capsule 与 Mesh 不参与 Camera 探针碰撞。
	if (UCapsuleComponent* EnemyCapsule = GetCapsuleComponent())
	{
		EnemyCapsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}
	if (USkeletalMeshComponent* EnemyMesh = GetMesh())
	{
		EnemyMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}

	// 初始化顺序先建 ActorInfo，再绑定委托，最后由服务端应用默认属性。
	InitializeAbilityActorInfo();
	BindAbilitySystemDelegates();

	if (HasAuthority())
	{
		ApplyDefaultAttributes();
		const bool bHasEliteConfiguration = EnemyAffixComponent && EnemyAffixComponent->IsElite();
		bEliteInitializationSucceeded = !bHasEliteConfiguration
			|| (bAppliedDefaultAttributes && EnemyAffixComponent->InitializeAfterDefaultAttributes());
		if (bEliteInitializationSucceeded)
		{
			GrantStartupAbilities();
		}
	}

	RefreshHealthBar();
	RefreshEliteAffixPresentation();
}

// 服务器授予敌人配置的 GameplayAbility，客户端通过 ASC 复制获得必要状态。
void AArenaEnemyCharacter::GrantStartupAbilities()
{
	if (bGrantedStartupAbilities || !AbilitySystemComponent)
	{
		return;
	}

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : StartupAbilities)
	{
		if (AbilityClass)
		{
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
		}
	}
	bGrantedStartupAbilities = true;
}

// 销毁前解绑 GAS 委托，避免属性或标签回调访问失效对象。
void AArenaEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindAbilitySystemDelegates();

	Super::EndPlay(EndPlayReason);
}

// 以敌人自身作为 OwnerActor 和 AvatarActor 初始化 ASC。
void AArenaEnemyCharacter::InitializeAbilityActorInfo()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}

// 通过默认 GameplayEffect 初始化敌人属性，保持属性修改走 GAS 流程。
void AArenaEnemyCharacter::ApplyDefaultAttributes()
{
	if (bAppliedDefaultAttributes || !AbilitySystemComponent || !DefaultAttributeEffect)
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	// 敌人初始属性同样通过 GameplayEffect 应用，保持与玩家属性流程一致。
	const FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(DefaultAttributeEffect, 1.0f, EffectContext);
	if (SpecHandle.IsValid())
	{
		AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		bAppliedDefaultAttributes = true;
	}
}

// 绑定死亡标签和 Health 属性变化，用事件驱动死亡、血条和受击反馈。
void AArenaEnemyCharacter::BindAbilitySystemDelegates()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	// 死亡只监听 State.Dead 标签，避免 Health 变化和死亡流程相互抢职责。
	DeadTagDelegateHandle = AbilitySystemComponent->RegisterAndCallGameplayTagEvent(
		ArenaGameplayTags::State_Dead,
		FOnGameplayEffectTagCountChanged::FDelegate::CreateUObject(this, &AArenaEnemyCharacter::HandleDeadTagChanged),
		EGameplayTagEventType::NewOrRemoved);
	StunnedTagDelegateHandle = AbilitySystemComponent->RegisterAndCallGameplayTagEvent(
		ArenaGameplayTags::State_Stunned,
		FOnGameplayEffectTagCountChanged::FDelegate::CreateUObject(this, &AArenaEnemyCharacter::HandleStunnedTagChanged),
		EGameplayTagEventType::NewOrRemoved);

	// Health delegate 只驱动 UI 和反馈，真正死亡由 State.Dead 标签统一触发。
	HealthChangedDelegateHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		UArenaAttributeSet::GetHealthAttribute()).AddUObject(this, &AArenaEnemyCharacter::HandleHealthChanged);
	MoveSpeedDelegateHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		UArenaAttributeSet::GetMoveSpeedAttribute()).AddUObject(this, &AArenaEnemyCharacter::HandleMoveSpeedChanged);
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->MaxWalkSpeed = FMath::Max(AttributeSet ? AttributeSet->GetMoveSpeed() : 0.0f, 0.0f);
	}
}

// 解绑已注册的 GAS 标签和属性委托，配合 EndPlay 做生命周期清理。
void AArenaEnemyCharacter::UnbindAbilitySystemDelegates()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	if (DeadTagDelegateHandle.IsValid())
	{
		AbilitySystemComponent->UnregisterGameplayTagEvent(
			DeadTagDelegateHandle,
			ArenaGameplayTags::State_Dead,
			EGameplayTagEventType::NewOrRemoved);
		DeadTagDelegateHandle.Reset();
	}

	if (HealthChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UArenaAttributeSet::GetHealthAttribute()).Remove(HealthChangedDelegateHandle);
		HealthChangedDelegateHandle.Reset();
	}

	if (StunnedTagDelegateHandle.IsValid())
	{
		AbilitySystemComponent->UnregisterGameplayTagEvent(
			StunnedTagDelegateHandle,
			ArenaGameplayTags::State_Stunned,
			EGameplayTagEventType::NewOrRemoved);
		StunnedTagDelegateHandle.Reset();
	}

	if (MoveSpeedDelegateHandle.IsValid())
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UArenaAttributeSet::GetMoveSpeedAttribute()).Remove(MoveSpeedDelegateHandle);
		MoveSpeedDelegateHandle.Reset();
	}
}

// 监听 State.Dead 标签新增，并把死亡处理集中到 HandleDeath。
void AArenaEnemyCharacter::HandleDeadTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	if (CallbackTag == ArenaGameplayTags::State_Dead && NewCount > 0)
	{
		HandleDeath();
	}
}

// 眩晕期间停止移动和攻击，解除后若未死亡则恢复 Walking。
void AArenaEnemyCharacter::HandleStunnedTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	if (CallbackTag != ArenaGameplayTags::State_Stunned)
	{
		return;
	}

	RefreshMovementState();
	if (NewCount > 0 && AbilitySystemComponent)
	{
		AbilitySystemComponent->CancelAllAbilities();
	}
}

// 将敌人 MoveSpeed Attribute 同步到 CharacterMovement。
void AArenaEnemyCharacter::HandleMoveSpeedChanged(const FOnAttributeChangeData& Data)
{
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->MaxWalkSpeed = FMath::Max(Data.NewValue, 0.0f);
	}
}

// Dead 优先于 Stunned；只有可行动状态才恢复敌人 Walking。
void AArenaEnemyCharacter::RefreshMovementState()
{
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	if (!MovementComponent || !AbilitySystemComponent)
	{
		return;
	}

	if (IsDeadOrStunned())
	{
		MovementComponent->StopMovementImmediately();
		MovementComponent->DisableMovement();
	}
	else if (MovementComponent->MovementMode == MOVE_None)
	{
		MovementComponent->SetMovementMode(MOVE_Walking);
	}
}

// Health 变化只刷新血条和数值通知，完整受击表现统一由权威 DamageFeedback 批次驱动。
void AArenaEnemyCharacter::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	const float MaxHealth = AttributeSet ? AttributeSet->GetMaxHealth() : 0.0f;
	SetHealthBarValues(Data.NewValue, MaxHealth);
	K2_OnHealthChanged(Data.OldValue, Data.NewValue, MaxHealth);
}

// 执行一次性死亡流程：先停战斗和播放死亡表现，再允许 Volatile 延迟最终波次广播。
void AArenaEnemyCharacter::HandleDeath()
{
	if (bDeathHandled)
	{
		return;
	}

	// 死亡可能被标签复制和服务端本地回调多次观察到，必须防重入。
	bDeathHandled = true;

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
		MovementComponent->DisableMovement();
	}

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Capsule->SetGenerateOverlapEvents(false);
	}

	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComponent->SetGenerateOverlapEvents(false);
	}

	SetActorEnableCollision(false);

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->CancelAllAbilities();
	}

	if (HasAuthority())
	{
		// 尸体保留死亡表现，但立刻销毁无主 AIController 并从 Crowd Manager 注销导航代理。
		DetachFromControllerPendingDestroy();
	}

	if (HealthBarWidgetComponent)
	{
		HealthBarWidgetComponent->SetHiddenInGame(true);
		HealthBarWidgetComponent->SetVisibility(false);
	}
	if (EliteAffixBadgeWidgetComponent)
	{
		EliteAffixBadgeWidgetComponent->SetHiddenInGame(true);
		EliteAffixBadgeWidgetComponent->SetVisibility(false);
	}

	K2_OnDeathStarted();
	if (EnemyAffixComponent && EnemyAffixComponent->BeginOwnerDeath())
	{
		return;
	}
	FinalizeDeferredEnemyDeath();
}

// 最终死亡广播与尸体寿命只执行一次，易爆延迟和普通死亡共用本入口。
void AArenaEnemyCharacter::FinalizeDeferredEnemyDeath()
{
	if (bDeathFinalized)
	{
		return;
	}
	bDeathFinalized = true;
	OnEnemyDeath.Broadcast(this);

	if (HasAuthority() && DeathLifeSpan > 0.0f)
	{
		SetLifeSpan(DeathLifeSpan);
	}
}

// Defeat 优先于易爆结算；取消 Timer/Cue 后仍走唯一死亡广播，由 WaveManager 的停止标记抑制奖励。
void AArenaEnemyCharacter::CancelDeferredDeathForDefeat()
{
	if (!HasAuthority() || !bDeathHandled || bDeathFinalized)
	{
		return;
	}
	if (EnemyAffixComponent)
	{
		EnemyAffixComponent->CancelAffixRuntime();
	}
	FinalizeDeferredEnemyDeath();
}

// 从复制词缀数据更新屏幕空间 Badge；死亡或普通敌人始终隐藏。
void AArenaEnemyCharacter::RefreshEliteAffixPresentation()
{
	if (!EliteAffixBadgeWidgetComponent)
	{
		return;
	}
	const UArenaEnemyAffixDataAsset* AffixData = EnemyAffixComponent
		? EnemyAffixComponent->GetActiveAffixData()
		: nullptr;
	const bool bShouldShow = AffixData && !bDeathHandled;
	EliteAffixBadgeWidgetComponent->SetHiddenInGame(!bShouldShow);
	EliteAffixBadgeWidgetComponent->SetVisibility(bShouldShow);
	if (!bShouldShow)
	{
		return;
	}
	EliteAffixBadgeWidgetComponent->InitWidget();
	if (UArenaEnemyAffixBadgeWidget* Badge = Cast<UArenaEnemyAffixBadgeWidget>(
		EliteAffixBadgeWidgetComponent->GetUserWidgetObject()))
	{
		Badge->SetAffixPresentation(AffixData->DisplayName, AffixData->AccentColor);
	}
}

// 使用当前 AttributeSet 数值刷新敌人血条初始显示。
void AArenaEnemyCharacter::RefreshHealthBar()
{
	if (!bShowWorldHealthBar)
	{
		if (HealthBarWidgetComponent)
		{
			HealthBarWidgetComponent->SetHiddenInGame(true);
			HealthBarWidgetComponent->SetVisibility(false);
		}
		return;
	}

	if (!AttributeSet)
	{
		return;
	}

	SetHealthBarValues(AttributeSet->GetHealth(), AttributeSet->GetMaxHealth());
}

// 将 Health/MaxHealth 写入头顶血条 Widget。
void AArenaEnemyCharacter::SetHealthBarValues(float Health, float MaxHealth)
{
	if (!bShowWorldHealthBar || !HealthBarWidgetComponent)
	{
		return;
	}

	HealthBarWidgetComponent->InitWidget();

	UArenaEnemyHealthBarWidget* HealthBarWidget = Cast<UArenaEnemyHealthBarWidget>(
		HealthBarWidgetComponent->GetUserWidgetObject());
	if (!HealthBarWidget)
	{
		return;
	}

	HealthBarWidget->SetHealthValues(Health, MaxHealth);
}

// 旧 Cue 兼容入口委托给公共组件，避免角色类继续维护第二套 SpawnActor 逻辑。
void AArenaEnemyCharacter::SpawnDamageNumber(float DamageAmount, bool bCriticalHit)
{
	if (HitReactionComponent)
	{
		HitReactionComponent->SpawnDamageNumber(
			DamageAmount,
			bCriticalHit,
			EArenaDamageFeedbackType::HealthOnly);
	}
}

// 暴露现有敌人数字 Blueprint Class，公共组件优先复用而不要求立即迁移资产字段。
TSubclassOf<AArenaDamageNumberActor> AArenaEnemyCharacter::GetDamageNumberActorClassForFeedback() const
{
	return DamageNumberActorClass;
}

// 保留敌人原有头顶数字高度，普通玩家仍可使用组件默认值。
FVector AArenaEnemyCharacter::GetDamageNumberSpawnOffsetForFeedback(const FVector& ComponentDefault) const
{
	return DamageNumberSpawnOffset;
}
