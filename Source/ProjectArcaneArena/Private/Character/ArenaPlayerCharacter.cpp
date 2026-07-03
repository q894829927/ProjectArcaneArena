#include "Character/ArenaPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

AArenaPlayerCharacter::AArenaPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;

	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	GetCharacterMovement()->MaxWalkSpeed = 600.0f;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 720.0f, 0.0f);

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

void AArenaPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	FaceMouseCursor();
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

void AArenaPlayerCharacter::FaceMouseCursor()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		return;
	}

	FVector AimPoint;
	if (!GetMouseAimPointOnPlane(*PlayerController, AimPoint))
	{
		return;
	}

	FVector FacingDirection = AimPoint - GetActorLocation();
	FacingDirection.Z = 0.0f;

	if (FacingDirection.IsNearlyZero())
	{
		return;
	}

	const FRotator TargetRotation = FRotator(0.0f, FacingDirection.Rotation().Yaw, 0.0f);
	ApplyFacingRotation(TargetRotation);

	const float CurrentTime = GetWorld()->GetTimeSeconds();
	if (!HasAuthority()
		&& CurrentTime - LastFacingReplicationTime >= FacingReplicationMinInterval
		&& FMath::Abs(FMath::FindDeltaAngleDegrees(LastSentFacingYaw, TargetRotation.Yaw)) > FacingReplicationYawTolerance)
	{
		LastSentFacingYaw = TargetRotation.Yaw;
		LastFacingReplicationTime = CurrentTime;
		Server_SetFacingRotation(TargetRotation);
	}
}

bool AArenaPlayerCharacter::GetMouseAimPointOnPlane(const APlayerController& PlayerController, FVector& OutAimPoint) const
{
	FVector WorldOrigin;
	FVector WorldDirection;
	if (!PlayerController.DeprojectMousePositionToWorld(WorldOrigin, WorldDirection))
	{
		return false;
	}

	if (FMath::IsNearlyZero(WorldDirection.Z))
	{
		return false;
	}

	const float DistanceToAimPlane = (AimPlaneZ - WorldOrigin.Z) / WorldDirection.Z;
	if (DistanceToAimPlane < 0.0f)
	{
		return false;
	}

	OutAimPoint = WorldOrigin + WorldDirection * DistanceToAimPlane;
	return true;
}

void AArenaPlayerCharacter::ApplyFacingRotation(const FRotator& NewRotation)
{
	SetActorRotation(NewRotation);
}

void AArenaPlayerCharacter::Input_Move(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();

	if (!Controller || MovementVector.IsNearlyZero())
	{
		return;
	}

	AddMovementInput(FVector::ForwardVector, MovementVector.Y);
	AddMovementInput(FVector::RightVector, MovementVector.X);
}

void AArenaPlayerCharacter::Input_BasicAttack()
{
	// TODO: Route to the AbilitySystemComponent input flow in the GAS pass.
}

void AArenaPlayerCharacter::Input_Fireball()
{
	// TODO: Route to the AbilitySystemComponent input flow in the GAS pass.
}

void AArenaPlayerCharacter::Input_Dash()
{
	// TODO: Route to the AbilitySystemComponent input flow in the GAS pass.
}

void AArenaPlayerCharacter::Input_Shield()
{
	// TODO: Route to the AbilitySystemComponent input flow in the GAS pass.
}

void AArenaPlayerCharacter::Input_Ultimate()
{
	// TODO: Route to the AbilitySystemComponent input flow in the GAS pass.
}

void AArenaPlayerCharacter::Server_SetFacingRotation_Implementation(FRotator NewRotation)
{
	NewRotation.Pitch = 0.0f;
	NewRotation.Roll = 0.0f;
	ApplyFacingRotation(NewRotation);
}
