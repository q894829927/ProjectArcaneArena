#pragma once

#include "CoreMinimal.h"
#include "Character/ArenaCharacterBase.h"
#include "InputActionValue.h"
#include "ArenaPlayerCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class USpringArmComponent;
class APlayerController;

UCLASS()
class PROJECTARCANEARENA_API AArenaPlayerCharacter : public AArenaCharacterBase
{
	GENERATED_BODY()

public:
	AArenaPlayerCharacter();

protected:
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
	void AddDefaultMappingContext() const;
	void CreateDefaultInputMappings();
	void FaceMouseCursor();
	bool GetMouseAimPointOnPlane(const APlayerController& PlayerController, FVector& OutAimPoint) const;
	void ApplyFacingRotation(const FRotator& NewRotation);

	void Input_Move(const FInputActionValue& Value);
	void Input_BasicAttack();
	void Input_Fireball();
	void Input_Dash();
	void Input_Shield();
	void Input_Ultimate();

	UFUNCTION(Server, Reliable)
	void Server_SetFacingRotation(FRotator NewRotation);

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

	UPROPERTY(EditDefaultsOnly, Category = "Aiming")
	float AimPlaneZ = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	float FacingReplicationYawTolerance = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	float FacingReplicationMinInterval = 0.05f;

	float LastSentFacingYaw = 0.0f;
	float LastFacingReplicationTime = 0.0f;
};
