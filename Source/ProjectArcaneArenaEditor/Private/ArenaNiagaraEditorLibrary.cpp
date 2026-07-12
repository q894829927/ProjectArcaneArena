#include "ArenaNiagaraEditorLibrary.h"

#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraSystem.h"
#include "UObject/Class.h"
#include "UObject/StructOnScope.h"
#include "ViewModels/NiagaraEmitterHandleViewModel.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "ViewModels/Stack/NiagaraStackFunctionInput.h"
#include "ViewModels/Stack/NiagaraStackModuleItem.h"
#include "ViewModels/Stack/NiagaraStackViewModel.h"

namespace ArenaNiagaraEditor
{
	using FStoredInputMap = TMap<FString, TSharedPtr<const FStructOnScope>>;

	// 为离线 Stack 处理创建带资产消息标识的 ViewModel，避免 Niagara 消息订阅断言。
	TSharedRef<FNiagaraSystemViewModel> CreateDataProcessingViewModel(UNiagaraSystem& System)
	{
		FNiagaraSystemViewModelOptions ViewModelOptions;
		ViewModelOptions.bCanModifyEmittersFromTimeline = false;
		ViewModelOptions.bCanAutoCompile = false;
		ViewModelOptions.bCanSimulate = false;
		ViewModelOptions.bIsForDataProcessingOnly = true;
		const FGuid AssetGuid = System.GetAssetGuid();
		ViewModelOptions.MessageLogGuid = AssetGuid.IsValid() ? AssetGuid : FGuid::NewGuid();

		TSharedRef<FNiagaraSystemViewModel> ViewModel = MakeShared<FNiagaraSystemViewModel>();
		ViewModel->Initialize(System, ViewModelOptions);
		return ViewModel;
	}

	// 统一 Niagara Stack 显示名，避免空格和下划线影响匹配。
	FString NormalizeName(FString Name)
	{
		Name.ToLowerInline();
		Name.ReplaceInline(TEXT(" "), TEXT(""));
		Name.ReplaceInline(TEXT("_"), TEXT(""));
		return Name;
	}

	// 只选择本次修复需要的 System State 与 Emitter State 输入。
	FString MakeInputKey(const FString& ModuleName, const FString& InputName)
	{
		const FString NormalizedModule = NormalizeName(ModuleName);
		const FString NormalizedInput = NormalizeName(InputName);
		if (NormalizedModule.Contains(TEXT("systemstate")) && NormalizedInput.Contains(TEXT("loopbehavior")))
		{
			return TEXT("System.LoopBehavior");
		}
		if (NormalizedModule.Contains(TEXT("emitterstate")) && NormalizedInput.Contains(TEXT("lifecyclemode")))
		{
			return TEXT("Emitter.LifeCycleMode");
		}
		return FString();
	}

	// 从一个 System 或 Emitter Stack 收集可复制的本地输入值。
	void CollectStackInputs(UNiagaraStackViewModel* StackViewModel, FStoredInputMap& OutInputs)
	{
		if (!StackViewModel || !StackViewModel->GetRootEntry())
		{
			return;
		}

		TArray<UNiagaraStackModuleItem*> Modules;
		StackViewModel->GetRootEntry()->GetUnfilteredChildrenOfType(Modules, true);
		for (UNiagaraStackModuleItem* Module : Modules)
		{
			if (!Module)
			{
				continue;
			}

			TArray<UNiagaraStackFunctionInput*> Inputs;
			Module->GetParameterInputs(Inputs);
			for (UNiagaraStackFunctionInput* Input : Inputs)
			{
				if (!Input)
				{
					continue;
				}

				const FString Key = MakeInputKey(Module->GetDisplayName().ToString(), Input->GetDisplayName().ToString());
				if (!Key.IsEmpty())
				{
					if (TSharedPtr<const FStructOnScope> Value = Input->GetLocalValueStruct())
					{
						OutInputs.FindOrAdd(Key) = Value;
					}
				}
			}
		}
	}

