#pragma once

#include "CoreMinimal.h"
#include "Character/ArenaEnemyCharacter.h"
#include "ArenaBossCharacter.generated.h"

UCLASS()
class PROJECTARCANEARENA_API AArenaBossCharacter : public AArenaEnemyCharacter
{
	GENERATED_BODY()

public:
	// 设置 Boss 专属 Controller、路径朝向和显示默认值，战斗能力与死亡流程继续复用敌人基类。
	AArenaBossCharacter();

	// 返回 Boss HUD 使用的本地化名称，UI 不自行保存玩法身份。
	UFUNCTION(BlueprintPure, Category = "Arena|Boss")
	FText GetBossDisplayName() const { return BossDisplayName; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Boss")
	FText BossDisplayName;
};
