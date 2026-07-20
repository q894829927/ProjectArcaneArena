#include "Character/ArenaBossCharacter.h"

#include "AI/ArenaBossAIController.h"
#include "GameFramework/CharacterMovementComponent.h"

// Boss 沿用敌人 ASC 与死亡链路，移动时面向路径速度，攻击时再由 Ability Task 面向目标。
AArenaBossCharacter::AArenaBossCharacter()
	: BossDisplayName(NSLOCTEXT("ArenaBossCharacter", "DefaultBossName", "悟空战将"))
{
	AIControllerClass = AArenaBossAIController::StaticClass();
	bShowWorldHealthBar = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
}
