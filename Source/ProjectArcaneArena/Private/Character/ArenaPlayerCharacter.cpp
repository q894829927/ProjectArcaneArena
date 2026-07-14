#include "Character/ArenaPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Core/ArenaGameMode.h"
#include "Core/ArenaPlayerController.h"
#include "Core/ArenaPlayerState.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GAS/ArenaGameplayAbility.h"
#include "GAS/ArenaAttributeSet.h"
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
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

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
	CameraBoom->TargetArmLength = TopDownArmLength;
	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->SetRelativeRotation(TopDownCameraRotation);
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

// 在切换过程中平滑推进视角混合值，到达目标后停止不必要的 Tick。
void AArenaPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float TargetAlpha = bThirdPersonView ? 1.0f : 0.0f;
	const float BlendSpeed = CameraTransitionDuration > KINDA_SMALL_NUMBER ? 1.0f / CameraTransitionDuration : BIG_NUMBER;
	CameraBlendAlpha = FMath::FInterpConstantTo(CameraBlendAlpha, TargetAlpha, DeltaSeconds, BlendSpeed);
	UpdateCameraTransform();

	if (FMath::IsNearlyEqual(CameraBlendAlpha, TargetAlpha, KINDA_SMALL_NUMBER))
	{
		CameraBlendAlpha = TargetAlpha;
		SetActorTickEnabled(false);
	}
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

// 销毁当前 Avatar 前解除 PlayerState ASC 上的角色级监听。
void AArenaPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindAbilitySystemDelegates();
	Super::EndPlay(EndPlayReason);
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
	BindAbilitySystemDelegates(ArenaASC);

	if (HasAuthority())
	{
		// 属性和技能只在服务端初始化，客户端通过 GAS 复制和 OnRep 接收结果。
		ApplyDefaultAttributes(ArenaPlayerState, ArenaASC);
		GrantStartupAbilities(ArenaPlayerState, ArenaASC);
	}
}

// 绑定并立即同步角色状态，重复初始化时先解除旧绑定。
void AArenaPlayerCharacter::BindAbilitySystemDelegates(UArenaAbilitySystemComponent* ArenaASC)
{
	UnbindAbilitySystemDelegates();
	if (!ArenaASC)
	{
		return;
	}

	BoundAbilitySystemComponent = ArenaASC;
	DeadTagDelegateHandle = ArenaASC->RegisterAndCallGameplayTagEvent(
		ArenaGameplayTags::State_Dead,
		FOnGameplayEffectTagCountChanged::FDelegate::CreateUObject(this, &AArenaPlayerCharacter::HandleDeadTagChanged),
		EGameplayTagEventType::NewOrRemoved);
	StunnedTagDelegateHandle = ArenaASC->RegisterAndCallGameplayTagEvent(
		ArenaGameplayTags::State_Stunned,
		FOnGameplayEffectTagCountChanged::FDelegate::CreateUObject(this, &AArenaPlayerCharacter::HandleStunnedTagChanged),
		EGameplayTagEventType::NewOrRemoved);
	MoveSpeedDelegateHandle = ArenaASC->GetGameplayAttributeValueChangeDelegate(
		UArenaAttributeSet::GetMoveSpeedAttribute()).AddUObject(this, &AArenaPlayerCharacter::HandleMoveSpeedChanged);

	RefreshMaxWalkSpeed();
	RefreshMovementState();
}

// 解除 PlayerState ASC 上属于当前 Avatar 的状态与属性监听。
void AArenaPlayerCharacter::UnbindAbilitySystemDelegates()
{
	UArenaAbilitySystemComponent* ArenaASC = BoundAbilitySystemComponent.Get();
	if (ArenaASC)
	{
		if (DeadTagDelegateHandle.IsValid())
		{
			ArenaASC->UnregisterGameplayTagEvent(DeadTagDelegateHandle, ArenaGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved);
			DeadTagDelegateHandle.Reset();
		}
		if (StunnedTagDelegateHandle.IsValid())
		{
			ArenaASC->UnregisterGameplayTagEvent(StunnedTagDelegateHandle, ArenaGameplayTags::State_Stunned, EGameplayTagEventType::NewOrRemoved);
			StunnedTagDelegateHandle.Reset();
		}
		if (MoveSpeedDelegateHandle.IsValid())
		{
			ArenaASC->GetGameplayAttributeValueChangeDelegate(UArenaAttributeSet::GetMoveSpeedAttribute()).Remove(MoveSpeedDelegateHandle);
			MoveSpeedDelegateHandle.Reset();
		}
	}

	BoundAbilitySystemComponent.Reset();
}

