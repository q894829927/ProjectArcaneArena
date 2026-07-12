#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ArenaNiagaraEditorLibrary.generated.h"

class UNiagaraSystem;

UCLASS()
class PROJECTARCANEARENAEDITOR_API UArenaNiagaraEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 从已验证的循环 Niagara 复制生命周期设置，并将目标 Emitter 切换为 Local Space。
	UFUNCTION(BlueprintCallable, Category = "Arena|Editor|Niagara")
	static bool CopyLoopingLifecycleFromReference(
		UNiagaraSystem* TargetSystem,
		UNiagaraSystem* ReferenceSystem,
		FString& OutReport);
};
