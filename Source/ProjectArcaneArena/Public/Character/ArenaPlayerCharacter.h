#pragma once

#include "CoreMinimal.h"
#include "Character/ArenaCharacterBase.h"
#include "GameplayEffectTypes.h"
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
	// 角色销毁或换 Pawn 时解绑 PlayerState ASC 委托，避免旧 Avatar 接收回调。
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// 绑定 Enhanced Input，本轮输入只路由到移动和 GAS 输入标签。
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// 玩家死亡表现入口；玩法状态和失败判定仍由 C++/服务器拥有。
	UFUNCTION(BlueprintImplementableEvent, Category = "Arena|Player")
	void K2_OnDeathStarted();

	// 眩晕开始或结束的表现入口，不用于决定角色能否移动。
	UFUNCTION(BlueprintImplementableEvent, Category = "Arena|Player")
	void K2_OnStunnedChanged(bool bIsStunned);

private:
	// 统一初始化 PlayerState ASC 的 OwnerActor/AvatarActor。
	void InitializeAbilityActorInfo();
	// 通过 GameplayEffect 初始化玩家默认属性，避免直接写属性值。
	void ApplyDefaultAttributes(AArenaPlayerState* ArenaPlayerState, UArenaAbilitySystemComponent* ArenaASC);
	// 服务端授予启动技能，并把技能输入标签写入 AbilitySpec。
	void GrantStartupAbilities(AArenaPlayerState* ArenaPlayerState, UArenaAbilitySystemComponent* ArenaASC);
	// 绑定死亡、眩晕和移速属性委托，支持 PlayerState ASC 在重生时重新指向 Avatar。
	void BindAbilitySystemDelegates(UArenaAbilitySystemComponent* ArenaASC);
	// 移除当前绑定的 GAS 委托，防止重复初始化和旧角色悬挂回调。
	void UnbindAbilitySystemDelegates();
	// 根据 Dead/Stunned 优先级统一刷新移动组件状态。
	void RefreshMovementState();
	void HandleDeadTagChanged(const FGameplayTag CallbackTag, int32 NewCount);
	void HandleStunnedTagChanged(const FGameplayTag CallbackTag, int32 NewCount);
	void HandleMoveSpeedChanged(const FOnAttributeChangeData& Data);
	// 使用当前 GAS MoveSpeed 和奔跑倍率统一刷新 CharacterMovement。
	void RefreshMaxWalkSpeed();
	// 切换本地或服务器的奔跑意图，死亡和眩晕状态会强制拒绝奔跑。
	void SetSprinting(bool bNewSprinting);
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
	// 按住 Shift 开始奔跑，并把意图同步给服务器。
	void Input_SprintStarted(const FInputActionValue& Value);
	// 松开 Shift 恢复 GAS MoveSpeed。
	void Input_SprintStopped(const FInputActionValue& Value);
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

	// 服务端校验并应用奔跑意图，防止客户端直接决定权威速度。
	UFUNCTION(Server, Reliable)
	void ServerSetSprinting(bool bNewSprinting);

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
	TObjectPtr<UInputAction> SprintAction;

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

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Sprint", meta = (ClampMin = "1.0"))
	float SprintSpeedMultiplier = 1.5f;

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

	TWeakObjectPtr<UArenaAbilitySystemComponent> BoundAbilitySystemComponent;
	FDelegateHandle DeadTagDelegateHandle;
	FDelegateHandle StunnedTagDelegateHandle;
	FDelegateHandle MoveSpeedDelegateHandle;
	bool bDeathHandled = false;
	bool bIsSprinting = false;
};
