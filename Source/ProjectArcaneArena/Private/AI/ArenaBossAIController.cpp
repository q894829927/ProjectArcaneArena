#include "AI/ArenaBossAIController.h"

#include "AbilitySystemComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BrainComponent.h"
#include "Character/ArenaBossCharacter.h"
#include "Engine/World.h"
#include "GAS/ArenaGameplayTags.h"

DEFINE_LOG_CATEGORY(LogArenaBossAI);

const FName AArenaBossAIController::TargetActorKeyName(TEXT("TargetActor"));

// Boss Controller 不启用普通敌人的决策 Tick，寻路与朝向分别交给 BehaviorTree 和 CharacterMovement。
AArenaBossAIController::AArenaBossAIController()
{
	PrimaryActorTick.bCanEverTick = false;
	bAttachToPawn = true;
}

// 使用服务器世界时间判断开场缓冲，不创建额外 Timer，也不复制本地 AI 决策状态。
bool AArenaBossAIController::CanActivateBossAbilities() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() >= AbilityActivationAllowedTime;
}

// 只接受 Boss Pawn，并在完整配置与 Combat 阶段下启动服务器 BehaviorTree。
void AArenaBossAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (!HasAuthority())
	{
		return;
	}

	ControlledBoss = Cast<AArenaBossCharacter>(InPawn);
	if (!ControlledBoss.IsValid())
	{
		UE_LOG(LogArenaBossAI, Error, TEXT("Boss AIController %s possessed a non-Boss Pawn %s."),
			*GetNameSafe(this), *GetNameSafe(InPawn));
		StopBossLogic(TEXT("Invalid Boss pawn"));
		return;
	}

	// BehaviorTree 可以立即选敌和 Chase，但技能 Decorator 要等统一开场缓冲结束。
	AbilityActivationAllowedTime = GetWorld()
		? GetWorld()->GetTimeSeconds() + FMath::Max(InitialAbilityDelay, 0.0f)
		: 0.0f;
	BindBossDelegates();
	RefreshBossLogicState();
}

// 在 Pawn 脱离前停止所有异步 AI 与攻击状态，再释放委托和弱引用。
void AArenaBossAIController::OnUnPossess()
{
	StopBossLogic(TEXT("Boss unpossessed"));
	UnbindBossDelegates();
	ControlledBoss.Reset();
	AbilityActivationAllowedTime = 0.0f;
	Super::OnUnPossess();
}

// Controller 销毁时重复清理是安全的，可覆盖关卡切换未先触发 UnPossess 的路径。
void AArenaBossAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopBossLogic(TEXT("Boss controller EndPlay"));
	UnbindBossDelegates();
	Super::EndPlay(EndPlayReason);
}

// 只有服务器 Combat 且 Boss 未死亡/眩晕时运行树，其余状态统一停止并清理攻击。
void AArenaBossAIController::RefreshBossLogicState()
{
	AArenaBossCharacter* Boss = ControlledBoss.Get();
	const AArenaGameState* ArenaGameState = BoundGameState.Get();
	const bool bCanRunBossLogic = HasAuthority()
		&& Boss
		&& !Boss->IsDeadOrStunned()
		&& ArenaGameState
		&& ArenaGameState->GetGamePhase() == EArenaGamePhase::Combat;
	if (bCanRunBossLogic)
	{
		StartBossLogic();
	}
	else
	{
		StopBossLogic(TEXT("Boss cannot currently run combat logic"));
	}
}

// 依赖 BT 自带 BlackboardAsset 初始化 Brain；同一棵树已运行时不重复启动。
bool AArenaBossAIController::StartBossLogic()
{
	if (!BehaviorTreeAsset || !BehaviorTreeAsset->BlackboardAsset)
	{
		UE_LOG(LogArenaBossAI, Error, TEXT("Boss AIController %s is missing BehaviorTreeAsset or BlackboardAsset."),
			*GetNameSafe(this));
		return false;
	}

	if (const UBehaviorTreeComponent* BehaviorTreeComponent = Cast<UBehaviorTreeComponent>(GetBrainComponent()))
	{
		if (BehaviorTreeComponent->IsRunning() && BehaviorTreeComponent->GetCurrentTree() == BehaviorTreeAsset)
		{
			return true;
		}
	}

	if (!RunBehaviorTree(BehaviorTreeAsset))
	{
		UE_LOG(LogArenaBossAI, Error, TEXT("Boss AIController %s failed to run BehaviorTree %s."),
			*GetNameSafe(this), *GetNameSafe(BehaviorTreeAsset));
		return false;
	}

	return true;
}

