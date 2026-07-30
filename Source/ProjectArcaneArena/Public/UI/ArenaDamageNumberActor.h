#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GAS/ArenaDamageFeedbackTypes.h"
#include "ArenaDamageNumberActor.generated.h"

class UWidgetComponent;

UCLASS()
class PROJECTARCANEARENA_API AArenaDamageNumberActor : public AActor
{
	GENERATED_BODY()

public:
	AArenaDamageNumberActor();

	// 让伤害数字按 Ease-Out 轨迹上浮，并在生命周期末段渐隐。
	virtual void Tick(float DeltaSeconds) override;

	// 设置显示数值，并同步到内部 Widget。
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void SetDamageAmount(float InDamageAmount);
	// 同步实际伤害和暴击样式，供本地 GameplayCue 调用。
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void SetDamagePresentation(float InDamageAmount, bool bInCriticalHit);
	// 根据权威资源损失分类刷新数字颜色与复合样式。
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void SetDamageFeedbackPresentation(
		float InDamageAmount,
		bool bInCriticalHit,
		EArenaDamageFeedbackType InFeedbackType);

protected:
	// BeginPlay 时刷新生命周期和初始显示数值。
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena|UI")
	TObjectPtr<UWidgetComponent> WidgetComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|UI", meta = (ClampMin = "0.0"))
	float FloatSpeed = 35.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|UI", meta = (ClampMin = "0.0"))
	float LifeSpan = 0.9f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|UI", meta = (ClampMin = "0.0", ClampMax = "0.95"))
	float FadeStartNormalized = 0.6f;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	float DamageAmount = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	bool bCriticalHit = false;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	EArenaDamageFeedbackType FeedbackType = EArenaDamageFeedbackType::HealthOnly;

private:
	FVector2D CachedBaseDrawSize = FVector2D::ZeroVector;
	FVector PresentationStartLocation = FVector::ZeroVector;
	float ElapsedPresentationTime = 0.0f;
};
