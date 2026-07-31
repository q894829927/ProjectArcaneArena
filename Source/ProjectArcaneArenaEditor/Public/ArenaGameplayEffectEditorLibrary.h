#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ArenaGameplayEffectEditorLibrary.generated.h"

class UGameplayEffect;

UCLASS()
class PROJECTARCANEARENAEDITOR_API UArenaGameplayEffectEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 批量替换 GameplayEffect Modifier 的常量 ScalableFloat，供 Python 安全编辑受保护的嵌套结构。
	UFUNCTION(BlueprintCallable, Category = "Arena|Editor|GameplayEffect")
	static bool SetScalableFloatModifierMagnitudes(
		UGameplayEffect* GameplayEffect,
		const TArray<int32>& ModifierIndices,
		const TArray<float>& Magnitudes,
		FString& OutReport);
};
