#include "UI/ArenaEnemyHealthBarWidget.h"

#include "Components/ProgressBar.h"

void UArenaEnemyHealthBarWidget::SetHealthValues(float InHealth, float InMaxHealth)
{
	CurrentHealth = FMath::Max(InHealth, 0.0f);
	CurrentMaxHealth = FMath::Max(InMaxHealth, 0.0f);
	HealthPercent = CurrentMaxHealth > 0.0f
		? FMath::Clamp(CurrentHealth / CurrentMaxHealth, 0.0f, 1.0f)
		: 0.0f;

	if (HealthProgressBar)
	{
		HealthProgressBar->SetPercent(HealthPercent);
	}
}
