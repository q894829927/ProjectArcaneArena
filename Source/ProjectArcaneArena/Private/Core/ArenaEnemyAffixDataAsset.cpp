#include "Core/ArenaEnemyAffixDataAsset.h"

#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffect.h"
#include "Misc/DataValidation.h"

namespace
{
	// 统一回填运行时校验错误，调用方可选择只读取布尔结果。
	bool FailAffixValidation(FText* OutError, const FText& Error)
	{
		if (OutError)
		{
			*OutError = Error;
		}
		return false;
	}
}

// 校验运行时所需的唯一身份、表现标签和对应行为配置。
bool UArenaEnemyAffixDataAsset::IsRuntimeDefinitionValid(FText* OutError) const
{
	if (AffixID.IsNone() || DisplayName.IsEmpty() || Description.IsEmpty())
	{
		return FailAffixValidation(OutError, NSLOCTEXT("ArenaElite", "MissingIdentity", "AffixID, DisplayName, and Description must be configured."));
	}
	if (!FMath::IsFinite(AccentColor.R) || !FMath::IsFinite(AccentColor.G)
		|| !FMath::IsFinite(AccentColor.B) || !FMath::IsFinite(AccentColor.A)
		|| AccentColor.R < 0.0f || AccentColor.G < 0.0f || AccentColor.B < 0.0f || AccentColor.A <= 0.0f)
	{
		return FailAffixValidation(OutError, NSLOCTEXT("ArenaElite", "InvalidAccentColor", "AccentColor must contain finite non-negative RGB values and positive alpha."));
	}
	if (!AffixTag.IsValid() || !AffixTag.ToString().StartsWith(TEXT("Enemy.Affix.")))
	{
		return FailAffixValidation(OutError, NSLOCTEXT("ArenaElite", "InvalidAffixTag", "AffixTag must be under Enemy.Affix."));
	}
	if (!ActiveGameplayCueTag.IsValid() || !TriggerGameplayCueTag.IsValid())
	{
		return FailAffixValidation(OutError, NSLOCTEXT("ArenaElite", "MissingCueTags", "Active and trigger GameplayCue tags must be configured."));
	}

	switch (Behavior)
	{
	case EArenaEnemyAffixBehavior::Volatile:
		if (AffixTag != ArenaGameplayTags::Enemy_Affix_Volatile
			|| ActiveGameplayCueTag != ArenaGameplayTags::GameplayCue_Enemy_Affix_Volatile_Active
			|| TriggerGameplayCueTag != ArenaGameplayTags::GameplayCue_Enemy_Affix_Volatile_Explode
			|| !Volatile.DamageEffectClass || !Volatile.TelegraphGameplayCueTag.IsValid()
			|| Volatile.TelegraphGameplayCueTag != ArenaGameplayTags::GameplayCue_Enemy_Affix_Volatile_Telegraph
			|| !FMath::IsFinite(Volatile.TelegraphDuration) || Volatile.TelegraphDuration < 0.0f
			|| !FMath::IsFinite(Volatile.ExplosionRadius) || Volatile.ExplosionRadius <= 0.0f
			|| !FMath::IsFinite(Volatile.BaseDamage) || Volatile.BaseDamage <= 0.0f)
		{
			return FailAffixValidation(OutError, NSLOCTEXT("ArenaElite", "InvalidVolatile", "Volatile requires valid damage GE, telegraph Cue, duration, radius, and damage."));
		}
		break;
	case EArenaEnemyAffixBehavior::ArcaneWarden:
		if (AffixTag != ArenaGameplayTags::Enemy_Affix_ArcaneWarden
			|| ActiveGameplayCueTag != ArenaGameplayTags::GameplayCue_Enemy_Affix_ArcaneWarden_Active
			|| TriggerGameplayCueTag != ArenaGameplayTags::GameplayCue_Enemy_Affix_ArcaneWarden_Pulse
			|| !ArcaneWarden.ShieldEffectClass
			|| !FMath::IsFinite(ArcaneWarden.InitialDelay) || ArcaneWarden.InitialDelay < 0.0f
			|| !FMath::IsFinite(ArcaneWarden.PulseInterval) || ArcaneWarden.PulseInterval <= 0.0f
			|| !FMath::IsFinite(ArcaneWarden.Radius) || ArcaneWarden.Radius <= 0.0f
			|| !FMath::IsFinite(ArcaneWarden.ShieldCap) || ArcaneWarden.ShieldCap <= 0.0f)
		{
			return FailAffixValidation(OutError, NSLOCTEXT("ArenaElite", "InvalidWarden", "ArcaneWarden requires valid shield GE, delays, radius, and cap."));
		}
		break;
	case EArenaEnemyAffixBehavior::Frenzy:
		if (AffixTag != ArenaGameplayTags::Enemy_Affix_Frenzy
			|| ActiveGameplayCueTag != ArenaGameplayTags::GameplayCue_Enemy_Affix_Frenzy_Active
			|| TriggerGameplayCueTag != ArenaGameplayTags::GameplayCue_Enemy_Affix_Frenzy_Trigger
			|| !Frenzy.FrenzyEffectClass || !FMath::IsFinite(Frenzy.HealthThreshold)
			|| Frenzy.HealthThreshold <= 0.0f || Frenzy.HealthThreshold >= 1.0f)
		{
			return FailAffixValidation(OutError, NSLOCTEXT("ArenaElite", "InvalidFrenzy", "Frenzy requires a valid GE and a threshold strictly between zero and one."));
		}
		break;
	default:
		return FailAffixValidation(OutError, NSLOCTEXT("ArenaElite", "UnknownBehavior", "Affix behavior is unsupported."));
	}

	return true;
}

#if WITH_EDITOR

// 将运行时校验结果接入编辑器 Content Validation。
EDataValidationResult UArenaEnemyAffixDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult SuperResult = Super::IsDataValid(Context);
	FText Error;
	if (SuperResult == EDataValidationResult::Invalid || !IsRuntimeDefinitionValid(&Error))
	{
		if (!Error.IsEmpty())
		{
			Context.AddError(Error);
		}
		return EDataValidationResult::Invalid;
	}
	return EDataValidationResult::Valid;
}

#endif
