#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "ArenaGameplayCueNotify_DamageNumber.generated.h"

UCLASS(Blueprintable)
class PROJECTARCANEARENA_API UArenaGameplayCueNotify_DamageNumber : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

protected:
	// 在收到服务器确认的 Execute Cue 时，为敌人生成一次本地伤害数字。
	virtual bool OnExecute_Implementation(
		AActor* MyTarget,
		const FGameplayCueParameters& Parameters) const override;

	// 由普通/暴击 Cue Blueprint CDO 决定本次数字使用哪种表现。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Damage Number")
	bool bCriticalStyle = false;
};
