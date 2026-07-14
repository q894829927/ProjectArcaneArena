#include "GAS/ArenaAbilitySystemComponent.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffect.h"

DEFINE_LOG_CATEGORY_STATIC(LogArenaDamageEvents, Log, All);

// 构造项目自定义 ASC，后续集中扩展输入、标签和项目辅助函数。
UArenaAbilitySystemComponent::UArenaAbilitySystemComponent()
{
}

// 根据输入 GameplayTag 查找匹配 AbilitySpec，并尝试激活对应技能。
void UArenaAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	// 当前输入模型按 AbilitySpec 的动态输入标签路由，后续可扩展为 Pressed/Held/Released。
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (!AbilitySpec.Ability || !AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			continue;
		}

		TryActivateAbility(AbilitySpec.Handle);
	}
}

// 仅由权威来源发送一次事件；载荷保留命中前目标标签，供击杀与状态组合被动可靠判断。
void UArenaAbilitySystemComponent::RouteAuthoritativeDamageEvent(
	const FGameplayEffectSpec& DamageSpec,
	UAbilitySystemComponent* TargetAbilitySystemComponent,
	const FGameplayTagContainer& TargetTagsBeforeDamage,
	float AppliedDamage)
{
	if (!IsOwnerActorAuthoritative() || !TargetAbilitySystemComponent || AppliedDamage <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	FGameplayTagContainer DamageAssetTags;
	DamageSpec.GetAllAssetTags(DamageAssetTags);

	const bool bPhysicalDamage = DamageAssetTags.HasTagExact(ArenaGameplayTags::Damage_Physical);
	const bool bFireDamage = DamageAssetTags.HasTagExact(ArenaGameplayTags::Damage_Fire);
	const bool bLightningDamage = DamageAssetTags.HasTagExact(ArenaGameplayTags::Damage_Lightning);
	const int32 PrimaryDamageTypeCount = static_cast<int32>(bPhysicalDamage)
		+ static_cast<int32>(bFireDamage)
		+ static_cast<int32>(bLightningDamage);

	FGameplayTag DamageEventTag = ArenaGameplayTags::Trigger_OnDamageDealt;
	if (PrimaryDamageTypeCount == 1)
	{
		DamageEventTag = bPhysicalDamage
			? ArenaGameplayTags::Trigger_OnDamageDealt_Physical
			: (bFireDamage
				? ArenaGameplayTags::Trigger_OnDamageDealt_Fire
				: ArenaGameplayTags::Trigger_OnDamageDealt_Lightning);
	}
	else
	{
		UE_LOG(LogArenaDamageEvents, Warning,
			TEXT("Damage event from %s has %d primary damage type tags; routing only the root event."),
			*GetNameSafe(GetAvatarActor()),
			PrimaryDamageTypeCount);
	}

	FGameplayEventData EventPayload;
	EventPayload.EventTag = DamageEventTag;
	EventPayload.Instigator = GetAvatarActor();
	EventPayload.Target = TargetAbilitySystemComponent->GetAvatarActor();
	EventPayload.OptionalObject = DamageSpec.Def.Get();
	EventPayload.OptionalObject2 = DamageSpec.GetEffectContext().GetSourceObject();
	EventPayload.ContextHandle = DamageSpec.GetEffectContext();
	EventPayload.EventMagnitude = AppliedDamage;
	GetOwnedGameplayTags(EventPayload.InstigatorTags);
	EventPayload.InstigatorTags.AppendTags(DamageAssetTags);
	EventPayload.TargetTags = TargetTagsBeforeDamage;

	// 事件发送给来源 ASC，自身拥有的被动 Ability 通过 AbilityTriggers 响应。
	HandleGameplayEvent(DamageEventTag, &EventPayload);
}
