#include "Core/ArenaGameMode.h"

#include "Core/ArenaBalanceTelemetryComponent.h"
#include "Character/ArenaPlayerCharacter.h"
#include "Core/ArenaGameState.h"
#include "Core/ArenaPlayerController.h"
#include "Core/ArenaPlayerState.h"
#include "Core/ArenaUpgradeDataAsset.h"
#include "Core/ArenaWaveDataAsset.h"
#include "Core/ArenaWaveManager.h"
#include "EngineUtils.h"
#include "GAS/ArenaAbilitySystemComponent.h"
#include "GAS/ArenaAttributeSet.h"
#include "GAS/ArenaGameplayEffect_UpgradeRecovery.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "Item/ArenaPickupDropTableDataAsset.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogArenaUpgrades, Log, All);
DEFINE_LOG_CATEGORY_STATIC(LogArenaNetworkFlow, Log, All);

// 构造正式游戏模式并保持非 Seamless Restart，确保 Victory 重开不会继承旧 GAS 或背包状态。
AArenaGameMode::AArenaGameMode()
{
	GameStateClass = AArenaGameState::StaticClass();
	PlayerControllerClass = AArenaPlayerController::StaticClass();
	PlayerStateClass = AArenaPlayerState::StaticClass();
	DefaultPawnClass = AArenaPlayerCharacter::StaticClass();
	WaveManagerClass = AArenaWaveManager::StaticClass();
	bUseSeamlessTravel = false;
}

// Lobby 参数只在正式地图初始化时读取一次，后续新连接不能通过自己的 URL 改写比赛边界。
void AArenaGameMode::InitGame(
	const FString& MapName,
	const FString& Options,
	FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	bRejectLateJoins = UGameplayStatics::HasOption(Options, TEXT("ArenaMatchStarted"))
		&& UGameplayStatics::ParseOption(Options, TEXT("ArenaMatchStarted")) == TEXT("1");
	const FString ExpectedPlayersOption = UGameplayStatics::ParseOption(Options, TEXT("ExpectedPlayers"));
	ExpectedInitialPlayerCount = FMath::Clamp(
		ExpectedPlayersOption.IsNumeric() ? FCString::Atoi(*ExpectedPlayersOption) : 1,
		1,
		4);
}

// 新登录先通过引擎容量和基础校验，再拒绝已经开始的 Direct IP 比赛。
void AArenaGameMode::PreLogin(
	const FString& Options,
	const FString& Address,
	const FUniqueNetIdRepl& UniqueId,
	FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (ErrorMessage.IsEmpty() && bRejectLateJoins)
	{
		ErrorMessage = TEXT("比赛已经开始，无法中途加入。");
	}
}

// 服务器生成随机种子、同步测试背包规则、创建 WaveManager，并启动 Lobby 初始玩家等待闸门。
void AArenaGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		return;
	}

	InitializeUpgradeRandomStream();
	if (AArenaGameState* MutableArenaGameState = GetGameState<AArenaGameState>())
	{
#if WITH_EDITORONLY_DATA
		MutableArenaGameState->SetAllowInventoryOperationsWhileWaiting(
			bAllowInventoryOperationsWhileWaiting);
#endif
		MutableArenaGameState->OnGamePhaseChanged.AddUniqueDynamic(this, &AArenaGameMode::HandleGamePhaseChanged);
	}
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
	WaveManager->Initialize(WaveData, PickupDropTable, UpgradeRandomSeed);
	ValidateMultiplayerPlayerStarts();
	if (ExpectedInitialPlayerCount > 1)
	{
		GetWorldTimerManager().SetTimer(
			InitialPlayerJoinTimeoutHandle,
			this,
			&AArenaGameMode::HandleInitialPlayerJoinTimeout,
			FMath::Max(InitialPlayerJoinTimeout, 1.0f),
			false);
	}
	const AArenaGameState* ArenaGameState = GetGameState<AArenaGameState>();
	if (WaveData && ArenaGameState && !ArenaGameState->PlayerArray.IsEmpty())
	{
		ScheduleInitialWaveStart();
	}
}

// 关卡结束或服务器旅行前清理阶段委托与首波计时器，避免旧 GameMode 收到迟到回调。
void AArenaGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(InitialWaveTimerHandle);
	GetWorldTimerManager().ClearTimer(InitialPlayerJoinTimeoutHandle);
	if (AArenaGameState* ArenaGameState = GetGameState<AArenaGameState>())
	{
		ArenaGameState->OnGamePhaseChanged.RemoveDynamic(this, &AArenaGameMode::HandleGamePhaseChanged);
	}
	if (WaveManager)
	{
		WaveManager->OnUpgradePhaseStarted.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

// Super 完成 RestartPlayer/Possess 后 ASC 已初始化，此时授予测试升级并重新检查首波人数闸门。
void AArenaGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);

