#include "Character/ArenaPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Core/ArenaPlayerState.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GAS/ArenaGameplayAbility.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "GAS/ArenaAbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

AArenaPlayerCharacter::AArenaPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	// 角色当前仍按移动方向旋转；后续鼠标朝向应独立驱动角色朝向。
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	GetCharacterMovement()->MaxWalkSpeed = 600.0f;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 720.0f, 0.0f);

	// 顶视角相机使用绝对旋转，避免角色朝向影响镜头。
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 900.0f;
	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->SetRelativeRotation(FRotator(-60.0f, 0.0f, 0.0f));
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritYaw = false;
	CameraBoom->bInheritRoll = false;
	CameraBoom->bDoCollisionTest = false;

	TopDownCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCamera->bUsePawnControlRotation = false;

	CreateDefaultInputMappings();
}

UAbilitySystemComponent* AArenaPlayerCharacter::GetAbilitySystemComponent() const
{
	const AArenaPlayerState* ArenaPlayerState = GetPlayerState<AArenaPlayerState>();
	return ArenaPlayerState ? ArenaPlayerState->GetAbilitySystemComponent() : nullptr;
}

void AArenaPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	InitializeAbilityActorInfo();
}

void AArenaPlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	InitializeAbilityActorInfo();
}

void AArenaPlayerCharacter::InitializeAbilityActorInfo()
{
	AArenaPlayerState* ArenaPlayerState = GetPlayerState<AArenaPlayerState>();
	if (!ArenaPlayerState)
	{
		return;
	}

	UArenaAbilitySystemComponent* ArenaASC = ArenaPlayerState->GetArenaAbilitySystemComponent();
	if (!ArenaASC)
	{
		return;
	}

	// OwnerActor 是 PlayerState，AvatarActor 是当前 Character，这是玩家 GAS 的多人友好结构。
	ArenaASC->InitAbilityActorInfo(ArenaPlayerState, this);

	if (HasAuthority())
	{
		// 属性和技能只在服务端初始化，客户端通过 GAS 复制和 OnRep 接收结果。
		ApplyDefaultAttributes(ArenaPlayerState, ArenaASC);
		GrantStartupAbilities(ArenaPlayerState, ArenaASC);
	}
}

void AArenaPlayerCharacter::ApplyDefaultAttributes(AArenaPlayerState* ArenaPlayerState, UArenaAbilitySystemComponent* ArenaASC)
{
	if (!ArenaPlayerState || !ArenaASC || ArenaPlayerState->HasAppliedDefaultAttributes() || !DefaultAttributeEffect)
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = ArenaASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	// 默认属性也走 GameplayEffect，避免绕过 AttributeSet/GAS 的统一流程。
	const FGameplayEffectSpecHandle SpecHandle = ArenaASC->MakeOutgoingSpec(DefaultAttributeEffect, 1.0f, EffectContext);
	if (SpecHandle.IsValid())
	{
		ArenaASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		ArenaPlayerState->SetAppliedDefaultAttributes(true);
	}
}

void AArenaPlayerCharacter::GrantStartupAbilities(AArenaPlayerState* ArenaPlayerState, UArenaAbilitySystemComponent* ArenaASC)
{
	if (!ArenaPlayerState || !ArenaASC || ArenaPlayerState->HasGrantedStartupAbilities())
	{
		return;
	}

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : StartupAbilities)
	{
		if (!AbilityClass)
		{
			continue;
		}

		FGameplayAbilitySpec AbilitySpec(AbilityClass, 1, INDEX_NONE, this);
		const UArenaGameplayAbility* ArenaAbilityCDO = Cast<UArenaGameplayAbility>(AbilityClass->GetDefaultObject<UGameplayAbility>());
		if (ArenaAbilityCDO && ArenaAbilityCDO->GetInputTag().IsValid())
		{
			// 输入标签存在 Spec 上，ASC 输入路由时不需要硬编码具体 Ability 类。
			AbilitySpec.GetDynamicSpecSourceTags().AddTag(ArenaAbilityCDO->GetInputTag());
		}

		ArenaASC->GiveAbility(AbilitySpec);
	}

	ArenaPlayerState->SetGrantedStartupAbilities(true);
}

void AArenaPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	AddDefaultMappingContext();

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInputComponent)
	{
		return;
	}

	EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AArenaPlayerCharacter::Input_Move);
	EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &AArenaPlayerCharacter::Input_MoveStopped);
	EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Canceled, this, &AArenaPlayerCharacter::Input_MoveStopped);
	EnhancedInputComponent->BindAction(BasicAttackAction, ETriggerEvent::Started, this, &AArenaPlayerCharacter::Input_BasicAttack);
	EnhancedInputComponent->BindAction(FireballAction, ETriggerEvent::Started, this, &AArenaPlayerCharacter::Input_Fireball);
	EnhancedInputComponent->BindAction(DashAction, ETriggerEvent::Started, this, &AArenaPlayerCharacter::Input_Dash);
	EnhancedInputComponent->BindAction(ShieldAction, ETriggerEvent::Started, this, &AArenaPlayerCharacter::Input_Shield);
	EnhancedInputComponent->BindAction(UltimateAction, ETriggerEvent::Started, this, &AArenaPlayerCharacter::Input_Ultimate);
}