// Dead 的优先级高于 Stunned；只有两个状态都不存在时才恢复 Walking。
void AArenaPlayerCharacter::RefreshMovementState()
{
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	const UArenaAbilitySystemComponent* ArenaASC = BoundAbilitySystemComponent.Get();
	if (!MovementComponent || !ArenaASC)
	{
		return;
	}

	const bool bIsDead = ArenaASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead);
	const bool bIsStunned = ArenaASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Stunned);
	if (bIsDead || bIsStunned)
	{
		SetSprinting(false);
		LastMovementInputDirection = FVector::ZeroVector;
		MovementComponent->StopMovementImmediately();
		MovementComponent->DisableMovement();
	}
	else if (MovementComponent->MovementMode == MOVE_None)
	{
		MovementComponent->SetMovementMode(MOVE_Walking);
	}
}

// State.Dead 增加时执行死亡流程，移除时恢复移动并重置门闩以支持再次死亡。
void AArenaPlayerCharacter::HandleDeadTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	if (CallbackTag != ArenaGameplayTags::State_Dead)
	{
		return;
	}

	RefreshMovementState();
	if (NewCount <= 0)
	{
		const bool bWasDead = bDeathHandled;
		bDeathHandled = false;
		if (bWasDead)
		{
			K2_OnRevived();
		}
		return;
	}

	if (bDeathHandled)
	{
		return;
	}

	bDeathHandled = true;
	if (UArenaAbilitySystemComponent* ArenaASC = BoundAbilitySystemComponent.Get())
	{
		ArenaASC->CancelAllAbilities();
	}
	K2_OnDeathStarted();

	if (HasAuthority())
	{
		if (AArenaGameMode* ArenaGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AArenaGameMode>() : nullptr)
		{
			ArenaGameMode->NotifyPlayerDeath();
		}
	}
}

// 眩晕变化立即刷新移动并取消进行中的技能，解除后由统一状态函数恢复。
void AArenaPlayerCharacter::HandleStunnedTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	if (CallbackTag != ArenaGameplayTags::State_Stunned)
	{
		return;
	}

	RefreshMovementState();
	if (NewCount > 0)
	{
		if (UArenaAbilitySystemComponent* ArenaASC = BoundAbilitySystemComponent.Get())
		{
			ArenaASC->CancelAllAbilities();
		}
	}
	K2_OnStunnedChanged(NewCount > 0);
}

// 将 GAS MoveSpeed 当前值和奔跑倍率同步到 CharacterMovement。
void AArenaPlayerCharacter::HandleMoveSpeedChanged(const FOnAttributeChangeData& Data)
{
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		const float SpeedMultiplier = bIsSprinting ? FMath::Max(SprintSpeedMultiplier, 1.0f) : 1.0f;
		MovementComponent->MaxWalkSpeed = FMath::Max(Data.NewValue, 0.0f) * SpeedMultiplier;
	}
}

// 从 PlayerState AttributeSet 读取基础移速，避免奔跑切换覆盖 GAS 升级结果。
void AArenaPlayerCharacter::RefreshMaxWalkSpeed()
{
	const AArenaPlayerState* ArenaPlayerState = GetPlayerState<AArenaPlayerState>();
	const UArenaAttributeSet* AttributeSet = ArenaPlayerState ? ArenaPlayerState->GetArenaAttributeSet() : nullptr;
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	if (!AttributeSet || !MovementComponent)
	{
		return;
	}

	const float SpeedMultiplier = bIsSprinting ? FMath::Max(SprintSpeedMultiplier, 1.0f) : 1.0f;
	MovementComponent->MaxWalkSpeed = FMath::Max(AttributeSet->GetMoveSpeed(), 0.0f) * SpeedMultiplier;
}

