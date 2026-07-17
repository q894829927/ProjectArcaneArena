#include "Character/ArenaBossCharacter.h"

// Boss 第一阶段沿用敌人 ASC、AI 和死亡链路，只关闭重复的头顶血条。
AArenaBossCharacter::AArenaBossCharacter()
	: BossDisplayName(NSLOCTEXT("ArenaBossCharacter", "DefaultBossName", "悟空战将"))
{
	bShowWorldHealthBar = false;
}
