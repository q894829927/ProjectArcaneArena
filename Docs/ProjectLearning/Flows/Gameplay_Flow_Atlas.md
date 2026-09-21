# Gameplay Flow Atlas（核心调用链图集）

> 最重要的笔记。只记录当前源码真实存在的 Flow，逐步累积。格式：入口 → 逐类函数 → 游戏系统 → 最终结果；网络处标注 [Local] [Owning Client] [Predicted] [Server] [Replicated] [Simulated Client] [Presentation]。
> 证据标签：【源码确认】【推测】【待编辑器验证】。每条 Flow 注明验证状态。

## 目录
- [Flow A：玩家进入 → GAS 就绪 → 技能输入入口](#flow-a)
- [Flow B：波次 → 升级选择 → 下一波](#flow-b)

---

## <a name="flow-a"></a>Flow A：玩家进入 → GAS 就绪 → 技能输入入口

状态：`Implemented`【源码确认】（PIE 冒烟部分通过，完整构建待验证）

```text
[Server] 玩家登录（PostLogin / RestartPlayer / 生成 Pawn）
↓
[Server] AArenaPlayerCharacter::PossessedBy()                    ArenaPlayerCharacter.cpp:101
↓
AArenaPlayerCharacter::InitializeAbilityActorInfo()               ArenaPlayerCharacter.cpp:127
↓
UArenaAbilitySystemComponent::InitAbilityActorInfo(PlayerState, this)
  OwnerActor = AArenaPlayerState，AvatarActor = AArenaPlayerCharacter
↓
[Server][HasAuthority() 分支]
  ApplyDefaultAttributes()                                        ArenaPlayerCharacter.cpp:436
    → ASC->MakeOutgoingSpec(DefaultAttributeEffect)
    → ApplyGameplayEffectSpecToSelf（属性复制给客户端）
  GrantStartupAbilities()                                         ArenaPlayerCharacter.cpp:456
    → 每个 StartupAbility：读 CDO->GetInputTag()
    → AbilitySpec.GetDynamicSpecSourceTags().AddTag(InputTag)
    → ASC->GiveAbility(AbilitySpec)
↓
[Replicated] ASC / 属性 / AbilitySpec 复制到 Owning Client（Mixed 模式）
↓
[Owning Client] AArenaPlayerCharacter::OnRep_PlayerState()        ArenaPlayerCharacter.cpp:110
  → 同一 InitializeAbilityActorInfo()（客户端不授予技能/不应用属性）
↓
[Local] 玩家按 LMB
↓
[Local] Enhanced Input → Input_BasicAttack()                      ArenaPlayerCharacter.cpp:770
↓
[Local] Input_AbilityInputTagPressed(Input.Ability.BasicAttack)   ArenaPlayerCharacter.cpp:686
  —— 门控检查：阶段锁 / 本地 UI 锁 / State.Dead / State.Stunned
↓
UArenaAbilitySystemComponent::AbilityInputTagPressed(InputTag)    ArenaAbilitySystemComponent.cpp:150
  —— 遍历 GetActivatableAbilities()，按 DynamicSpecSourceTags 精确匹配
↓
UAbilitySystemComponent::TryActivateAbility(SpecHandle)
↓
（进入 GAS 激活/预测流程 —— Day 5/6 主题：CanActivateAbility → Commit → TargetData → Server 执行）
↓
最终结果：技能按 GAS 规则激活；服务器拥有最终玩法结果，客户端拥有预测与表现
```

关键点：
- 为什么两端都调 `InitializeAbilityActorInfo`：服务器在 `PossessedBy`、客户端必须等 PlayerState 复制到位后在 `OnRep_PlayerState`；漏客户端会"技能按了没反应"。
- 为什么不会重复授予：授予在 `HasAuthority()` 分支（只有服务器），且 `bGrantedStartupAbilities` / `bAppliedDefaultAttributes` 守卫（ArenaPlayerCharacter.cpp:438/458）。

---

## <a name="flow-b"></a>Flow B：波次 → 升级选择 → 下一波

状态：`Implemented`【源码确认】（升级候选随机/验证链来自源码；资产内配置值【待编辑器验证】）

```text
[Server] AArenaGameMode::BeginPlay()                             ArenaGameMode.cpp:68
  → InitializeUpgradeRandomStream()（种子写入 GameState 复制给客户端观察）
  → SpawnActor<AArenaWaveManager>() → WaveManager->Initialize(WaveData, DropTable, Seed)
  → ScheduleInitialWaveStart()（等预期玩家 Pawn/ASC 就绪；10s 超时兜底）
↓
[Server] AArenaGameMode::StartNextWave()                          ArenaGameMode.cpp:218
  → AArenaWaveManager::StartNextWave()
↓
[Server] WaveManager 刷怪
  → AArenaGameState::SetGamePhase(Combat) / SetCurrentWaveIndex / SetRemainingEnemyCount
↓
[Replicated] AArenaGameState OnRep_* → 广播委托
  → [Presentation] PlayerController/Character 刷新 HUD、按阶段门控移动
↓
[Server] 敌人全灭 → WaveManager::HandleEnemyDeath → CheckWaveCompletion
  → 进入 Upgrade 阶段 → OnUpgradePhaseStarted 委托
↓
[Server] AArenaGameMode::HandleUpgradePhaseStarted()              ArenaGameMode.cpp:380
  → 每个玩家 PrepareUpgradeChoicesForPlayer()                       ArenaGameMode.cpp:397
      （权重抽取 + 构筑保底 + 打乱）
  → AArenaPlayerState::BeginUpgradeSelection()（候选 COND_OwnerOnly 复制）
↓
[Replicated][OwnerOnly] OnRep_UpgradeCandidates → 本地 Controller 显示三选一
↓
[Owning Client] 玩家点击 → AArenaPlayerController::HandleUpgradeChosen()   ArenaPlayerController.cpp:1328
  → ServerSelectUpgrade(UpgradeID)                                 [Server RPC]
↓
[Server] AArenaGameMode::SubmitUpgradeSelection()                 ArenaGameMode.cpp:718
  —— 服务器重验：阶段==Upgrade、未选过、ID 在本轮候选、IsUpgradeEligible
  → ApplyUpgrade()                                                 ArenaGameMode.cpp:575
      · GrantedGameplayEffect（SetByCaller 注入 Upgrade.NumericValue）
      · GiveAbility（SourceObject = 升级 DataAsset，被动读取数值不硬编码）
      · AddLooseGameplayTags + AddReplicatedLooseGameplayTags（Build.* 等）
  → AArenaPlayerState::CompleteUpgradeSelection()（记录堆叠，OwnerOnly 复制）
  → RestorePlayerResourcesAfterUpgrade()（补满 Health/Energy，可驱动复活）
↓
[Server] TryAdvanceAfterUpgradeSelections() → 全员选完 → 下一波
```

关键点：
- 候选随机种子由服务器生成并复制（GameState `UpgradeRandomSeed`），客户端不生成候选。
- 客户端只提交 `FName UpgradeID`，资格/堆叠/阶段全部由服务器重验。
- `COND_OwnerOnly`：候选与 OwnedUpgrades 私有；`bHasSelectedUpgrade` 全端复制供 UI 显示等待。
