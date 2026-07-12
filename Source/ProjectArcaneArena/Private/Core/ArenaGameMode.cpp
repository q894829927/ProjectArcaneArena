#include "Core/ArenaGameMode.h"

#include "Character/ArenaPlayerCharacter.h"
#include "Core/ArenaGameState.h"
#include "Core/ArenaPlayerController.h"
#include "Core/ArenaPlayerState.h"
#include "Core/ArenaUpgradeDataAsset.h"
#include "Core/ArenaWaveDataAsset.h"
#include "Core/ArenaWaveManager.h"
#include "GAS/ArenaAbilitySystemComponent.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"

DEFINE_LOG_CATEGORY_STATIC(LogArenaUpgrades, Log, All);

// 构造游戏模式，指定项目默认的 GameState、Controller、PlayerState 和 Pawn。
AArenaGameMode::AArenaGameMode()
{
	GameStateClass = AArenaGameState::StaticClass();
	PlayerControllerClass = AArenaPlayerController::StaticClass();
	PlayerStateClass = AArenaPlayerState::StaticClass();
	DefaultPawnClass = AArenaPlayerCharacter::StaticClass();
	WaveManagerClass = AArenaWaveManager::StaticClass();
}

// 服务器创建 WaveManager、接入正式升级阶段，并在配置 WaveData 后启动第一波。
void AArenaGameMode::BeginPlay()
{
	Super::BeginPlay();
	UpgradeRandomStream.Initialize(UpgradeRandomSeed);
	if (!HasAuthority() || !WaveManagerClass)
	{
		return;
	}

	WaveManager = GetWorld()->SpawnActor<AArenaWaveManager>(WaveManagerClass);
	if (!WaveManager)
	{
		return;
	}

	WaveManager->SetUpgradeSystemEnabled(true);
	WaveManager->OnUpgradePhaseStarted.AddUObject(this, &AArenaGameMode::HandleUpgradePhaseStarted);
	WaveManager->Initialize(WaveData);
	if (WaveData)
	{
		GetWorldTimerManager().SetTimer(InitialWaveTimerHandle, this, &AArenaGameMode::StartNextWave, InitialWaveDelay, false);
	}
}

// 玩家在 Upgrade 阶段加入时为其补发独立候选，避免中途连接无法完成全员选择。
void AArenaGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	const AArenaGameState* ArenaGameState = GetGameState<AArenaGameState>();
	if (HasAuthority() && ArenaGameState && ArenaGameState->GetGamePhase() == EArenaGamePhase::Upgrade)
	{
		PrepareUpgradeChoicesForPlayer(NewPlayer ? NewPlayer->GetPlayerState<AArenaPlayerState>() : nullptr);
	}
}

// 玩家离开后重新检查剩余参与者的选择状态，避免断线玩家永久阻塞下一波。
void AArenaGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
	TryAdvanceAfterUpgradeSelections();
}

// 提供给后续升级选择和当前手动测试的服务器波次推进入口。
void AArenaGameMode::StartNextWave()
{
	if (HasAuthority() && WaveManager)
	{
		WaveManager->StartNextWave();
	}
}

// 为所有 PlayerState 生成独立候选；候选只复制给拥有者，服务器保留同一份用于选择校验。
void AArenaGameMode::HandleUpgradePhaseStarted()
{
	AArenaGameState* ArenaGameState = GetGameState<AArenaGameState>();
	if (!HasAuthority() || !ArenaGameState || ArenaGameState->GetGamePhase() != EArenaGamePhase::Upgrade)
	{
		return;
	}

	for (APlayerState* PlayerState : ArenaGameState->PlayerArray)
	{
		PrepareUpgradeChoicesForPlayer(Cast<AArenaPlayerState>(PlayerState));
	}

	TryAdvanceAfterUpgradeSelections();
}

