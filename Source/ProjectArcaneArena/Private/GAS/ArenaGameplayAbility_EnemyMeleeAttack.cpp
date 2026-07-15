#include "GAS/ArenaGameplayAbility_EnemyMeleeAttack.h"

#include "AbilitySystemComponent.h"
#include "Character/ArenaEnemyCharacter.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffect.h"

// 初始化近战攻击标签；公共服务器攻击生命周期由 EnemyAttackBase 负责。
UArenaGameplayAbility_EnemyMeleeAttack::UArenaGameplayAbility_EnemyMeleeAttack()
{
	SetAssetTags(FGameplayTagContainer(ArenaGameplayTags::Ability_Enemy_MeleeAttack));
	ActivationBlockedTags.AddTag(ArenaGameplayTags::Cooldown_Enemy_MeleeAttack);
}

// 近战攻击必须同时拥有动画与 GE_Damage，缺少配置时在 Commit 前安全取消。
bool UArenaGameplayAbility_EnemyMeleeAttack::HasRequiredAttackConfiguration() const
{
	return Super::HasRequiredAttackConfiguration() && DamageEffectClass != nullptr;
}

// 使用释放时的最新属性构造物理伤害 Spec，每次攻击窗口最多应用一次。
void UArenaGameplayAbility_EnemyMeleeAttack::ExecuteAttack(
	AArenaEnemyCharacter* SourceEnemy,
	AActor* TargetActor,
	UAbilitySystemComponent* SourceASC,
	UAbilitySystemComponent* TargetASC)
{
	if (!SourceEnemy || !TargetActor || !SourceASC || !TargetASC || !DamageEffectClass)
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);
	EffectContext.AddInstigator(SourceEnemy, SourceEnemy);
	FGameplayEffectSpecHandle DamageSpecHandle = SourceASC->MakeOutgoingSpec(
		DamageEffectClass,
		GetAbilityLevel(),
		EffectContext);
	if (!DamageSpecHandle.IsValid())
	{
		return;
	}

	FGameplayEffectSpec* DamageSpec = DamageSpecHandle.Data.Get();
	DamageSpec->SetSetByCallerMagnitude(ArenaGameplayTags::SetByCaller_Damage_Base, BaseDamage);
	DamageSpec->SetSetByCallerMagnitude(ArenaGameplayTags::SetByCaller_Damage_SkillMultiplier, SkillMultiplier);
	DamageSpec->AddDynamicAssetTag(ArenaGameplayTags::Damage_Physical);
	SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpec, TargetASC);
}

// 返回既有近战激活 Cue，保持蓝图表现和网络观察行为兼容。
FGameplayTag UArenaGameplayAbility_EnemyMeleeAttack::GetAttackActivationCueTag() const
{
	return ArenaGameplayTags::GameplayCue_Ability_EnemyMelee_Activate;
}