void AArenaPlayerCharacter::AddDefaultMappingContext() const
{
	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		return;
	}

	const ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!InputSubsystem || !DefaultMappingContext)
	{
		return;
	}

	InputSubsystem->AddMappingContext(DefaultMappingContext, InputMappingPriority);
}

void AArenaPlayerCharacter::CreateDefaultInputMappings()
{
	DefaultMappingContext = CreateDefaultSubobject<UInputMappingContext>(TEXT("DefaultMappingContext"));

	MoveAction = CreateDefaultSubobject<UInputAction>(TEXT("Move"));
	MoveAction->ValueType = EInputActionValueType::Axis2D;

	BasicAttackAction = CreateDefaultSubobject<UInputAction>(TEXT("BasicAttack"));
	BasicAttackAction->ValueType = EInputActionValueType::Boolean;

	FireballAction = CreateDefaultSubobject<UInputAction>(TEXT("Fireball"));
	FireballAction->ValueType = EInputActionValueType::Boolean;

	DashAction = CreateDefaultSubobject<UInputAction>(TEXT("Dash"));
	DashAction->ValueType = EInputActionValueType::Boolean;

	ShieldAction = CreateDefaultSubobject<UInputAction>(TEXT("Shield"));
	ShieldAction->ValueType = EInputActionValueType::Boolean;

	UltimateAction = CreateDefaultSubobject<UInputAction>(TEXT("Ultimate"));
	UltimateAction->ValueType = EInputActionValueType::Boolean;

	UInputModifierSwizzleAxis* MoveSwizzle = CreateDefaultSubobject<UInputModifierSwizzleAxis>(TEXT("MoveSwizzle"));
	MoveSwizzle->Order = EInputAxisSwizzle::YXZ;

	UInputModifierNegate* MoveNegate = CreateDefaultSubobject<UInputModifierNegate>(TEXT("MoveNegate"));

	FEnhancedActionKeyMapping& MoveForwardMapping = DefaultMappingContext->MapKey(MoveAction, EKeys::W);
	MoveForwardMapping.Modifiers.Add(MoveSwizzle);

	FEnhancedActionKeyMapping& MoveBackwardMapping = DefaultMappingContext->MapKey(MoveAction, EKeys::S);
	MoveBackwardMapping.Modifiers.Add(MoveNegate);
	MoveBackwardMapping.Modifiers.Add(MoveSwizzle);

	FEnhancedActionKeyMapping& MoveRightMapping = DefaultMappingContext->MapKey(MoveAction, EKeys::D);

	FEnhancedActionKeyMapping& MoveLeftMapping = DefaultMappingContext->MapKey(MoveAction, EKeys::A);
	MoveLeftMapping.Modifiers.Add(MoveNegate);

	DefaultMappingContext->MapKey(BasicAttackAction, EKeys::LeftMouseButton);
	DefaultMappingContext->MapKey(FireballAction, EKeys::Q);
	DefaultMappingContext->MapKey(DashAction, EKeys::E);
	DefaultMappingContext->MapKey(ShieldAction, EKeys::F);
	DefaultMappingContext->MapKey(UltimateAction, EKeys::R);
}

void AArenaPlayerCharacter::Input_AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	// Character 只负责把本地输入转成标签，是否能激活由 ASC/GAS 判断。
	UArenaAbilitySystemComponent* ArenaASC = Cast<UArenaAbilitySystemComponent>(GetAbilitySystemComponent());
	if (!ArenaASC)
	{
		return;
	}

	ArenaASC->AbilityInputTagPressed(InputTag);
}

void AArenaPlayerCharacter::Input_Move(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();

	if (!Controller || MovementVector.IsNearlyZero())
	{
		LastMovementInputDirection = FVector::ZeroVector;
		return;
	}

	const FVector MoveDirection = (FVector::ForwardVector * MovementVector.Y + FVector::RightVector * MovementVector.X).GetSafeNormal();
	LastMovementInputDirection = MoveDirection;

	AddMovementInput(FVector::ForwardVector, MovementVector.Y);
	AddMovementInput(FVector::RightVector, MovementVector.X);
}

void AArenaPlayerCharacter::Input_MoveStopped(const FInputActionValue& Value)
{
	LastMovementInputDirection = FVector::ZeroVector;
}

void AArenaPlayerCharacter::Input_BasicAttack()
{
	Input_AbilityInputTagPressed(ArenaGameplayTags::Ability_BasicAttack);
}

void AArenaPlayerCharacter::Input_Fireball()
{
	Input_AbilityInputTagPressed(ArenaGameplayTags::Ability_Fireball);
}

void AArenaPlayerCharacter::Input_Dash()
{
	Input_AbilityInputTagPressed(ArenaGameplayTags::Ability_Dash);
}

void AArenaPlayerCharacter::Input_Shield()
{
	// TODO: Route to the AbilitySystemComponent input flow in the GAS pass.
}

void AArenaPlayerCharacter::Input_Ultimate()
{
	// TODO: Route to the AbilitySystemComponent input flow in the GAS pass.
}
