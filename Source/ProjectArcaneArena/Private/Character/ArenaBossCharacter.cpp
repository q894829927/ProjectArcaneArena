#include "Character/ArenaBossCharacter.h"

#include "AI/ArenaBossAIController.h"

// Boss 沿用敌人 ASC 与死亡链路，但改由专用 BehaviorTree Controller 决策并关闭重复头顶血条。
AArenaBossCharacter::AArenaBossCharacter()
	: BossDisplayName(NSLOCTEXT("ArenaBossCharacter", "DefaultBossName", "悟空战将"))
{
	AIControllerClass = AArenaBossAIController::StaticClass();
	bShowWorldHealthBar = false;
}
