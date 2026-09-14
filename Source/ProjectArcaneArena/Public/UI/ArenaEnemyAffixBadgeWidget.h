#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ArenaEnemyAffixBadgeWidget.generated.h"

class UTextBlock;

UCLASS()
class PROJECTARCANEARENA_API UArenaEnemyAffixBadgeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 刷新精英词缀名称和主题色，Widget 只观察复制数据。
	void SetAffixPresentation(const FText& AffixName, const FLinearColor& AccentColor);

protected:
	// 构造无 Blueprint 依赖的轻量文本 Badge，确保生成资产前也有可读反馈。
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BadgeText;
};
