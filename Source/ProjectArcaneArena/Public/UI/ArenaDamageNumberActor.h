#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaDamageNumberActor.generated.h"

class UWidgetComponent;

UCLASS()
class PROJECTARCANEARENA_API AArenaDamageNumberActor : public AActor
{
	GENERATED_BODY()

public:
	AArenaDamageNumberActor();

	// 让伤害数字以轻量方式向上漂浮，作为临时受击反馈。
	virtual void Tick(float DeltaSeconds) override;

	// 设置显示数值，并同步到内部 Widget。
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void SetDamageAmount(float InDamageAmount);

protected:
	// BeginPlay 时刷新生命周期和初始显示数值。
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena|UI")
	TObjectPtr<UWidgetComponent> WidgetComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|UI", meta = (ClampMin = "0.0"))
	float FloatSpeed = 35.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|UI", meta = (ClampMin = "0.0"))
	float LifeSpan = 0.8f;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|UI")
	float DamageAmount = 0.0f;
};
