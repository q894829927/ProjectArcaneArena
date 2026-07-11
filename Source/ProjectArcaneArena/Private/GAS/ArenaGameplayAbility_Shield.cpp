#include "GAS/ArenaGameplayAbility_Shield.h"

#include "AbilitySystemComponent.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffect.h"

// 构造护盾技能，配置服务端执行、输入标签和激活阻断标签。
UArenaGameplayAbility_Shield::UArenaGameplayAbility_Shield()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InputTag = ArenaGameplayTags::Ability_Shield;

	SetAssetTags(FGameplayTagContainer(ArenaGameplayTags::Ability_Shield));
	ActivationBlockedTags.AddTag(ArenaGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(ArenaGameplayTags::State_Stunned);
	ActivationBlockedTags.AddTag(ArenaGameplayTags::Cooldown_Shield);
}

// 激活护盾技能，提交消耗/冷却后通过 GE 给自身添加护盾值。
void UArenaGameplayAbility_Shield::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid() || !ActorInfo->AbilitySystemComponent.IsValid() || !ShieldEffectClass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilitySystemComponent* SourceASC = ActorInfo->AbilitySystemComponent.Get();

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	// 先构造 GE_Shield spec，避免资产配置无效时仍然消耗 Energy 或进入冷却。
	const FGameplayEffectSpecHandle ShieldSpecHandle = SourceASC->MakeOutgoingSpec(
		ShieldEffectClass,
		GetAbilityLevel(Handle, ActorInfo),
		EffectContext);
	if (!ShieldSpecHandle.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 护盾数值只通过 GE 修改 AttributeSet，保持 Cost/Cooldown/Clamp 都在 GAS 流程中。
	SourceASC->ApplyGameplayEffectSpecToSelf(*ShieldSpecHandle.Data.Get());
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
