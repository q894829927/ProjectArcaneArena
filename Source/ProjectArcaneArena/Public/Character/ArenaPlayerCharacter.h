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

	// 玩家角色的 ASC 存放在 PlayerState 上，这里只负责转发访问。
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// Returns current dash input direction, or zero when standing still.
	FVector GetLastMovementInputDirection() const { return LastMovementInputDirection; }

protected:
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
	// 预留技能输入入口，后续接入对应 GameplayAbility。
	void Input_Dash();
	// 预留技能输入入口，后续接入对应 GameplayAbility。
	void Input_Shield();
	// 预留技能输入入口，后续接入对应 GameplayAbility。
	void Input_Ultimate();

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

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	int32 InputMappingPriority = 0;

	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TSubclassOf<UGameplayEffect> DefaultAttributeEffect;

	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	FVector LastMovementInputDirection = FVector::ZeroVector;
};
