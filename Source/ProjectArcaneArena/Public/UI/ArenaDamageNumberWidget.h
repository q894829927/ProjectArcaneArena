#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GAS/ArenaDamageFeedbackTypes.h"
#include "Styling/SlateColor.h"
#include "ArenaDamageNumberWidget.generated.h"

class UTextBlock;

UCLASS()
class PROJECTARCANEARENA_API UArenaDamageNumberWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 设置伤害数字文本，Widget 不保存任何玩法状态。
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void SetDamageAmount(float InDamageAmount);
	// 使用普通或暴击样式显示服务器确认的实际伤害。
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void SetDamagePresentation(float InDamageAmount, bool bInCriticalHit);
	// 按护盾、生命或复合资源损失设置单个数字的颜色与强调等级。
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void SetDamageFeedbackPresentation(
		float InDamageAmount,
		bool bInCriticalHit,
		EArenaDamageFeedbackType InFeedbackType);

	UFUNCTION(BlueprintPure, Category = "Arena|UI")
	float GetDamageAmount() const { return DamageAmount; }

	// 供蓝图动画或样式逻辑读取当前数字是否为暴击。
	UFUNCTION(BlueprintPure, Category = "Arena|UI")
	bool IsCriticalHit() const { return bCriticalHit; }
	// 供蓝图动画读取当前数字属于护盾、生命或复合伤害。
	UFUNCTION(BlueprintPure, Category = "Arena|UI")
	EArenaDamageFeedbackType GetFeedbackType() const { return FeedbackType; }

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Arena|UI")
	TObjectPtr<UTextBlock> DamageText;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	float DamageAmount = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	bool bCriticalHit = false;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	EArenaDamageFeedbackType FeedbackType = EArenaDamageFeedbackType::HealthOnly;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|UI", meta = (ClampMin = "1.0"))
	float CriticalFontScale = 1.35f;

private:
	int32 CachedBaseFontSize = 0;
	FSlateColor CachedBaseColor;
	bool bCachedBaseStyle = false;
};
