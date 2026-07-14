#include "Core/ArenaGameMode.h"

#include "Character/ArenaPlayerCharacter.h"
#include "Core/ArenaGameState.h"
#include "Core/ArenaPlayerController.h"
#include "Core/ArenaPlayerState.h"
#include "Core/ArenaUpgradeDataAsset.h"
#include "Core/ArenaWaveDataAsset.h"
#include "Core/ArenaWaveManager.h"
#include "GAS/ArenaAbilitySystemComponent.h"
#include "GAS/ArenaAttributeSet.h"
#include "GAS/ArenaGameplayEffect_UpgradeRecovery.h"
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

// 服务器生成本局升级随机流、创建 WaveManager，并在配置 WaveData 后启动第一波。
void AArenaGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		return;
	}

	InitializeUpgradeRandomStream();
	if (!WaveManagerClass)
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

// 优先使用蓝图配置的固定测试种子，否则由会话 GUID 派生非零种子；客户端只接收复制值。
void AArenaGameMode::InitializeUpgradeRandomStream()
{
	if (!HasAuthority())
	{
		return;
	}

	UpgradeRandomSeed = UpgradeRandomSeedOverride > 0
		? UpgradeRandomSeedOverride
		: static_cast<int32>(GetTypeHash(FGuid::NewGuid()) & 0x7fffffff);
	if (UpgradeRandomSeed == 0)
	{
		UpgradeRandomSeed = 1;
	}

	UpgradeRandomStream.Initialize(UpgradeRandomSeed);
	if (AArenaGameState* ArenaGameState = GetGameState<AArenaGameState>())
	{
		ArenaGameState->SetUpgradeRandomSeed(UpgradeRandomSeed);
	}

	UE_LOG(LogArenaUpgrades, Log, TEXT("Initialized server upgrade random stream with seed %d%s."),
		UpgradeRandomSeed,
		UpgradeRandomSeedOverride > 0 ? TEXT(" (override)") : TEXT(""));
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

// 按配置顺序过滤候选，先执行一次同构筑保底，再按稀有度无放回抽取并确定性洗牌。
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

	TArray<UArenaUpgradeDataAsset*> OwnedBuildUpgrades;
	for (UArenaUpgradeDataAsset* Upgrade : EligibleUpgrades)
	{
		if (IsUpgradeForOwnedBuild(ArenaPlayerState, Upgrade))
		{
			OwnedBuildUpgrades.Add(Upgrade);
		}
	}

	if (!OwnedBuildUpgrades.IsEmpty())
	{
		const int32 BuildChoiceIndex = DrawWeightedUpgradeIndex(OwnedBuildUpgrades);
		if (OwnedBuildUpgrades.IsValidIndex(BuildChoiceIndex))
		{
			UArenaUpgradeDataAsset* BuildChoice = OwnedBuildUpgrades[BuildChoiceIndex];
			Choices.Add(BuildChoice);
			EligibleUpgrades.RemoveSingle(BuildChoice);
		}
	}

	while (!EligibleUpgrades.IsEmpty() && Choices.Num() < DesiredChoiceCount)
	{
		const int32 ChosenIndex = DrawWeightedUpgradeIndex(EligibleUpgrades);
		if (!EligibleUpgrades.IsValidIndex(ChosenIndex))
		{
			break;
		}
		Choices.Add(EligibleUpgrades[ChosenIndex]);
		EligibleUpgrades.RemoveAt(ChosenIndex);
	}
	ShuffleUpgradeChoices(Choices);

	ArenaPlayerState->BeginUpgradeSelection(Choices);
	if (Choices.IsEmpty())
	{
		UE_LOG(LogArenaUpgrades, Error, TEXT("Player %s has no eligible upgrade choices; Upgrade phase will wait for valid configuration."), *GetNameSafe(ArenaPlayerState));
	}
}

// 使用升级资产的稀有度权重抽取一个候选索引，数组顺序保持为 UpgradePool 配置顺序。
int32 AArenaGameMode::DrawWeightedUpgradeIndex(const TArray<UArenaUpgradeDataAsset*>& Candidates)
{
	if (Candidates.IsEmpty())
	{
		return INDEX_NONE;
	}

	double TotalWeight = 0.0;
	for (const UArenaUpgradeDataAsset* Candidate : Candidates)
	{
		TotalWeight += static_cast<double>(GetUpgradeRarityWeight(Candidate));
	}

	if (TotalWeight <= 0.0)
	{
		UE_LOG(LogArenaUpgrades, Warning, TEXT("Upgrade rarity weights produced an empty weighted pool; using deterministic uniform fallback."));
		return UpgradeRandomStream.RandRange(0, Candidates.Num() - 1);
	}

	const double Draw = static_cast<double>(UpgradeRandomStream.FRand()) * TotalWeight;
	double CumulativeWeight = 0.0;
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		CumulativeWeight += static_cast<double>(GetUpgradeRarityWeight(Candidates[Index]));
		if (Draw < CumulativeWeight)
		{
			return Index;
		}
	}

	return Candidates.Num() - 1;
}

// 将资产稀有度映射到 GameMode 可调权重，运行时至少返回一以保持所有稀有度可被抽中。
int32 AArenaGameMode::GetUpgradeRarityWeight(const UArenaUpgradeDataAsset* Upgrade) const
{
	if (!Upgrade)
	{
		return 1;
	}

	switch (Upgrade->Rarity)
	{
	case EArenaUpgradeRarity::Rare:
		return FMath::Max(UpgradeRarityWeights.Rare, 1);
	case EArenaUpgradeRarity::Epic:
		return FMath::Max(UpgradeRarityWeights.Epic, 1);
	case EArenaUpgradeRarity::Legendary:
		return FMath::Max(UpgradeRarityWeights.Legendary, 1);
	case EArenaUpgradeRarity::Common:
	default:
		return FMath::Max(UpgradeRarityWeights.Common, 1);
	}
}