// 本地与服务器共用同一状态入口，Dead/Stunned 永远覆盖奔跑意图。
void AArenaPlayerCharacter::SetSprinting(bool bNewSprinting)
{
	const UAbilitySystemComponent* ArenaASC = GetAbilitySystemComponent();
	const bool bMovementBlocked = ArenaASC
		&& (ArenaASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead)
			|| ArenaASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Stunned));
	bIsSprinting = bNewSprinting && !bMovementBlocked;
	RefreshMaxWalkSpeed();
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
	EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &AArenaPlayerCharacter::Input_SprintStarted);
	EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &AArenaPlayerCharacter::Input_SprintStopped);
	EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Canceled, this, &AArenaPlayerCharacter::Input_SprintStopped);
	EnhancedInputComponent->BindAction(BasicAttackAction, ETriggerEvent::Started, this, &AArenaPlayerCharacter::Input_BasicAttack);
	EnhancedInputComponent->BindAction(FireballAction, ETriggerEvent::Started, this, &AArenaPlayerCharacter::Input_Fireball);
	EnhancedInputComponent->BindAction(DashAction, ETriggerEvent::Started, this, &AArenaPlayerCharacter::Input_Dash);
	EnhancedInputComponent->BindAction(ShieldAction, ETriggerEvent::Started, this, &AArenaPlayerCharacter::Input_Shield);
	EnhancedInputComponent->BindAction(UltimateAction, ETriggerEvent::Started, this, &AArenaPlayerCharacter::Input_Ultimate);
	EnhancedInputComponent->BindAction(ViewToggleAction, ETriggerEvent::Started, this, &AArenaPlayerCharacter::Input_ToggleView);
	EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AArenaPlayerCharacter::Input_Look);
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

	SprintAction = CreateDefaultSubobject<UInputAction>(TEXT("Sprint"));
	SprintAction->ValueType = EInputActionValueType::Boolean;

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

	ViewToggleAction = CreateDefaultSubobject<UInputAction>(TEXT("ToggleView"));
	ViewToggleAction->ValueType = EInputActionValueType::Boolean;

	LookAction = CreateDefaultSubobject<UInputAction>(TEXT("Look"));
	LookAction->ValueType = EInputActionValueType::Axis2D;

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
	DefaultMappingContext->MapKey(ViewToggleAction, EKeys::Zero);
	DefaultMappingContext->MapKey(ViewToggleAction, EKeys::NumPadZero);
	DefaultMappingContext->MapKey(LookAction, EKeys::Mouse2D);
	DefaultMappingContext->MapKey(SprintAction, EKeys::LeftShift);
	DefaultMappingContext->MapKey(SprintAction, EKeys::RightShift);
}

// 将本地技能输入转换为 GameplayTag，让 ASC 决定能否激活技能。
void AArenaPlayerCharacter::Input_AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	// Character 只负责把本地输入转成标签，是否能激活由 ASC/GAS 判断。
	UArenaAbilitySystemComponent* ArenaASC = Cast<UArenaAbilitySystemComponent>(GetAbilitySystemComponent());
	if (!ArenaASC
		|| ArenaASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead)
		|| ArenaASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Stunned))
	{
		return;
	}

	ArenaASC->AbilityInputTagPressed(InputTag);
}

// 处理 WASD 移动，并缓存移动方向供 Dash 等技能读取。
void AArenaPlayerCharacter::Input_Move(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();
	const UAbilitySystemComponent* ArenaASC = GetAbilitySystemComponent();

	if (!Controller || MovementVector.IsNearlyZero()
		|| (ArenaASC && (ArenaASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead)
			|| ArenaASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Stunned))))
	{
		LastMovementInputDirection = FVector::ZeroVector;
		return;
	}

	FVector ForwardDirection = FVector::ForwardVector;
	FVector RightDirection = FVector::RightVector;
	if (bThirdPersonView)
	{
		const FRotator ControlYawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
		ForwardDirection = FRotationMatrix(ControlYawRotation).GetUnitAxis(EAxis::X);
		RightDirection = FRotationMatrix(ControlYawRotation).GetUnitAxis(EAxis::Y);
	}

	const FVector MoveDirection = (ForwardDirection * MovementVector.Y + RightDirection * MovementVector.X).GetSafeNormal();
	LastMovementInputDirection = MoveDirection;

	AddMovementInput(ForwardDirection, MovementVector.Y);
	AddMovementInput(RightDirection, MovementVector.X);
}

