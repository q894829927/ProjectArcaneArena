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
	SetDamageFeedbackPresentation(InDamageAmount, bInCriticalHit, EArenaDamageFeedbackType::HealthOnly);
}

// 资源分类决定普通数字颜色，暴击在其上统一使用更大的金色强调。
void UArenaDamageNumberWidget::SetDamageFeedbackPresentation(
	float InDamageAmount,
	bool bInCriticalHit,
	EArenaDamageFeedbackType InFeedbackType)
{
	DamageAmount = FMath::Max(InDamageAmount, 0.0f);
	bCriticalHit = bInCriticalHit;
	FeedbackType = InFeedbackType;

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
		FLinearColor FeedbackColor = CachedBaseColor.GetSpecifiedColor();
		switch (FeedbackType)
		{
		case EArenaDamageFeedbackType::ShieldOnly:
			FeedbackColor = FLinearColor(0.1f, 0.85f, 1.0f, 1.0f);
			break;
		case EArenaDamageFeedbackType::ShieldBreak:
			FeedbackColor = FLinearColor(0.65f, 0.95f, 1.0f, 1.0f);
			break;
		case EArenaDamageFeedbackType::ShieldBreakWithHealthDamage:
			FeedbackColor = FLinearColor(0.9f, 0.4f, 0.75f, 1.0f);
			break;
		case EArenaDamageFeedbackType::HealthOnly:
		default:
			FeedbackColor = FLinearColor(1.0f, 0.72f, 0.68f, 1.0f);
			break;
		}
		DamageText->SetColorAndOpacity(FSlateColor(bCriticalHit
			? FLinearColor(1.0f, 0.72f, 0.12f, 1.0f)
			: FeedbackColor));
	}
}
