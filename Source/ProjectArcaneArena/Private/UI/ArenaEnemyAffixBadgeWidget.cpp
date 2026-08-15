#include "UI/ArenaEnemyAffixBadgeWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"

// 创建原生文本根节点，避免精英身份依赖现有 WBP_EnemyHealthBar 的 WidgetTree。
void UArenaEnemyAffixBadgeWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree)
	{
		return;
	}

	BadgeText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EliteAffixText"));
	if (!BadgeText)
	{
		return;
	}
	BadgeText->SetJustification(ETextJustify::Center);
	BadgeText->SetShadowOffset(FVector2D(1.5f, 1.5f));
	BadgeText->SetShadowColorAndOpacity(FLinearColor::Black);
	FSlateFontInfo Font = BadgeText->GetFont();
	Font.Size = 15;
	Font.OutlineSettings.OutlineSize = 1;
	Font.OutlineSettings.OutlineColor = FLinearColor::Black;
	BadgeText->SetFont(Font);
	WidgetTree->RootWidget = BadgeText;
}

// 将本地化词缀名组合为精英 Badge，不参与任何玩法状态判断。
void UArenaEnemyAffixBadgeWidget::SetAffixPresentation(const FText& AffixName, const FLinearColor& AccentColor)
{
	if (!BadgeText)
	{
		return;
	}
	BadgeText->SetText(FText::Format(NSLOCTEXT("ArenaElite", "EliteBadgeFormat", "精英 · {0}"), AffixName));
	BadgeText->SetColorAndOpacity(FSlateColor(AccentColor));
}