// 移动输入结束时清空缓存方向，避免后续技能使用过期方向。
void AArenaPlayerCharacter::Input_MoveStopped(const FInputActionValue& Value)
{
	LastMovementInputDirection = FVector::ZeroVector;
}

// 本地先更新速度减少输入延迟，再由服务器应用相同的受限奔跑倍率。
void AArenaPlayerCharacter::Input_SprintStarted(const FInputActionValue& Value)
{
	SetSprinting(true);
	if (!HasAuthority())
	{
		ServerSetSprinting(true);
	}
}

// 松开 Shift 时两端恢复未经倍率放大的 GAS MoveSpeed。
void AArenaPlayerCharacter::Input_SprintStopped(const FInputActionValue& Value)
{
	SetSprinting(false);
	if (!HasAuthority())
	{
		ServerSetSprinting(false);
	}
}

// 服务端仅接受开关意图，最终速度仍由受限倍率和服务器持有的 MoveSpeed 决定。
void AArenaPlayerCharacter::ServerSetSprinting_Implementation(bool bNewSprinting)
{
	SetSprinting(bNewSprinting);
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

// 闪电风暴输入入口，仅发送 Ability.LightningStorm 标签。
void AArenaPlayerCharacter::Input_Ultimate()
{
	Input_AbilityInputTagPressed(ArenaGameplayTags::Ability_LightningStorm);
}

// 切换本地视角，并初始化第三人称控制角度或恢复顶视角鼠标模式。
void AArenaPlayerCharacter::Input_ToggleView()
{
	if (!IsLocallyControlled() || !Controller)
	{
		return;
	}

	bThirdPersonView = !bThirdPersonView;
	if (bThirdPersonView)
	{
		Controller->SetControlRotation(FRotator(ThirdPersonInitialPitch, GetActorRotation().Yaw, 0.0f));
	}

	CameraBoom->bDoCollisionTest = bThirdPersonView;
	if (AArenaPlayerController* ArenaPlayerController = Cast<AArenaPlayerController>(Controller))
	{
		ArenaPlayerController->SetThirdPersonInputMode(bThirdPersonView);
	}

	SetActorTickEnabled(true);
	UpdateCameraTransform();
}

// 第三人称下把鼠标增量转换为受限的 ControlRotation，并立即刷新相机。
void AArenaPlayerCharacter::Input_Look(const FInputActionValue& Value)
{
	if (!bThirdPersonView || !Controller)
	{
		return;
	}

	const FVector2D LookAxis = Value.Get<FVector2D>();
	FRotator ControlRotation = Controller->GetControlRotation();
	ControlRotation.Yaw += LookAxis.X * LookYawSensitivity;
	ControlRotation.Pitch = FMath::Clamp(
		FRotator::NormalizeAxis(ControlRotation.Pitch + LookAxis.Y * LookPitchSensitivity),
		ThirdPersonMinPitch,
		ThirdPersonMaxPitch);
	ControlRotation.Roll = 0.0f;
	Controller->SetControlRotation(ControlRotation);
	UpdateCameraTransform();
}

// 用同一套 SpringArm 在固定顶视角和控制器驱动的第三人称之间插值。
void AArenaPlayerCharacter::UpdateCameraTransform()
{
	if (!CameraBoom)
	{
		return;
	}

	const float SmoothedAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, CameraBlendAlpha, 2.0f);
	CameraBoom->TargetArmLength = FMath::Lerp(TopDownArmLength, ThirdPersonArmLength, SmoothedAlpha);
	CameraBoom->TargetOffset = FMath::Lerp(FVector::ZeroVector, ThirdPersonTargetOffset, SmoothedAlpha);

	const FRotator ThirdPersonRotation = Controller
		? Controller->GetControlRotation()
		: FRotator(ThirdPersonInitialPitch, GetActorRotation().Yaw, 0.0f);
	const FQuat BlendedRotation = FQuat::Slerp(
		TopDownCameraRotation.Quaternion(),
		ThirdPersonRotation.Quaternion(),
		SmoothedAlpha);
	CameraBoom->SetWorldRotation(BlendedRotation);
}