#if WITH_EDITOR
	if (HasAuthority() && bEnableDebugStartingUpgrades)
	{
		ApplyDebugStartingUpgrades(NewPlayer ? NewPlayer->GetPlayerState<AArenaPlayerState>() : nullptr);
	}
#endif

	if (HasAuthority())
	{
		ScheduleInitialWaveStart();
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

// 登录时按 Waiting、Upgrade 或 Victory 阶段分别重排首波、补发候选或刷新重开人数。
void AArenaGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	const AArenaGameState* ArenaGameState = GetGameState<AArenaGameState>();
	if (!HasAuthority() || !ArenaGameState)
	{
		return;
	}

	if (ArenaGameState->GetGamePhase() == EArenaGamePhase::Waiting)
	{
		ScheduleInitialWaveStart();
	}
	else if (ArenaGameState->GetGamePhase() == EArenaGamePhase::Upgrade)
	{
		PrepareUpgradeChoicesForPlayer(NewPlayer ? NewPlayer->GetPlayerState<AArenaPlayerState>() : nullptr);
		TryAdvanceAfterUpgradeSelections();
	}
	else if (ArenaGameState->GetGamePhase() == EArenaGamePhase::Victory)
	{
		if (AArenaPlayerState* ArenaPlayerState = NewPlayer ? NewPlayer->GetPlayerState<AArenaPlayerState>() : nullptr)
		{
			ArenaPlayerState->SetVictoryRestartReady(false);
		}
		RefreshVictoryRestartCounts();
	}
}

// 玩家离开后重新检查升级选择和 Victory Ready，避免断线玩家永久阻塞下一波或重开。
void AArenaGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
	TryAdvanceAfterUpgradeSelections();
	RefreshVictoryRestartCounts();
	TryRestartAfterVictoryReady();
}

// 提供给后续升级选择和当前手动测试的服务器波次推进入口。
void AArenaGameMode::StartNextWave()
{
	if (HasAuthority() && WaveManager)
	{
		WaveManager->StartNextWave();
	}
}

// 只在服务器转发有效 Controller 的 Intro 跳过请求，最终阶段和 Boss 状态由 WaveManager 重验。
bool AArenaGameMode::RequestBossIntroSkip(AArenaPlayerController* RequestingController)
{
	return HasAuthority() && WaveManager && WaveManager->RequestBossIntroSkip(RequestingController);
}

// 只在服务器转发有效 Controller 的 Outro 跳过请求，最终阶段和死亡 Boss 由 WaveManager 重验。
bool AArenaGameMode::RequestBossOutroSkip(AArenaPlayerController* RequestingController)
{
	return HasAuthority() && WaveManager && WaveManager->RequestBossOutroSkip(RequestingController);
}

// 服务器验证 Victory 参与者后更新个人 Ready，旅行开始后拒绝迟到的状态切换。
void AArenaGameMode::SetVictoryRestartReady(AArenaPlayerController* RequestingController, bool bReady)
{
	AArenaGameState* ArenaGameState = GetGameState<AArenaGameState>();
	AArenaPlayerState* ArenaPlayerState = RequestingController
		? RequestingController->GetPlayerState<AArenaPlayerState>()
		: nullptr;
	if (!HasAuthority()
		|| bVictoryRestartTravelStarted
		|| !ArenaGameState
		|| ArenaGameState->GetGamePhase() != EArenaGamePhase::Victory
		|| !ArenaPlayerState
		|| !ArenaPlayerState->GetArenaAbilitySystemComponent()
		|| !ArenaGameState->PlayerArray.Contains(ArenaPlayerState))
	{
		return;
	}

	ArenaPlayerState->SetVictoryRestartReady(bReady);
	RefreshVictoryRestartCounts();
	TryRestartAfterVictoryReady();
}

