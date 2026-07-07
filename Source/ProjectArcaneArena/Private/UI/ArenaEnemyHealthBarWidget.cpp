#include "UI/ArenaEnemyHealthBarWidget.h"

#include "Components/ProgressBar.h"

void UArenaEnemyHealthBarWidget::SetHealthValues(float InHealth, float InMaxHealth)
{
	CurrentHealth = FMath::Max(InHealth, 0.0f);
	CurrentMaxHealth = FMath::Max(InMaxHealth, 0.0f);
	// MaxHealth 可能尚未初始化或被配置为 0，UI 层只显示 0% 而不修正玩法属性。
	HealthPercent = CurrentMaxHealth > 0.0f
		? FMath::Clamp(CurrentHealth / CurrentMaxHealth, 0.0f, 1.0f)
		: 0.0f;

	if (HealthProgressBar)
	{
		HealthProgressBar->SetPercent(HealthPercent);
	}
}
