#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "ArenaAbilitySystemComponent.generated.h"

struct FGameplayEffectSpec;
class UGameplayAbility;

// 保存一段权威伤害反馈的独立参数，网络批次只合并传输次数而不合并表现语义。
USTRUCT()
struct FArenaGameplayCueBatchItem
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTagContainer GameplayCueTags;

	UPROPERTY()
	FGameplayCueParameters CueParameters;
};

UCLASS()
class PROJECTARCANEARENA_API UArenaAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UArenaAbilitySystemComponent();

	// 在 GAS 成功提交 Cost/Cooldown 后，由服务器统一路由玩家主动技能施放事件。
	virtual void NotifyAbilityCommit(UGameplayAbility* Ability) override;

	// 根据输入标签查找对应 AbilitySpec，并交给 GAS 标准激活流程处理。
	void AbilityInputTagPressed(const FGameplayTag& InputTag);

	// 在权威端暂存同次伤害的多个 Cue，并在下一 Tick 与该目标的其他伤害反馈一起发送。
	void QueueAuthoritativeGameplayCues(
		const FGameplayTagContainer& GameplayCueTags,
		const FGameplayCueParameters& GameplayCueParameters);

	// 在服务端按伤害、暴击、首次击杀顺序向来源 ASC 路由 GameplayEvent。
	void RouteAuthoritativeDamageEvent(
		const FGameplayEffectSpec& DamageSpec,
		UAbilitySystemComponent* TargetAbilitySystemComponent,
		const FGameplayTagContainer& TargetTagsBeforeDamage,
		float AppliedDamage);

	// 在服务端向护盾拥有者路由一次伤害驱动的破盾事件，供自身被动 Ability 响应。
	void RouteAuthoritativeShieldBreakEvent(
		const FGameplayEffectSpec& DamageSpec,
		UAbilitySystemComponent* SourceAbilitySystemComponent,
		const FGameplayTagContainer& TargetTagsBeforeDamage,
		float AppliedShieldDamage);

private:
	// 在下一 Tick 用一个不可靠 Multicast 发送当前目标积累的全部瞬时伤害表现。
	void FlushPendingGameplayCueBatch();

	// 各端收到批次后按原始顺序逐项、逐标签进入标准 GameplayCue Notify 路由。
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastExecuteGameplayCueBatch(const TArray<FArenaGameplayCueBatchItem>& GameplayCueBatch);

	UPROPERTY(Transient)
	TArray<FArenaGameplayCueBatchItem> PendingGameplayCueBatch;

	FTimerHandle GameplayCueBatchTimerHandle;
};
