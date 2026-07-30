#include "Item/ArenaItemDataAsset.h"

#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffect.h"
#include "Item/ArenaInventoryPickupActor.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

// 统一验证进入世界 Pickup 和背包 Model 所需的身份、堆叠与使用事务配置。
bool UArenaItemDataAsset::IsRuntimeDefinitionValid(FText* OutError) const
{
	if (!ItemTag.IsValid())
	{
		if (OutError)
		{
			*OutError = NSLOCTEXT(
				"ArenaInventory",
				"InvalidItemTag",
				"ItemTag must be a valid unique GameplayTag.");
		}
		return false;
	}
	if (MaxStackSize < 1)
	{
		if (OutError)
		{
			*OutError = NSLOCTEXT(
				"ArenaInventory",
				"InvalidMaxStackSize",
				"MaxStackSize must be at least one.");
		}
		return false;
	}
	return IsUseConfigurationValid(OutError);
}

// 统一验证服务器使用物品所依赖的恢复和共享冷却配置，避免错误 GE 产生无法回滚的延迟效果。
bool UArenaItemDataAsset::IsUseConfigurationValid(FText* OutError) const
{
	const auto SetError = [OutError](const FText& ErrorText)
	{
		if (OutError)
		{
			*OutError = ErrorText;
		}
	};

	if (UseMagnitude <= KINDA_SMALL_NUMBER)
	{
		SetError(NSLOCTEXT("ArenaInventory", "InvalidUseMagnitude", "UseMagnitude must be greater than zero."));
		return false;
	}

	const bool bUsesSupportedResource =
		SetByCallerMagnitudeTag == ArenaGameplayTags::SetByCaller_Recovery_Health
		|| SetByCallerMagnitudeTag == ArenaGameplayTags::SetByCaller_Recovery_Energy;
	if (!bUsesSupportedResource)
	{
		SetError(NSLOCTEXT(
			"ArenaInventory",
			"UnsupportedRecoveryRoute",
			"SetByCallerMagnitudeTag must use the supported Health or Energy recovery route."));
		return false;
	}

	const UGameplayEffect* UseEffect = UseGameplayEffectClass
		? UseGameplayEffectClass.GetDefaultObject()
		: nullptr;
	if (!UseEffect)
	{
		SetError(NSLOCTEXT("ArenaInventory", "MissingUseEffect", "UseGameplayEffectClass is required."));
		return false;
	}
	if (UseEffect->DurationPolicy != EGameplayEffectDurationType::Instant)
	{
		SetError(NSLOCTEXT(
			"ArenaInventory",
			"UseEffectMustBeInstant",
			"UseGameplayEffectClass must be Instant so inventory consumption can validate the restored value atomically."));
		return false;
	}

	const UGameplayEffect* CooldownEffect = CooldownGameplayEffectClass
		? CooldownGameplayEffectClass.GetDefaultObject()
		: nullptr;
	if (!CooldownEffect)
	{
		SetError(NSLOCTEXT(
			"ArenaInventory",
			"MissingConsumableCooldown",
			"CooldownGameplayEffectClass is required."));
		return false;
	}
	if (CooldownEffect->DurationPolicy != EGameplayEffectDurationType::HasDuration)
	{
		SetError(NSLOCTEXT(
			"ArenaInventory",
			"ConsumableCooldownMustHaveDuration",
			"CooldownGameplayEffectClass must use HasDuration."));
		return false;
	}
	if (!CooldownEffect->GetGrantedTags().HasTagExact(ArenaGameplayTags::Cooldown_Item_Consumable))
	{
		SetError(NSLOCTEXT(
			"ArenaInventory",
			"MissingConsumableCooldownTag",
			"CooldownGameplayEffectClass must grant Cooldown.Item.Consumable."));
		return false;
	}

	if (OutError)
	{
		*OutError = FText::GetEmpty();
	}
	return true;
}

#if WITH_EDITOR

// 验证消耗品身份、恢复 GE、共享冷却和基础展示配置，避免错误资产进入运行时事务。
EDataValidationResult UArenaItemDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	bool bIsValid = Result != EDataValidationResult::Invalid;
	const auto AddValidationError = [&Context, &bIsValid](const FText& ErrorText)
	{
		Context.AddError(ErrorText);
		bIsValid = false;
	};

	if (DisplayName.IsEmpty())
	{
		AddValidationError(NSLOCTEXT("ArenaInventory", "EmptyItemDisplayName", "DisplayName must not be empty."));
	}
	FText RuntimeDefinitionError;
	if (!IsRuntimeDefinitionValid(&RuntimeDefinitionError))
	{
		AddValidationError(RuntimeDefinitionError);
	}

	if (Icon.IsNull())
	{
		Context.AddWarning(NSLOCTEXT(
			"ArenaInventory",
			"MissingItemIcon",
			"Icon is not configured; the inventory slot will use its empty presentation."));
	}
	if (!WorldPickupClass)
	{
		Context.AddWarning(NSLOCTEXT(
			"ArenaInventory",
			"MissingWorldPickupClass",
			"WorldPickupClass is not configured; dropped items will use the native fallback Pickup."));
	}
	if (!WorldMesh.IsNull()
		&& (WorldMeshRelativeScale.ContainsNaN()
			|| WorldMeshRelativeScale.GetAbsMin() <= KINDA_SMALL_NUMBER))
	{
		AddValidationError(NSLOCTEXT(
			"ArenaInventory",
			"InvalidWorldMeshScale",
			"WorldMeshRelativeScale must contain finite non-zero components when WorldMesh is configured."));
	}

	return bIsValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}

#endif