// 从配置池中过滤并随机抽取该玩家当前可选的升级，空池时保留 Upgrade 供排错。
void AArenaGameMode::PrepareUpgradeChoicesForPlayer(AArenaPlayerState* ArenaPlayerState)
{
	if (!HasAuthority() || !ArenaPlayerState)
	{
		return;
	}

	TArray<UArenaUpgradeDataAsset*> EligibleUpgrades;
	TSet<FName> SeenUpgradeIDs;
	for (UArenaUpgradeDataAsset* Upgrade : UpgradePool)
	{
		if (!IsUpgradeEligible(ArenaPlayerState, Upgrade) || SeenUpgradeIDs.Contains(Upgrade->UpgradeID))
		{
			continue;
		}
		SeenUpgradeIDs.Add(Upgrade->UpgradeID);
		EligibleUpgrades.Add(Upgrade);
	}

	TArray<UArenaUpgradeDataAsset*> Choices;
	const int32 DesiredChoiceCount = FMath::Clamp(UpgradeChoiceCount, 1, 3);
	while (!EligibleUpgrades.IsEmpty() && Choices.Num() < DesiredChoiceCount)
	{
		const int32 ChosenIndex = UpgradeRandomStream.RandRange(0, EligibleUpgrades.Num() - 1);
		Choices.Add(EligibleUpgrades[ChosenIndex]);
		EligibleUpgrades.RemoveAtSwap(ChosenIndex);
	}

	ArenaPlayerState->BeginUpgradeSelection(Choices);
	if (Choices.IsEmpty())
	{
		UE_LOG(LogArenaUpgrades, Error, TEXT("Player %s has no eligible upgrade choices; Upgrade phase will wait for valid configuration."), *GetNameSafe(ArenaPlayerState));
	}
}

// 按唯一 ID、Required/Blocked Tags、可叠加性和最大层数验证候选资格。
bool AArenaGameMode::IsUpgradeEligible(const AArenaPlayerState* ArenaPlayerState, const UArenaUpgradeDataAsset* Upgrade) const
{
	if (!ArenaPlayerState || !Upgrade || Upgrade->UpgradeID.IsNone())
	{
		return false;
	}

	const UArenaAbilitySystemComponent* ASC = ArenaPlayerState->GetArenaAbilitySystemComponent();
	if (!ASC || !ASC->HasAllMatchingGameplayTags(Upgrade->RequiredTags) || ASC->HasAnyMatchingGameplayTags(Upgrade->BlockedTags))
	{
		return false;
	}

	const int32 CurrentStacks = ArenaPlayerState->GetUpgradeStackCount(Upgrade->UpgradeID);
	if ((!Upgrade->bStackable && CurrentStacks > 0) || CurrentStacks >= FMath::Max(Upgrade->MaxStacks, 1))
	{
		return false;
	}

	return Upgrade->GrantedGameplayEffect || Upgrade->GrantedAbility || !Upgrade->UpgradeTags.IsEmpty();
}

// 在服务器通过 GAS 应用升级 GE、首次授予 Ability，并同步永久 Build Tags。
bool AArenaGameMode::ApplyUpgrade(AArenaPlayerState* ArenaPlayerState, const UArenaUpgradeDataAsset* Upgrade) const
{
	UArenaAbilitySystemComponent* ASC = ArenaPlayerState ? ArenaPlayerState->GetArenaAbilitySystemComponent() : nullptr;
	if (!ASC || !Upgrade)
	{
		return false;
	}

	const bool bFirstStack = ArenaPlayerState->GetUpgradeStackCount(Upgrade->UpgradeID) == 0;
	bool bAppliedAnything = false;
	if (Upgrade->GrantedGameplayEffect)
	{
		FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
		EffectContext.AddSourceObject(const_cast<UArenaUpgradeDataAsset*>(Upgrade));
		const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(Upgrade->GrantedGameplayEffect, 1.0f, EffectContext);
		if (SpecHandle.IsValid())
		{
			ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
			bAppliedAnything = true;
		}
	}

	if (bFirstStack && Upgrade->GrantedAbility && !ASC->FindAbilitySpecFromClass(Upgrade->GrantedAbility))
	{
		ASC->GiveAbility(FGameplayAbilitySpec(Upgrade->GrantedAbility, 1));
		bAppliedAnything = true;
	}

	if (bFirstStack && !Upgrade->UpgradeTags.IsEmpty())
	{
		ASC->AddReplicatedLooseGameplayTags(Upgrade->UpgradeTags);
		bAppliedAnything = true;
	}

	return bAppliedAnything;
}

