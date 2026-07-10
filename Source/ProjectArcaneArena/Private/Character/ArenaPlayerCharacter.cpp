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

// 构造玩家角色，配置顶视角相机、基础移动参数和默认输入资产。
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

// 从 PlayerState 取得玩家 ASC，保持角色重生时 GAS 状态不丢失。
UAbilitySystemComponent* AArenaPlayerCharacter::GetAbilitySystemComponent() const
{
	const AArenaPlayerState* ArenaPlayerState = GetPlayerState<AArenaPlayerState>();
	return ArenaPlayerState ? ArenaPlayerState->GetAbilitySystemComponent() : nullptr;
}

// 服务端 Possess 后初始化 GAS Avatar，并授予服务器权威的默认数据。
void AArenaPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	InitializeAbilityActorInfo();
}

// 客户端收到 PlayerState 后重新绑定 ASC 到当前角色实例。
void AArenaPlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	InitializeAbilityActorInfo();
}

// 统一初始化玩家 ASC 的 Owner/Avatar，并在服务端补齐默认属性和技能。
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

// 应用玩家初始属性 GameplayEffect，避免绕过 GAS 直接改属性。
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

// 授予玩家初始技能，并把输入标签写入 AbilitySpec 供 ASC 路由。
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

// 绑定 Enhanced Input，把按键输入转交给移动或 GAS 输入标签流程。
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

// 将默认 MappingContext 添加到本地玩家输入子系统。
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

// 创建 C++ 默认输入映射，便于早期原型不依赖外部输入资产。
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

// 将本地技能输入转换为 GameplayTag，让 ASC 决定能否激活技能。
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

// 处理 WASD 移动，并缓存移动方向供 Dash 等技能读取。
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

// 移动输入结束时清空缓存方向，避免后续技能使用过期方向。
void AArenaPlayerCharacter::Input_MoveStopped(const FInputActionValue& Value)
{
	LastMovementInputDirection = FVector::ZeroVector;
}

// 基础攻击输入入口，仅发送 Ability.BasicAttack 标签。
void AArenaPlayerCharacter::Input_BasicAttack()
{
	Input_AbilityInputTagPressed(ArenaGameplayTags::Ability_BasicAttack);
}

// 火球技能输入入口，仅发送 Ability.Fireball 标签。
void AArenaPlayerCharacter::Input_Fireball()
{
	Input_AbilityInputTagPressed(ArenaGameplayTags::Ability_Fireball);
}

// 冲刺技能输入入口，仅发送 Ability.Dash 标签。
void AArenaPlayerCharacter::Input_Dash()
{
	Input_AbilityInputTagPressed(ArenaGameplayTags::Ability_Dash);
}

// 护盾技能输入入口，仅发送 Ability.Shield 标签。
void AArenaPlayerCharacter::Input_Shield()
{
	Input_AbilityInputTagPressed(ArenaGameplayTags::Ability_Shield);
}

// 终极技能预留入口，后续接入 LightningStorm 或其他 GameplayAbility。
void AArenaPlayerCharacter::Input_Ultimate()
{
	// TODO: 在 GAS 技能补齐后路由到对应输入标签。
}