// 达到预期初始化人数后只安排一次首波；后续 PostLogin/HandleStarting 回调不会重置计时器。
void AArenaGameMode::ScheduleInitialWaveStart()
{
	if (!HasAuthority() || !WaveManager || !WaveData || bInitialWaveStartScheduled)
	{
		return;
	}
	const int32 InitializedPlayerCount = CountInitializedInitialPlayers();
	if (InitializedPlayerCount < ExpectedInitialPlayerCount)
	{
		UE_LOG(
			LogArenaNetworkFlow,
			Verbose,
			TEXT("Waiting for initial players: %d/%d initialized."),
			InitializedPlayerCount,
			ExpectedInitialPlayerCount);
		return;
	}

	bInitialWaveStartScheduled = true;
	GetWorldTimerManager().ClearTimer(InitialPlayerJoinTimeoutHandle);
	GetWorldTimerManager().SetTimer(
		InitialWaveTimerHandle,
		this,
		&AArenaGameMode::StartNextWave,
		FMath::Max(InitialWaveDelay, 0.1f),
		false);
	UE_LOG(
		LogArenaNetworkFlow,
		Log,
		TEXT("Initial wave scheduled after %d/%d players initialized."),
		InitializedPlayerCount,
		ExpectedInitialPlayerCount);
}

// 超时后缩小预期人数快照并继续；零人时短暂重试，避免无人世界提前生成整波敌人。
void AArenaGameMode::HandleInitialPlayerJoinTimeout()
{
	if (!HasAuthority() || bInitialWaveStartScheduled)
	{
		return;
	}

	const int32 InitializedPlayerCount = CountInitializedInitialPlayers();
	if (InitializedPlayerCount <= 0)
	{
		UE_LOG(LogArenaNetworkFlow, Warning, TEXT("Initial player wait expired with no initialized players; retrying in one second."));
		GetWorldTimerManager().SetTimer(
			InitialPlayerJoinTimeoutHandle,
			this,
			&AArenaGameMode::HandleInitialPlayerJoinTimeout,
			1.0f,
			false);
		return;
	}

	if (InitializedPlayerCount < ExpectedInitialPlayerCount)
	{
		UE_LOG(
			LogArenaNetworkFlow,
			Warning,
			TEXT("Only %d/%d expected players initialized before timeout; continuing with arrived players."),
			InitializedPlayerCount,
			ExpectedInitialPlayerCount);
		ExpectedInitialPlayerCount = InitializedPlayerCount;
	}
	ScheduleInitialWaveStart();
}

// 只有 Pawn 已存在且 ASC Avatar 指向该 Pawn 时才视为完成 Seamless Travel 的玩法初始化。
int32 AArenaGameMode::CountInitializedInitialPlayers() const
{
	const AArenaGameState* ArenaGameState = GetGameState<AArenaGameState>();
	if (!ArenaGameState)
	{
		return 0;
	}

	int32 InitializedPlayerCount = 0;
	for (APlayerState* PlayerState : ArenaGameState->PlayerArray)
	{
		const AArenaPlayerState* ArenaPlayerState = Cast<AArenaPlayerState>(PlayerState);
		const APawn* PlayerPawn = ArenaPlayerState ? ArenaPlayerState->GetPawn() : nullptr;
		const UArenaAbilitySystemComponent* PlayerASC = ArenaPlayerState
			? ArenaPlayerState->GetArenaAbilitySystemComponent()
			: nullptr;
		if (PlayerPawn && PlayerASC && PlayerASC->GetAvatarActor() == PlayerPawn)
		{
			++InitializedPlayerCount;
		}
	}
	return InitializedPlayerCount;
}

