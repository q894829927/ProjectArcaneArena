#include "GAS/ArenaGameplayAbility_Shield.h"

#include "AbilitySystemComponent.h"
#include "Core/ArenaPlayerState.h"
#include "GAS/ArenaAbilityNetworkDebug.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffect.h"

// 构造本地预测护盾技能，资源、冷却和 Shield GE 使用同一 PredictionKey 自动确认或回滚。
UArenaGameplayAbility_Shield::UArenaGameplayAbility_Shield()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	NetworkAbilityId = EArenaNetworkAbilityId::Shield;
	InputTag = ArenaGameplayTags::Ability_Shield;

	SetAssetTags(FGameplayTagContainer(ArenaGameplayTags::Ability_Shield));
	ActivationBlockedTags.AddTag(ArenaGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(ArenaGameplayTags::State_Stunned);
	ActivationBlockedTags.AddTag(ArenaGameplayTags::Cooldown_Shield);
}

// 两端提交消耗/冷却并应用同一护盾 GE，服务器结果通过 PredictionKey 确认或回滚客户端预测。
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

	// OwnerOnly 升级堆叠在客户端和服务器分别计算同一预测值，最终仍由服务器确认。
	const AArenaPlayerState* ArenaPlayerState = Cast<AArenaPlayerState>(ActorInfo->OwnerActor.Get());
	const float ShieldUpgradeBonus = ArenaPlayerState
		? ArenaPlayerState->GetOwnedUpgradeNumericTotal(
			ArenaGameplayTags::Ability_Shield,
			FGameplayTag(),
			ArenaGameplayTags::Upgrade_Shield_Amount)
		: 0.0f;
	const float ShieldAmount = FMath::Max(
		FMath::FloorToFloat(BaseShieldAmount * (1.0f + ShieldUpgradeBonus)),
		0.0f);
	if (ShieldAmount <= KINDA_SMALL_NUMBER)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 先构造并填充 Shield spec，避免资产配置无效时仍然消耗 Energy 或进入冷却。
	const FGameplayEffectSpecHandle ShieldSpecHandle = SourceASC->MakeOutgoingSpec(
		ShieldEffectClass,
		GetAbilityLevel(Handle, ActorInfo),
		EffectContext);
	if (!ShieldSpecHandle.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	ShieldSpecHandle.Data->SetSetByCallerMagnitude(
		ArenaGameplayTags::SetByCaller_Shield_Amount,
		ShieldAmount);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 护盾数值只通过 SetByCaller GE 修改 AttributeSet，保持 Cost/Cooldown/Clamp 都在 GAS 流程中。
	SourceASC->ApplyGameplayEffectSpecToSelf(
		*ShieldSpecHandle.Data.Get(),
		ActivationInfo.GetActivationPredictionKey());
	if (ActorInfo->IsNetAuthority() && ArenaAbilityNetworkDebug::IsAuditEnabled())
	{
		UE_LOG(LogArenaAbilityNet, Log, TEXT("[%llu] Shield Key=%d Handle=%s Avatar=%s"),
			ArenaAbilityNetworkDebug::NextServerExecutionSequence(),
			ActivationInfo.GetActivationPredictionKey().Current,
			*Handle.ToString(),
			*GetNameSafe(ActorInfo->AvatarActor.Get()));
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
