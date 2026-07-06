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

AArenaEnemyCharacter::AArenaEnemyCharacter()
{
	AbilitySystemComponent = CreateDefaultSubobject<UArenaAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UArenaAttributeSet>(TEXT("AttributeSet"));
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

UAbilitySystemComponent* AArenaEnemyCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AArenaEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	InitializeAbilityActorInfo();
	BindAbilitySystemDelegates();

	if (HasAuthority())
	{
		ApplyDefaultAttributes();
	}

	RefreshHealthBar();
}

void AArenaEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindAbilitySystemDelegates();

	Super::EndPlay(EndPlayReason);
}

void AArenaEnemyCharacter::InitializeAbilityActorInfo()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}

void AArenaEnemyCharacter::ApplyDefaultAttributes()
{
	if (bAppliedDefaultAttributes || !AbilitySystemComponent || !DefaultAttributeEffect)
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	const FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(DefaultAttributeEffect, 1.0f, EffectContext);
	if (SpecHandle.IsValid())
	{
		AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		bAppliedDefaultAttributes = true;
	}
}

void AArenaEnemyCharacter::BindAbilitySystemDelegates()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	DeadTagDelegateHandle = AbilitySystemComponent->RegisterAndCallGameplayTagEvent(
		ArenaGameplayTags::State_Dead,
		FOnGameplayEffectTagCountChanged::FDelegate::CreateUObject(this, &AArenaEnemyCharacter::HandleDeadTagChanged),
		EGameplayTagEventType::NewOrRemoved);

	HealthChangedDelegateHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		UArenaAttributeSet::GetHealthAttribute()).AddUObject(this, &AArenaEnemyCharacter::HandleHealthChanged);
}

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

void AArenaEnemyCharacter::HandleDeadTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	if (CallbackTag == ArenaGameplayTags::State_Dead && NewCount > 0)
	{
		HandleDeath();
	}
}

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

void AArenaEnemyCharacter::HandleDeath()
{
	if (bDeathHandled)
	{
		return;
	}

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

	OnEnemyDeath.Broadcast(this);
	K2_OnDeathStarted();

	if (HasAuthority() && DeathLifeSpan > 0.0f)
	{
		SetLifeSpan(DeathLifeSpan);
	}
}

void AArenaEnemyCharacter::RefreshHealthBar()
{
	if (!AttributeSet)
	{
		return;
	}

	SetHealthBarValues(AttributeSet->GetHealth(), AttributeSet->GetMaxHealth());
}

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