	// 将参考 Stack 的本地值复制到目标 Stack，触发 Niagara 正常的输入更新流程。
	int32 ApplyStackInputs(UNiagaraStackViewModel* StackViewModel, const FStoredInputMap& SourceInputs)
	{
		if (!StackViewModel || !StackViewModel->GetRootEntry())
		{
			return 0;
		}

		int32 AppliedCount = 0;
		TArray<UNiagaraStackModuleItem*> Modules;
		StackViewModel->GetRootEntry()->GetUnfilteredChildrenOfType(Modules, true);
		for (UNiagaraStackModuleItem* Module : Modules)
		{
			if (!Module)
			{
				continue;
			}

			TArray<UNiagaraStackFunctionInput*> Inputs;
			Module->GetParameterInputs(Inputs);
			for (UNiagaraStackFunctionInput* Input : Inputs)
			{
				if (!Input)
				{
					continue;
				}

				const FString Key = MakeInputKey(Module->GetDisplayName().ToString(), Input->GetDisplayName().ToString());
				const TSharedPtr<const FStructOnScope>* SourceValue = SourceInputs.Find(Key);
				if (Key.IsEmpty() || !SourceValue || !SourceValue->IsValid())
				{
					continue;
				}

				const UScriptStruct* SourceStruct = Cast<UScriptStruct>((*SourceValue)->GetStruct());
				if (!SourceStruct)
				{
					continue;
				}

				TSharedRef<FStructOnScope> ValueCopy = MakeShared<FStructOnScope>(SourceStruct);
				SourceStruct->CopyScriptStruct(
					ValueCopy->GetStructMemory(),
					(*SourceValue)->GetStructMemory());
				Input->SetLocalValue(ValueCopy);
				++AppliedCount;
			}
		}
		return AppliedCount;
	}

	// 收集 System Stack 与所有 Emitter Stack 的生命周期参考值。
	FStoredInputMap CollectReferenceInputs(const TSharedRef<FNiagaraSystemViewModel>& ViewModel)
	{
		FStoredInputMap Inputs;
		CollectStackInputs(ViewModel->GetSystemStackViewModel(), Inputs);
		for (const TSharedRef<FNiagaraEmitterHandleViewModel>& EmitterViewModel : ViewModel->GetEmitterHandleViewModels())
		{
			CollectStackInputs(EmitterViewModel->GetEmitterStackViewModel(), Inputs);
		}
		return Inputs;
	}
}

bool UArenaNiagaraEditorLibrary::CopyLoopingLifecycleFromReference(
	UNiagaraSystem* TargetSystem,
	UNiagaraSystem* ReferenceSystem,
	FString& OutReport)
{
	if (!TargetSystem || !ReferenceSystem || TargetSystem == ReferenceSystem)
	{
		OutReport = TEXT("Target and reference Niagara Systems must both be valid and different.");
		return false;
	}

	TSharedRef<FNiagaraSystemViewModel> ReferenceViewModel =
		ArenaNiagaraEditor::CreateDataProcessingViewModel(*ReferenceSystem);
	const ArenaNiagaraEditor::FStoredInputMap ReferenceInputs =
		ArenaNiagaraEditor::CollectReferenceInputs(ReferenceViewModel);
	if (!ReferenceInputs.Contains(TEXT("System.LoopBehavior"))
		|| !ReferenceInputs.Contains(TEXT("Emitter.LifeCycleMode")))
	{
		OutReport = TEXT("Reference system does not expose Infinite System State and System-owned Emitter State values.");
		return false;
	}

	TargetSystem->Modify();
	TSharedRef<FNiagaraSystemViewModel> TargetViewModel =
		ArenaNiagaraEditor::CreateDataProcessingViewModel(*TargetSystem);

	int32 AppliedInputCount = ArenaNiagaraEditor::ApplyStackInputs(
		TargetViewModel->GetSystemStackViewModel(),
		ReferenceInputs);
	for (const TSharedRef<FNiagaraEmitterHandleViewModel>& EmitterViewModel : TargetViewModel->GetEmitterHandleViewModels())
	{
		AppliedInputCount += ArenaNiagaraEditor::ApplyStackInputs(
			EmitterViewModel->GetEmitterStackViewModel(),
			ReferenceInputs);
	}

	int32 LocalSpaceCount = 0;
	for (FNiagaraEmitterHandle& EmitterHandle : TargetSystem->GetEmitterHandles())
	{
		FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData();
		UNiagaraEmitter* Emitter = EmitterHandle.GetInstance().Emitter;
		if (EmitterData && Emitter)
		{
			Emitter->Modify();
			EmitterData->bLocalSpace = true;
			++LocalSpaceCount;
		}
	}

	if (AppliedInputCount <= 0)
	{
		OutReport = TEXT("No writable lifecycle inputs were found on the target Niagara System.");
		return false;
	}

	TargetSystem->ForceGraphToRecompileOnNextCheck();
	TargetSystem->RequestCompile(true);
	TargetSystem->WaitForCompilationComplete(false, true);
	TargetSystem->MarkPackageDirty();
	OutReport = FString::Printf(
		TEXT("Applied %d lifecycle inputs and enabled Local Space on %d emitters."),
		AppliedInputCount,
		LocalSpaceCount);
	return true;
}
