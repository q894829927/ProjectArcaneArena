#include "GAS/ArenaAbilitySystemComponent.h"

#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Character/ArenaPlayerCharacter.h"
#include "Core/ArenaPlayerState.h"
#include "Engine/World.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffect.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogArenaDamageEvents, Log, All);

// 构造项目自定义 ASC，后续集中扩展输入、标签和项目辅助函数。
UArenaAbilitySystemComponent::UArenaAbilitySystemComponent()
{
}

// 只在服务器确认完整 Commit 后派发事件，避免客户端预测、被动技能或敌人技能重复触发奖励。
void UArenaAbilitySystemComponent::NotifyAbilityCommit(UGameplayAbility* Ability)
{
	Super::NotifyAbilityCommit(Ability);

	const AArenaPlayerState* ArenaPlayerState = Cast<AArenaPlayerState>(GetOwnerActor());
	AArenaPlayerCharacter* PlayerAvatar = Cast<AArenaPlayerCharacter>(GetAvatarActor());
	if (!IsOwnerActorAuthoritative()
		|| !Ability
		|| !ArenaPlayerState
		|| !PlayerAvatar
		|| HasMatchingGameplayTag(ArenaGameplayTags::State_Dead))
	{
		return;
	}

	const FGameplayTagContainer& AbilityAssetTags = Ability->GetAssetTags();
	if (!AbilityAssetTags.HasTagExact(ArenaGameplayTags::Ability_Type_PlayerActive))
	{
		return;
	}

	FGameplayEventData EventPayload;
	EventPayload.EventTag = ArenaGameplayTags::Trigger_OnAbilityCast;
	EventPayload.Instigator = PlayerAvatar;
	EventPayload.Target = PlayerAvatar;
	EventPayload.OptionalObject = Ability;
	EventPayload.OptionalObject2 = Ability->GetCurrentSourceObject();
	EventPayload.EventMagnitude = 1.0f;
	GetOwnedGameplayTags(EventPayload.InstigatorTags);
	EventPayload.InstigatorTags.AppendTags(AbilityAssetTags);
	GetOwnedGameplayTags(EventPayload.TargetTags);

	// 事件发送给同一 PlayerState ASC，升级授予的服务器被动按技能分类标签自行筛选。
	HandleGameplayEvent(ArenaGameplayTags::Trigger_OnAbilityCast, &EventPayload);
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

// 将同一目标本 Tick 的伤害反馈排入一个网络批次，避免并发状态伤害继续触发第三个 Multicast。
void UArenaAbilitySystemComponent::QueueAuthoritativeGameplayCues(
	const FGameplayTagContainer& GameplayCueTags,
	const FGameplayCueParameters& GameplayCueParameters)
{
	if (!IsOwnerActorAuthoritative() || GameplayCueTags.IsEmpty())
	{
		return;
	}

	FArenaGameplayCueBatchItem& BatchItem = PendingGameplayCueBatch.Emplace_GetRef();
	BatchItem.GameplayCueTags = GameplayCueTags;
	BatchItem.CueParameters = GameplayCueParameters;

	if (GameplayCueBatchTimerHandle.IsValid())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		GameplayCueBatchTimerHandle = World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &UArenaAbilitySystemComponent::FlushPendingGameplayCueBatch));
	}
	else
	{
		FlushPendingGameplayCueBatch();
	}
}

// 复制前移动当前队列，保证 Multicast 本地执行期间新增的反馈会进入下一批而不是修改正在遍历的数据。
void UArenaAbilitySystemComponent::FlushPendingGameplayCueBatch()
{
	GameplayCueBatchTimerHandle.Invalidate();
	if (!IsOwnerActorAuthoritative() || PendingGameplayCueBatch.IsEmpty())
	{
		PendingGameplayCueBatch.Reset();
		return;
	}

	TArray<FArenaGameplayCueBatchItem> GameplayCueBatch = MoveTemp(PendingGameplayCueBatch);
	PendingGameplayCueBatch.Reset();
	ForceReplication();
	MulticastExecuteGameplayCueBatch(GameplayCueBatch);
}

