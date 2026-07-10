#include "Character/ArenaEnemyCharacter.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "GAS/ArenaAbilitySystemComponent.h"
#include "GAS/ArenaAttributeSet.h"
#include "GAS/ArenaGameplayTags.h"
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
}

// 返回敌人自身持有的 ASC，供伤害、标签和 AI 技能系统访问。
UAbilitySystemComponent* AArenaEnemyCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
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
	}

	RefreshHealthBar();
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

	// Health delegate 只驱动 UI 和反馈，真正死亡由 State.Dead 标签统一触发。
	HealthChangedDelegateHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		UArenaAttributeSet::GetHealthAttribute()).AddUObject(this, &AArenaEnemyCharacter::HandleHealthChanged);
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
}

// 监听 State.Dead 标签新增，并把死亡处理集中到 HandleDeath。
void AArenaEnemyCharacter::HandleDeadTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	if (CallbackTag == ArenaGameplayTags::State_Dead && NewCount > 0)
	{
		HandleDeath();
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