// Lobby 人数超过关卡安全出生点时记录错误，位置仍由关卡设计者手动放置和验证 NavMesh。
void AArenaGameMode::ValidateMultiplayerPlayerStarts() const
{
	if (ExpectedInitialPlayerCount <= 1 || !GetWorld())
	{
		return;
	}

	int32 PlayerStartCount = 0;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		++PlayerStartCount;
	}
	if (PlayerStartCount < ExpectedInitialPlayerCount)
	{
		UE_LOG(
			LogArenaNetworkFlow,
			Error,
			TEXT("Gameplay map has %d PlayerStarts but Lobby expects %d players. Add non-overlapping PlayerStarts on NavMesh."),
			PlayerStartCount,
			ExpectedInitialPlayerCount);
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

// 按配置过滤并抽取候选；空池记录错误、无奖励完成且恢复资源，有候选时才进入等待选择状态。
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

	if (Choices.IsEmpty())
	{
		UE_LOG(LogArenaUpgrades, Error, TEXT("Player %s has no eligible upgrade choices; completing without a reward."), *GetNameSafe(ArenaPlayerState));
		ArenaPlayerState->CompleteUpgradeSelectionWithoutReward();
		RestorePlayerResourcesAfterUpgrade(ArenaPlayerState);
		return;
	}

	ArenaPlayerState->BeginUpgradeSelection(Choices);
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

// 使用 ASC 当前持有的构筑标签检查候选，让五条首版构筑分支都能参与同构筑保底。
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
	const bool bOwnsCritBuild = ASC->HasMatchingGameplayTag(ArenaGameplayTags::Build_Crit);
	const bool bOwnsShieldBuild = ASC->HasMatchingGameplayTag(ArenaGameplayTags::Build_Shield);
	const bool bOwnsDashBuild = ASC->HasMatchingGameplayTag(ArenaGameplayTags::Build_Dash);
	return (bOwnsFireBuild && Upgrade->UpgradeTags.HasTagExact(ArenaGameplayTags::Build_Fire))
		|| (bOwnsLightningBuild && Upgrade->UpgradeTags.HasTagExact(ArenaGameplayTags::Build_Lightning))
		|| (bOwnsCritBuild && Upgrade->UpgradeTags.HasTagExact(ArenaGameplayTags::Build_Crit))
		|| (bOwnsShieldBuild && Upgrade->UpgradeTags.HasTagExact(ArenaGameplayTags::Build_Shield))
		|| (bOwnsDashBuild && Upgrade->UpgradeTags.HasTagExact(ArenaGameplayTags::Build_Dash));
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

// 在服务器应用升级授予，向 GE 注入 NumericValue，并保存 Ability 来源数据与 Build Tags。
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
			// 通用 NumericValue 由 DataAsset 注入；未读取该 SetByCaller 的旧升级 GE 不受影响。
			SpecHandle.Data->SetSetByCallerMagnitude(
				ArenaGameplayTags::SetByCaller_Upgrade_NumericValue,
				Upgrade->NumericValue);
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

#if WITH_EDITOR
// 编辑器测试奖励仍执行服务器资格验证与正式 GAS 授予，成功后记录层数并恢复资源。
bool AArenaGameMode::TryGrantDebugUpgrade(
	AArenaPlayerState* ArenaPlayerState,
	UArenaUpgradeDataAsset* Upgrade) const
{
	if (!HasAuthority() || !ArenaPlayerState || !Upgrade)
	{
		return false;
	}

	if (!IsUpgradeEligible(ArenaPlayerState, Upgrade))
	{
		UE_LOG(LogArenaUpgrades, Warning,
			TEXT("Debug upgrade %s is not eligible for %s; check required tags and stack limits."),
			*Upgrade->UpgradeID.ToString(),
			*GetNameSafe(ArenaPlayerState));
		return false;
	}

	if (!ApplyUpgrade(ArenaPlayerState, Upgrade))
	{
		UE_LOG(LogArenaUpgrades, Warning, TEXT("Failed to apply debug upgrade %s to %s."),
			*Upgrade->UpgradeID.ToString(),
			*GetNameSafe(ArenaPlayerState));
		return false;
	}

	ArenaPlayerState->CompleteUpgradeSelection(Upgrade);
	RestorePlayerResourcesAfterUpgrade(ArenaPlayerState);
	UE_LOG(LogArenaUpgrades, Log, TEXT("Granted debug upgrade %s to %s (stack %d)."),
		*Upgrade->UpgradeID.ToString(),
		*GetNameSafe(ArenaPlayerState),
		ArenaPlayerState->GetUpgradeStackCount(Upgrade->UpgradeID));
	return true;
}

// 测试起始升级严格按数组顺序校验并授予，使依赖 Build Tags 的后续升级获得与正式选择相同的状态。
void AArenaGameMode::ApplyDebugStartingUpgrades(AArenaPlayerState* ArenaPlayerState) const
{
	if (!HasAuthority() || !ArenaPlayerState)
	{
		return;
	}

	for (UArenaUpgradeDataAsset* Upgrade : DebugStartingUpgrades)
	{
		if (!Upgrade)
		{
			UE_LOG(LogArenaUpgrades, Warning, TEXT("Skipped an empty DebugStartingUpgrades entry for %s."), *GetNameSafe(ArenaPlayerState));
			continue;
		}

		TryGrantDebugUpgrade(ArenaPlayerState, Upgrade);
	}

}
#endif

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

// 记录当前新死亡玩家并检查长期死亡状态，避免 Pawn 关联短暂为空时误判全员失败。
void AArenaGameMode::NotifyPlayerDeath()
{
	AArenaGameState* ArenaGameState = GetGameState<AArenaGameState>();
	if (!HasAuthority() || !ArenaGameState
		|| ArenaGameState->GetGamePhase() == EArenaGamePhase::Defeat
		|| ArenaGameState->GetGamePhase() == EArenaGamePhase::BossOutro
		|| ArenaGameState->GetGamePhase() == EArenaGamePhase::Victory)
	{
		return;
	}

	bool bFoundParticipatingPlayer = false;
	bool bAllParticipatingPlayersDead = true;
	for (APlayerState* PlayerState : ArenaGameState->PlayerArray)
	{
		const AArenaPlayerState* ArenaPlayerState = Cast<AArenaPlayerState>(PlayerState);
		const UArenaAbilitySystemComponent* ArenaASC = ArenaPlayerState ? ArenaPlayerState->GetArenaAbilitySystemComponent() : nullptr;
		if (!ArenaPlayerState || !ArenaASC)
		{
			continue;
		}

		bFoundParticipatingPlayer = true;
		if (ArenaASC->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead))
		{
			if (UArenaBalanceTelemetryComponent* Telemetry =
				ArenaGameState->GetBalanceTelemetryComponent())
			{
				Telemetry->RecordPlayerDeath(ArenaPlayerState);
			}
		}
		else
		{
			bAllParticipatingPlayersDead = false;
		}
	}

	if (bFoundParticipatingPlayer && bAllParticipatingPlayersDead)
	{
		if (WaveManager)
		{
			WaveManager->StopForDefeat();
		}
		ArenaGameState->SetGamePhase(EArenaGamePhase::Defeat);
	}
}

