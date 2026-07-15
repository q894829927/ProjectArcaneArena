#include "Character/ArenaEnemyCharacter.h"

#include "AI/ArenaEnemyAIController.h"
#include "Components/CapsuleComponent.h"
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

// 只取消配置的主攻击 Spec，不影响敌人未来可能拥有的被动或其他辅助 Ability。
void AArenaEnemyCharacter::CancelPrimaryAttack()
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	const TSubclassOf<UGameplayAbility> PrimaryAttackClass = FindPrimaryAttackAbilityClass();
	FGameplayAbilitySpec* PrimaryAttackSpec = PrimaryAttackClass
		? AbilitySystemComponent->FindAbilitySpecFromClass(PrimaryAttackClass)
		: nullptr;
	if (PrimaryAttackSpec && PrimaryAttackSpec->IsActive())
	{
		AbilitySystemComponent->CancelAbilityHandle(PrimaryAttackSpec->Handle);
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

// BeginPlay 阶段初始化敌人 GAS、绑定反馈委托，并由服务端应用默认属性。
void AArenaEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 初始化顺序先建 ActorInfo，再绑定委托，最后由服务端应用默认属性。
	InitializeAbilityActorInfo();
	BindAbilitySystemDelegates();

	if (HasAuthority())
	{
		ApplyDefaultAttributes();
		GrantStartupAbilities();
	}

	RefreshHealthBar();
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

// 响应 Health 变化，刷新血条并触发本地受击表现。
void AArenaEnemyCharacter::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	const float MaxHealth = AttributeSet ? AttributeSet->GetMaxHealth() : 0.0f;
	SetHealthBarValues(Data.NewValue, MaxHealth);
	K2_OnHealthChanged(Data.OldValue, Data.NewValue, MaxHealth);

	const float DamageAmount = FMath::Max(Data.OldValue - Data.NewValue, 0.0f);
	if (DamageAmount > 0.0f)
	{
		SpawnDamageNumber(DamageAmount);
		K2_OnDamaged(DamageAmount, Data.NewValue, MaxHealth);
	}
}

// 执行一次性死亡流程：停移动、关碰撞、取消技能、广播死亡事件。
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

	if (HealthBarWidgetComponent)
	{
		HealthBarWidgetComponent->SetHiddenInGame(true);
		HealthBarWidgetComponent->SetVisibility(false);
	}

	// 广播给后续 WaveManager/GameMode 使用，蓝图事件只负责表现层。
	OnEnemyDeath.Broadcast(this);
	K2_OnDeathStarted();

	if (HasAuthority() && DeathLifeSpan > 0.0f)
	{
		SetLifeSpan(DeathLifeSpan);
	}
}

// 使用当前 AttributeSet 数值刷新敌人血条初始显示。
void AArenaEnemyCharacter::RefreshHealthBar()
{
	if (!AttributeSet)
	{
		return;
	}

	SetHealthBarValues(AttributeSet->GetHealth(), AttributeSet->GetMaxHealth());
}

// 将 Health/MaxHealth 写入头顶血条 Widget。
void AArenaEnemyCharacter::SetHealthBarValues(float Health, float MaxHealth)
{
	if (!HealthBarWidgetComponent)
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

// 生成本地伤害数字表现，不参与复制或权威伤害结算。
void AArenaEnemyCharacter::SpawnDamageNumber(float DamageAmount)
{
	if (DamageAmount <= 0.0f || !DamageNumberActorClass || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.Instigator = GetInstigator();
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// 伤害数字是本地表现 Actor，不复制也不参与任何伤害结算。
	AArenaDamageNumberActor* DamageNumberActor = World->SpawnActor<AArenaDamageNumberActor>(
		DamageNumberActorClass,
		GetActorLocation() + DamageNumberSpawnOffset,
		FRotator::ZeroRotator,
		SpawnParameters);

	if (DamageNumberActor)
	{
		DamageNumberActor->SetDamageAmount(DamageAmount);
	}
}