// 每个客户端在本地执行标准 Cue 路由；批量 RPC 只替换传输层，不改变 Notify 标签或参数。
void UArenaAbilitySystemComponent::MulticastExecuteGameplayCueBatch_Implementation(
	const TArray<FArenaGameplayCueBatchItem>& GameplayCueBatch)
{
	for (const FArenaGameplayCueBatchItem& BatchItem : GameplayCueBatch)
	{
		for (const FGameplayTag& GameplayCueTag : BatchItem.GameplayCueTags)
		{
			InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::Executed, BatchItem.CueParameters);
		}
	}
}

// 由权威来源派发实际伤害、暴击与首次击杀结果事件。
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
	const bool bCriticalHit = DamageAssetTags.HasTagExact(ArenaGameplayTags::Damage_Critical);
	// 在任何伤害被动同步执行前锁定本次击杀结果，避免嵌套伤害改变原始事件判定。
	const bool bKilledTarget = !TargetTagsBeforeDamage.HasTag(ArenaGameplayTags::State_Dead)
		&& TargetAbilitySystemComponent->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead);

	// 事件发送给来源 ASC，自身拥有的被动 Ability 通过 AbilityTriggers 响应。
	HandleGameplayEvent(DamageEventTag, &EventPayload);
	if (bCriticalHit)
	{
		// OnCrit 沿用实际伤害和命中上下文，不再做第二次随机。
		EventPayload.EventTag = ArenaGameplayTags::Trigger_OnCrit;
		HandleGameplayEvent(ArenaGameplayTags::Trigger_OnCrit, &EventPayload);
	}
	if (bKilledTarget)
	{
		// OnKill 复用同一伤害上下文和命中前目标标签，供后续状态击杀协同可靠判断。
		EventPayload.EventTag = ArenaGameplayTags::Trigger_OnKill;
		HandleGameplayEvent(ArenaGameplayTags::Trigger_OnKill, &EventPayload);
	}
}

// 由权威受害者 ASC 派发破盾事件，事件数值只记录本次实际消耗的 Shield。
void UArenaAbilitySystemComponent::RouteAuthoritativeShieldBreakEvent(
	const FGameplayEffectSpec& DamageSpec,
	UAbilitySystemComponent* SourceAbilitySystemComponent,
	const FGameplayTagContainer& TargetTagsBeforeDamage,
	float AppliedShieldDamage)
{
	if (!IsOwnerActorAuthoritative()
		|| AppliedShieldDamage <= KINDA_SMALL_NUMBER
		|| HasMatchingGameplayTag(ArenaGameplayTags::State_Dead))
	{
		return;
	}

	FGameplayTagContainer DamageAssetTags;
	DamageSpec.GetAllAssetTags(DamageAssetTags);

	FGameplayEventData EventPayload;
	EventPayload.EventTag = ArenaGameplayTags::Trigger_OnShieldBreak;
	EventPayload.Instigator = SourceAbilitySystemComponent
		? SourceAbilitySystemComponent->GetAvatarActor()
		: DamageSpec.GetEffectContext().GetOriginalInstigator();
	EventPayload.Target = GetAvatarActor();
	EventPayload.OptionalObject = DamageSpec.Def.Get();
	EventPayload.OptionalObject2 = DamageSpec.GetEffectContext().GetSourceObject();
	EventPayload.ContextHandle = DamageSpec.GetEffectContext();
	EventPayload.EventMagnitude = AppliedShieldDamage;
	if (SourceAbilitySystemComponent)
	{
		SourceAbilitySystemComponent->GetOwnedGameplayTags(EventPayload.InstigatorTags);
	}
	EventPayload.InstigatorTags.AppendTags(DamageAssetTags);
	EventPayload.TargetTags = TargetTagsBeforeDamage;

	// 事件发送给受害者 ASC，使 Shield Build 被动归属于护盾拥有者而不是伤害来源。
	HandleGameplayEvent(ArenaGameplayTags::Trigger_OnShieldBreak, &EventPayload);
}