// 进入 Victory 时清空旧确认并建立当前参与人数，退出时移除所有终局 Ready 状态。
void AArenaGameMode::HandleGamePhaseChanged(EArenaGamePhase OldPhase, EArenaGamePhase NewPhase)
{
	if (!HasAuthority())
	{
		return;
	}

	AArenaGameState* ArenaGameState = GetGameState<AArenaGameState>();
	if (!ArenaGameState)
	{
		return;
	}

	if (NewPhase == EArenaGamePhase::Victory)
	{
		bVictoryRestartTravelStarted = false;
		for (APlayerState* PlayerState : ArenaGameState->PlayerArray)
		{
			if (AArenaPlayerState* ArenaPlayerState = Cast<AArenaPlayerState>(PlayerState))
			{
				ArenaPlayerState->SetVictoryRestartReady(false);
			}
		}
		RefreshVictoryRestartCounts();
	}
	else if (OldPhase == EArenaGamePhase::Victory)
	{
		for (APlayerState* PlayerState : ArenaGameState->PlayerArray)
		{
			if (AArenaPlayerState* ArenaPlayerState = Cast<AArenaPlayerState>(PlayerState))
			{
				ArenaPlayerState->SetVictoryRestartReady(false);
			}
		}
		ArenaGameState->SetVictoryRestartCounts(0, 0);
	}
}

// 依据当前仍连接且拥有 ASC 的 PlayerState 重新汇总 Victory Ready 计数。
void AArenaGameMode::RefreshVictoryRestartCounts()
{
	AArenaGameState* ArenaGameState = GetGameState<AArenaGameState>();
	if (!HasAuthority() || !ArenaGameState || ArenaGameState->GetGamePhase() != EArenaGamePhase::Victory)
	{
		return;
	}

	int32 ReadyCount = 0;
	int32 RequiredCount = 0;
	for (APlayerState* PlayerState : ArenaGameState->PlayerArray)
	{
		const AArenaPlayerState* ArenaPlayerState = Cast<AArenaPlayerState>(PlayerState);
		if (!ArenaPlayerState || !ArenaPlayerState->GetArenaAbilitySystemComponent())
		{
			continue;
		}

		++RequiredCount;
		ReadyCount += ArenaPlayerState->IsVictoryRestartReady() ? 1 : 0;
	}
	ArenaGameState->SetVictoryRestartCounts(ReadyCount, RequiredCount);
}

// 全员确认后使用服务器旅行重载当前关卡，并通过防重标记避免重复请求。
void AArenaGameMode::TryRestartAfterVictoryReady()
{
	const AArenaGameState* ArenaGameState = GetGameState<AArenaGameState>();
	if (!HasAuthority()
		|| bVictoryRestartTravelStarted
		|| !ArenaGameState
		|| ArenaGameState->GetGamePhase() != EArenaGamePhase::Victory)
	{
		return;
	}

	const int32 RequiredCount = ArenaGameState->GetVictoryRestartRequiredCount();
	if (RequiredCount <= 0 || ArenaGameState->GetVictoryRestartReadyCount() < RequiredCount)
	{
		return;
	}

	bVictoryRestartTravelStarted = true;
	GetWorld()->ServerTravel(TEXT("?Restart"), false);
}
