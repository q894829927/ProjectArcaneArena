#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "GAS/ArenaDamageFeedbackTypes.h"
#include "ArenaAbilitySystemComponent.generated.h"

struct FGameplayEffectSpec;
class UGameplayAbility;

// 保存一段权威伤害反馈，网络批次只合并传输次数而不合并结算语义。
USTRUCT()
struct FArenaGameplayCueBatchItem
{
	GENERATED_BODY()

	UPROPERTY()
	FArenaDamageFeedbackData DamageFeedback;
};

UCLASS()
class PROJECTARCANEARENA_API UArenaAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UArenaAbilitySystemComponent();

	// 在 GAS 成功提交 Cost/Cooldown 后，服务器先记录主动技能统计，再路由玩家施放事件。
	virtual void NotifyAbilityCommit(UGameplayAbility* Ability) override;

	// 根据输入标签查找对应 AbilitySpec，并交给 GAS 标准激活流程处理。
	void AbilityInputTagPressed(const FGameplayTag& InputTag);

	// 在权威端暂存一段完整伤害反馈，并在下一 Tick 与该目标的其他结算一起发送。
	void QueueAuthoritativeDamageFeedback(const FArenaDamageFeedbackData& DamageFeedback);

	// 在服务端记录实际 Shield/Health 损失，再按伤害、暴击和首次击杀顺序路由事件。
	void RouteAuthoritativeDamageEvent(
		const FGameplayEffectSpec& DamageSpec,
		UAbilitySystemComponent* TargetAbilitySystemComponent,
		const FGameplayTagContainer& TargetTagsBeforeDamage,
		float AppliedShieldDamage,
		float AppliedHealthDamage);

	// 在服务端向护盾拥有者路由一次伤害驱动的破盾事件，供自身被动 Ability 响应。
	void RouteAuthoritativeShieldBreakEvent(
		const FGameplayEffectSpec& DamageSpec,
		UAbilitySystemComponent* SourceAbilitySystemComponent,
		const FGameplayTagContainer& TargetTagsBeforeDamage,
		float AppliedShieldDamage);

private:
	// 在下一 Tick 发送当前目标积累的视觉批次，并用独立可靠消息补充唯一结果音。
	void FlushPendingGameplayCueBatch();

	// 各端收到批次后逐项播放 Cue，再把同 Tick 角色反应汇总为一次表现。
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastExecuteGameplayCueBatch(const TArray<FArenaGameplayCueBatchItem>& GameplayCueBatch);

	// 可靠广播每 Tick 汇总后的唯一命中结果音，避免可丢失视觉批次导致听感断续。
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayDamageFeedbackSound(EArenaDamageFeedbackType FeedbackType);

	UPROPERTY(Transient)
	TArray<FArenaGameplayCueBatchItem> PendingGameplayCueBatch;

	FTimerHandle GameplayCueBatchTimerHandle;
};