// 使用 ASC 当前持有的构筑标签检查候选，火焰和闪电同时存在时共享同一个匹配池。
bool AArenaGameMode::IsUpgradeForOwnedBuild(
	const AArenaPlayerState* ArenaPlayerState,
	const UArenaUpgradeDataAsset* Upgrade) const
{
	const UArenaAbilitySystemComponent* ASC = ArenaPlayerState ? ArenaPlayerState->GetArenaAbilitySystemComponent() : nullptr;
	if (!ASC || !Upgrade)
	{
		return false;
	}

	const bool bOwnsFireBuild = ASC->HasMatchingGameplayTag(ArenaGameplayTags::Build_Fire);
	const bool bOwnsLightningBuild = ASC->HasMatchingGameplayTag(ArenaGameplayTags::Build_Lightning);
	return (bOwnsFireBuild && Upgrade->UpgradeTags.HasTagExact(ArenaGameplayTags::Build_Fire))
		|| (bOwnsLightningBuild && Upgrade->UpgradeTags.HasTagExact(ArenaGameplayTags::Build_Lightning));
}

// 使用升级随机流执行 Fisher-Yates 洗牌，使相同种子和相同输入始终得到相同槽位顺序。
void AArenaGameMode::ShuffleUpgradeChoices(TArray<UArenaUpgradeDataAsset*>& Choices)
{
	for (int32 Index = Choices.Num() - 1; Index > 0; --Index)
	{
		const int32 SwapIndex = UpgradeRandomStream.RandRange(0, Index);
		Choices.Swap(Index, SwapIndex);
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

// 在服务器应用升级授予、保存 Ability 来源数据并同步 Build Tags，后续层继续累计数值元数据。
bool AArenaGameMode::ApplyUpgrade(AArenaPlayerState* ArenaPlayerState, const UArenaUpgradeDataAsset* Upgrade) const
{
	UArenaAbilitySystemComponent* ASC = ArenaPlayerState ? ArenaPlayerState->GetArenaAbilitySystemComponent() : nullptr;
	if (!ASC || !Upgrade)
	{
		return false;
	}

	const bool bFirstStack = ArenaPlayerState->GetUpgradeStackCount(Upgrade->UpgradeID) == 0;
	// Ability/Build Tag 只需在首层授予；后续层通过 OwnedUpgrades 驱动 NumericValue 等能力增幅。
	bool bAppliedAnything = !bFirstStack && Upgrade->bStackable && !Upgrade->UpgradeTags.IsEmpty();
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
		// SourceObject 保存升级 DataAsset，让被动技能从数据读取数值而不是硬编码 UpgradeID。
		ASC->GiveAbility(FGameplayAbilitySpec(
			Upgrade->GrantedAbility,
			1,
			INDEX_NONE,
			const_cast<UArenaUpgradeDataAsset*>(Upgrade)));
		bAppliedAnything = true;
	}

	if (bFirstStack && !Upgrade->UpgradeTags.IsEmpty())
	{
		// 服务端资格检查读取本地 TagMap；复制 loose tags 只负责把相同状态同步给客户端。
		ASC->AddLooseGameplayTags(Upgrade->UpgradeTags);
		ASC->AddReplicatedLooseGameplayTags(Upgrade->UpgradeTags);
		bAppliedAnything = true;
	}

	return bAppliedAnything;
}

// 按最新资源上限应用恢复 GE；Health 从零恢复时由 AttributeSet 移除 Dead Tag 并复活玩家。
void AArenaGameMode::RestorePlayerResourcesAfterUpgrade(AArenaPlayerState* ArenaPlayerState) const
{
	UArenaAbilitySystemComponent* ASC = ArenaPlayerState ? ArenaPlayerState->GetArenaAbilitySystemComponent() : nullptr;
	const UArenaAttributeSet* AttributeSet = ArenaPlayerState ? ArenaPlayerState->GetArenaAttributeSet() : nullptr;
	if (!ASC || !AttributeSet)
	{
		return;
	}

	const float HealthRecovery = FMath::Max(AttributeSet->GetMaxHealth() - AttributeSet->GetHealth(), 0.0f);
	const float EnergyRecovery = FMath::Max(AttributeSet->GetMaxEnergy() - AttributeSet->GetEnergy(), 0.0f);
	if (HealthRecovery <= KINDA_SMALL_NUMBER && EnergyRecovery <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
	EffectContext.AddSourceObject(ArenaPlayerState);
	const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(
		UArenaGameplayEffect_UpgradeRecovery::StaticClass(),
		1.0f,
		EffectContext);
	if (!SpecHandle.IsValid())
	{
		UE_LOG(LogArenaUpgrades, Warning, TEXT("Failed to create upgrade recovery effect for %s."), *GetNameSafe(ArenaPlayerState));
		return;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(ArenaGameplayTags::SetByCaller_Recovery_Health, HealthRecovery);
	SpecHandle.Data->SetSetByCallerMagnitude(ArenaGameplayTags::SetByCaller_Recovery_Energy, EnergyRecovery);
	ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

// 重新验证候选 ID，成功后记录层数、恢复资源并检查是否可以推进波次。
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

	ArenaPlayerState->CompleteUpgradeSelection(SelectedUpgrade);
	RestorePlayerResourcesAfterUpgrade(ArenaPlayerState);
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