// 对停止操作保持幂等，确保阶段结束、死亡和销毁路径都不会留下攻击或寻路。
void AArenaBossAIController::StopBossLogic(const FString& Reason)
{
	if (UBrainComponent* Brain = GetBrainComponent())
	{
		Brain->StopLogic(Reason);
	}
	StopMovement();
	ClearFocus(EAIFocusPriority::Gameplay);
	if (UBlackboardComponent* BlackboardComponent = GetBlackboardComponent())
	{
		BlackboardComponent->ClearValue(TargetActorKeyName);
	}
	if (AArenaBossCharacter* Boss = ControlledBoss.Get())
	{
		Boss->CancelPrimaryAttack();
		Boss->SetCombatTarget(nullptr);
	}
}

// 使用现有 ASC Tag Delegate 与 GameState 动态委托驱动即时停止，不依赖额外 Controller Tick。
void AArenaBossAIController::BindBossDelegates()
{
	UnbindBossDelegates();
	AArenaBossCharacter* Boss = ControlledBoss.Get();
	UAbilitySystemComponent* BossASC = Boss ? Boss->GetAbilitySystemComponent() : nullptr;
	if (BossASC)
	{
		DeadTagDelegateHandle = BossASC->RegisterGameplayTagEvent(
			ArenaGameplayTags::State_Dead,
			EGameplayTagEventType::NewOrRemoved).AddUObject(this, &AArenaBossAIController::HandleBossStateTagChanged);
		StunnedTagDelegateHandle = BossASC->RegisterGameplayTagEvent(
			ArenaGameplayTags::State_Stunned,
			EGameplayTagEventType::NewOrRemoved).AddUObject(this, &AArenaBossAIController::HandleBossStateTagChanged);
	}

	AArenaGameState* ArenaGameState = GetWorld() ? GetWorld()->GetGameState<AArenaGameState>() : nullptr;
	if (ArenaGameState)
	{
		BoundGameState = ArenaGameState;
		ArenaGameState->OnGamePhaseChanged.AddUniqueDynamic(this, &AArenaBossAIController::HandleGamePhaseChanged);
	}
}

// 解除时使用保存的弱引用，避免世界切换后重新查询到另一局 GameState。
void AArenaBossAIController::UnbindBossDelegates()
{
	if (AArenaBossCharacter* Boss = ControlledBoss.Get())
	{
		if (UAbilitySystemComponent* BossASC = Boss->GetAbilitySystemComponent())
		{
			if (DeadTagDelegateHandle.IsValid())
			{
				BossASC->RegisterGameplayTagEvent(ArenaGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved)
					.Remove(DeadTagDelegateHandle);
			}
			if (StunnedTagDelegateHandle.IsValid())
			{
				BossASC->RegisterGameplayTagEvent(ArenaGameplayTags::State_Stunned, EGameplayTagEventType::NewOrRemoved)
					.Remove(StunnedTagDelegateHandle);
			}
		}
	}
	DeadTagDelegateHandle.Reset();
	StunnedTagDelegateHandle.Reset();

	if (AArenaGameState* ArenaGameState = BoundGameState.Get())
	{
		ArenaGameState->OnGamePhaseChanged.RemoveDynamic(this, &AArenaBossAIController::HandleGamePhaseChanged);
	}
	BoundGameState.Reset();
}

// 标签出现或移除都重新计算运行资格，使 Stun 结束后可恢复同一 BehaviorTree。
void AArenaBossAIController::HandleBossStateTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	RefreshBossLogicState();
}

// 服务器阶段变化直接控制 Brain，避免 Victory/Defeat 后等待 Service 下一次 Tick。
void AArenaBossAIController::HandleGamePhaseChanged(EArenaGamePhase OldPhase, EArenaGamePhase NewPhase)
{
	RefreshBossLogicState();
}
