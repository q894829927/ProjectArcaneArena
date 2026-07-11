#pragma once

#include "CoreMinimal.h"
#include "Character/ArenaCharacterBase.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "ArenaPlayerCharacter.generated.h"

class AArenaPlayerState;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class USpringArmComponent;
class UAbilitySystemComponent;
class UArenaAbilitySystemComponent;
class UGameplayAbility;
class UGameplayEffect;

UCLASS()
class PROJECTARCANEARENA_API AArenaPlayerCharacter : public AArenaCharacterBase
{
	GENERATED_BODY()

public:
	AArenaPlayerCharacter();

	// 返回本地玩家当前是否使用第三人称瞄准模式，供 TargetActor 选择鼠标或中心准星。
	bool IsUsingThirdPersonView() const { return bThirdPersonView; }

	// 玩家角色的 ASC 存放在 PlayerState 上，这里只负责转发访问。
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// Returns current dash input direction, or zero when standing still.
	FVector GetLastMovementInputDirection() const { return LastMovementInputDirection; }

protected:
	// 仅在视角过渡期间更新相机插值，第三人称稳定后由 Look 输入直接刷新。
	virtual void Tick(float DeltaSeconds) override;
	// 服务端 Possess 后初始化 AvatarActor，并授予默认属性和启动技能。
	virtual void PossessedBy(AController* NewController) override;
	// 客户端收到 PlayerState 后重新初始化 AvatarActor，确保 ASC 指向当前角色。
	virtual void OnRep_PlayerState() override;
	// 绑定 Enhanced Input，本轮输入只路由到移动和 GAS 输入标签。
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
	// 统一初始化 PlayerState ASC 的 OwnerActor/AvatarActor。
	void InitializeAbilityActorInfo();
	// 通过 GameplayEffect 初始化玩家默认属性，避免直接写属性值。
	void ApplyDefaultAttributes(AArenaPlayerState* ArenaPlayerState, UArenaAbilitySystemComponent* ArenaASC);
	// 服务端授予启动技能，并把技能输入标签写入 AbilitySpec。
	void GrantStartupAbilities(AArenaPlayerState* ArenaPlayerState, UArenaAbilitySystemComponent* ArenaASC);
	// 将默认输入映射加入本地玩家的 Enhanced Input 子系统。
	void AddDefaultMappingContext() const;
	// 创建模板阶段使用的 C++ 默认输入资产，后续可迁移到项目资产。
	void CreateDefaultInputMappings();
	// 把本地输入转换成 GAS InputTag，由 ASC 决定是否激活 Ability。
	void Input_AbilityInputTagPressed(const FGameplayTag& InputTag);

	// WASD 移动输入，使用 CharacterMovement 以保持后续网络移动兼容。
	void Input_Move(const FInputActionValue& Value);
	// Clears cached dash direction when movement input stops.
	void Input_MoveStopped(const FInputActionValue& Value);
	// 基础攻击输入入口，当前只发送 Ability.BasicAttack 标签。
	void Input_BasicAttack();
	// Fireball 输入入口，只发送 Ability.Fireball 标签，具体技能逻辑由 GAS 处理。
	void Input_Fireball();
	// Dash 输入入口，只发送 Ability.Dash 标签，具体技能逻辑由 GAS 处理。
	void Input_Dash();
	// Shield 输入入口，只发送 Ability.Shield 标签，具体护盾逻辑由 GAS 处理。
	void Input_Shield();
	// LightningStorm 输入入口，只发送 Ability.LightningStorm 标签，具体范围伤害由 GAS 处理。
	void Input_Ultimate();
	// 切换顶视角和第三人称，并同步本地鼠标/准星输入模式。
	void Input_ToggleView();
	// 第三人称模式下使用鼠标增量旋转控制器和相机。
	void Input_Look(const FInputActionValue& Value);
	// 根据当前混合值更新 SpringArm 的距离、偏移和世界旋转。
	void UpdateCameraTransform();

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> TopDownCamera;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> BasicAttackAction;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> FireballAction;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> DashAction;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> ShieldAction;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> UltimateAction;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> ViewToggleAction;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	int32 InputMappingPriority = 0;

	UPROPERTY(EditDefaultsOnly, Category = "Camera|TopDown", meta = (ClampMin = "0.0"))
	float TopDownArmLength = 900.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera|TopDown")
	FRotator TopDownCameraRotation = FRotator(-60.0f, 0.0f, 0.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Camera|ThirdPerson", meta = (ClampMin = "0.0"))
	float ThirdPersonArmLength = 400.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera|ThirdPerson")
	FVector ThirdPersonTargetOffset = FVector(0.0f, 0.0f, 70.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Camera|ThirdPerson")
	float ThirdPersonInitialPitch = -15.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera|ThirdPerson")
	float ThirdPersonMinPitch = -65.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera|ThirdPerson")
	float ThirdPersonMaxPitch = 35.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera|ThirdPerson", meta = (ClampMin = "0.01"))
	float CameraTransitionDuration = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera|ThirdPerson", meta = (ClampMin = "0.0"))
	float LookYawSensitivity = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera|ThirdPerson", meta = (ClampMin = "0.0"))
	float LookPitchSensitivity = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TSubclassOf<UGameplayEffect> DefaultAttributeEffect;

	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	FVector LastMovementInputDirection = FVector::ZeroVector;
	bool bThirdPersonView = false;
	float CameraBlendAlpha = 0.0f;
};
