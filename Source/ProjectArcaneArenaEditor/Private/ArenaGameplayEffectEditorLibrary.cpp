#include "ArenaGameplayEffectEditorLibrary.h"

#include "GameplayEffect.h"

bool UArenaGameplayEffectEditorLibrary::SetScalableFloatModifierMagnitudes(
	UGameplayEffect* GameplayEffect,
	const TArray<int32>& ModifierIndices,
	const TArray<float>& Magnitudes,
	FString& OutReport)
{
	// 写入前完整验证全部索引和值，避免资产只更新一部分。
	if (!GameplayEffect)
	{
		OutReport = TEXT("GameplayEffect must be valid.");
		return false;
	}

	if (ModifierIndices.IsEmpty() || ModifierIndices.Num() != Magnitudes.Num())
	{
		OutReport = TEXT("ModifierIndices and Magnitudes must contain the same non-zero number of entries.");
		return false;
	}

	TSet<int32> UniqueIndices;
	for (int32 EntryIndex = 0; EntryIndex < ModifierIndices.Num(); ++EntryIndex)
	{
		const int32 ModifierIndex = ModifierIndices[EntryIndex];
		if (!GameplayEffect->Modifiers.IsValidIndex(ModifierIndex))
		{
			OutReport = FString::Printf(
				TEXT("Modifier index %d is outside the valid range [0, %d)."),
				ModifierIndex,
				GameplayEffect->Modifiers.Num());
			return false;
		}

		if (UniqueIndices.Contains(ModifierIndex))
		{
			OutReport = FString::Printf(TEXT("Modifier index %d was supplied more than once."), ModifierIndex);
			return false;
		}
		UniqueIndices.Add(ModifierIndex);

		if (!FMath::IsFinite(Magnitudes[EntryIndex]))
		{
			OutReport = FString::Printf(TEXT("Magnitude at entry %d is not finite."), EntryIndex);
			return false;
		}

		if (GameplayEffect->Modifiers[ModifierIndex].ModifierMagnitude.GetMagnitudeCalculationType()
			!= EGameplayEffectMagnitudeCalculation::ScalableFloat)
		{
			OutReport = FString::Printf(
				TEXT("Modifier index %d does not use a ScalableFloat magnitude."),
				ModifierIndex);
			return false;
		}
	}

	// 官方 C++ 构造函数可以安全替换 Python 无权编辑的 protected EditDefaultsOnly 字段。
	GameplayEffect->Modify();
	for (int32 EntryIndex = 0; EntryIndex < ModifierIndices.Num(); ++EntryIndex)
	{
		FGameplayModifierInfo& Modifier = GameplayEffect->Modifiers[ModifierIndices[EntryIndex]];
		Modifier.ModifierMagnitude =
			FGameplayEffectModifierMagnitude(FScalableFloat(Magnitudes[EntryIndex]));
	}

	GameplayEffect->PostEditChange();
	GameplayEffect->MarkPackageDirty();
	OutReport = FString::Printf(
		TEXT("Updated %d ScalableFloat GameplayEffect modifiers."),
		ModifierIndices.Num());
	return true;
}