// 重新验证客户端提交的候选 ID，成功后记录层数并检查是否可以推进波次。
void AArenaGameMode::SubmitUpgradeSelection(AArenaPlayerController* RequestingController, FName UpgradeID)
{
	AArenaGameState* ArenaGameState = GetGameState<AArenaGameState>();
	AArenaPlayerState* ArenaPlayerState = RequestingController ? RequestingController->GetPlayerState<AArenaPlayerState>() : nullptr;
	if (!HasAuthority() || !ArenaGameState || ArenaGameState->GetGamePhase() != EArenaGamePhase::Upgrade
		|| !ArenaPlayerState || ArenaPlayerState->HasSelectedUpgrade())
	{
		return;
	}

	const TArray<UArenaUpgradeDataAsset*> Candidates = ArenaPlayerState->GetUpgradeCandidates();
	UArenaUpgradeDataAsset* SelectedUpgrade = nullptr;
	for (UArenaUpgradeDataAsset* Candidate : Candidates)
	{
		if (Candidate && Candidate->UpgradeID == UpgradeID)
		{
			SelectedUpgrade = Candidate;
			break;
		}
	}
	if (!SelectedUpgrade || !IsUpgradeEligible(ArenaPlayerState, SelectedUpgrade) || !ApplyUpgrade(ArenaPlayerState, SelectedUpgrade))
	{
		UE_LOG(LogArenaUpgrades, Warning, TEXT("Rejected upgrade selection '%s' from %s."), *UpgradeID.ToString(), *GetNameSafe(ArenaPlayerState));
		return;
	}

	ArenaPlayerState->CompleteUpgradeSelection(UpgradeID);
	UE_LOG(LogArenaUpgrades, Log, TEXT("Player %s selected upgrade %s (stack %d)."),
		*GetNameSafe(ArenaPlayerState),
		*UpgradeID.ToString(),
		ArenaPlayerState->GetUpgradeStackCount(UpgradeID));
	TryAdvanceAfterUpgradeSelections();
}

// 检查所有有效 PlayerState 是否完成本轮选择，断线玩家不参与等待。
bool AArenaGameMode::HaveAllPlayersCompletedUpgradeSelection() const
{
	const AArenaGameState* ArenaGameState = GetGameState<AArenaGameState>();
	bool bFoundPlayer = false;
	if (!ArenaGameState)
	{
		return false;
	}

	for (APlayerState* PlayerState : ArenaGameState->PlayerArray)
	{
		const AArenaPlayerState* ArenaPlayerState = Cast<AArenaPlayerState>(PlayerState);
		if (!ArenaPlayerState)
		{
			continue;
		}
		bFoundPlayer = true;
		if (!ArenaPlayerState->HasSelectedUpgrade())
		{
			return false;
		}
	}
	return bFoundPlayer;
}

// 仅在 Upgrade 阶段且全员完成选择后调用服务器波次入口。
void AArenaGameMode::TryAdvanceAfterUpgradeSelections()
{
	const AArenaGameState* ArenaGameState = GetGameState<AArenaGameState>();
	if (HasAuthority() && ArenaGameState && ArenaGameState->GetGamePhase() == EArenaGamePhase::Upgrade
		&& HaveAllPlayersCompletedUpgradeSelection())
	{
		StartNextWave();
	}
}

// 检查 PlayerState ASC 的长期死亡状态，避免 Pawn 关联短暂为空时误判全员失败。
void AArenaGameMode::NotifyPlayerDeath()
{
	AArenaGameState* ArenaGameState = GetGameState<AArenaGameState>();
	if (!HasAuthority() || !ArenaGameState
		|| ArenaGameState->GetGamePhase() == EArenaGamePhase::Defeat
		|| ArenaGameState->GetGamePhase() == EArenaGamePhase::Victory)
	{
		return;
	}

	bool bFoundParticipatingPlayer = false;
	for (APlayerState* PlayerState : ArenaGameState->PlayerArray)
	{
		const AArenaPlayerState* ArenaPlayerState = Cast<AArenaPlayerState>(PlayerState);
		const UArenaAbilitySystemComponent* ArenaASC = ArenaPlayerState ? ArenaPlayerState->GetArenaAbilitySystemComponent() : nullptr;
		if (!ArenaPlayerState || !ArenaASC)
		{
			continue;
		}

		bFoundParticipatingPlayer = true;
		if (!ArenaASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead))
		{
			return;
		}
	}

	if (bFoundParticipatingPlayer)
	{
		if (WaveManager)
		{
			WaveManager->StopForDefeat();
		}
		ArenaGameState->SetGamePhase(EArenaGamePhase::Defeat);
	}
}
