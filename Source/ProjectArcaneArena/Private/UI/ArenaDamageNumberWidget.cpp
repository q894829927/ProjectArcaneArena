#include "UI/ArenaDamageNumberWidget.h"

#include "Components/TextBlock.h"

// 保留原有蓝图接口，未指定样式时按普通伤害显示。
void UArenaDamageNumberWidget::SetDamageAmount(float InDamageAmount)
{
	SetDamagePresentation(InDamageAmount, false);
}

// 根据暴击结果设置文本、颜色和字号，显示层不参与伤害结算。
void UArenaDamageNumberWidget::SetDamagePresentation(float InDamageAmount, bool bInCriticalHit)
{
	DamageAmount = FMath::Max(InDamageAmount, 0.0f);
	bCriticalHit = bInCriticalHit;

	if (DamageText)
	{
		DamageText->SetText(FText::AsNumber(FMath::RoundToInt(DamageAmount)));
		FSlateFontInfo Font = DamageText->GetFont();
		if (!bCachedBaseStyle)
		{
			CachedBaseFontSize = FMath::Max(Font.Size, 1);
			CachedBaseColor = DamageText->GetColorAndOpacity();
			bCachedBaseStyle = true;
		}
		Font.Size = bCriticalHit
			? FMath::RoundToInt(static_cast<float>(CachedBaseFontSize) * CriticalFontScale)
			: CachedBaseFontSize;
		DamageText->SetFont(Font);
		DamageText->SetColorAndOpacity(bCriticalHit
			? FSlateColor(FLinearColor(1.0f, 0.72f, 0.12f, 1.0f))
			: CachedBaseColor);
	}
}
