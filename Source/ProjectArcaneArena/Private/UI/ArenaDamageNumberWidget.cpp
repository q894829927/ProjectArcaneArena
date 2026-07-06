#include "UI/ArenaDamageNumberWidget.h"

#include "Components/TextBlock.h"

void UArenaDamageNumberWidget::SetDamageAmount(float InDamageAmount)
{
	DamageAmount = FMath::Max(InDamageAmount, 0.0f);

	if (DamageText)
	{
		DamageText->SetText(FText::AsNumber(FMath::RoundToInt(DamageAmount)));
	}
}
