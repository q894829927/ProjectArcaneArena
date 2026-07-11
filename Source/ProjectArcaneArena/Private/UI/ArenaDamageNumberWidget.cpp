#include "UI/ArenaDamageNumberWidget.h"

#include "Components/TextBlock.h"

// 设置伤害数字文本，显示层只格式化数值不参与伤害结算。
void UArenaDamageNumberWidget::SetDamageAmount(float InDamageAmount)
{
	DamageAmount = FMath::Max(InDamageAmount, 0.0f);

	if (DamageText)
	{
		DamageText->SetText(FText::AsNumber(FMath::RoundToInt(DamageAmount)));
	}
}
