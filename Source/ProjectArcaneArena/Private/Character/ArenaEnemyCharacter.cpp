#include "Character/ArenaEnemyCharacter.h"

#include "GAS/ArenaAbilitySystemComponent.h"
#include "GAS/ArenaAttributeSet.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"

AArenaEnemyCharacter::AArenaEnemyCharacter()
{
	AbilitySystemComponent = CreateDefaultSubobject<UArenaAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UArenaAttributeSet>(TEXT("AttributeSet"));
	AbilitySystemComponent->AddAttributeSetSubobject(AttributeSet.Get());

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

	if (HasAuthority())
	{
		ApplyDefaultAttributes();
	}
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
