# Implemented Features

This file records gameplay features that currently exist in Project Arcane Arena. Update it in the same change whenever a feature is added, removed, materially expanded, or changes network authority.

Status meanings:

* `Implemented`: The feature exists in code or configured project assets.
* `Partial`: A usable foundation exists, but important behavior is still missing.
* `Verified`: The feature has completed its stated PIE or build verification pass.
* `Not Implemented`: The subsystem is named only to make a major project gap explicit.

## Core Framework

### Gameplay Framework — Implemented

* `AArenaGameMode`, `AArenaGameState`, `AArenaPlayerController`, and `AArenaPlayerState` provide the project gameplay framework.
* Player ASC and AttributeSet live on `AArenaPlayerState`; `AArenaPlayerCharacter` initializes Owner/Avatar actor info on server possession and client PlayerState replication.
* Startup abilities and default attributes are granted/applied by the authority side.
* `AArenaGameState` replicates `EArenaGamePhase`, current wave index, and remaining enemy count through Blueprint-observable delegates.
* `AArenaGameMode` evaluates long-lived PlayerState ASC death tags on the server and enters Defeat only after all participating players are dead, without treating a temporarily missing Pawn association as a dead/absent player.
* Verification: the all-players-dead rule and dead-target retarget path passed a two-player Listen Server PIE smoke test while the final GameMode fix was loaded through Live Coding. A full build containing that final source change is still pending.

### Dual Top-Down / Third-Person View — Implemented

* `0` and `NumPad0` toggle between top-down and third-person views with a short SpringArm blend.
* Top-down uses a visible cursor and world-relative movement; third-person uses mouse-look, camera-relative movement, camera collision, and a center reticle.
* Camera mode and reticle state are local presentation state and are not replicated.
* A shared view-aware TargetActor uses cursor hits in top-down and a center-screen trace that ignores the owning Avatar in third-person.
* Holding either Shift key enables sprint in both camera modes. The local player predicts the speed change, while a server RPC applies the same validated `MoveSpeed × SprintSpeedMultiplier` authority value; death and stun force sprint off.
* Verification: implementation and API paths inspected; two-player independent-view PIE is not recorded as verified.

## GAS and Attributes

### Shared Attributes — Implemented

* Health, MaxHealth, Shield, Energy, MaxEnergy, AttackPower, Defense, MoveSpeed, CritChance, and CritDamage use replicated GAS attributes with RepNotify.
* Damage and Healing are transient meta attributes consumed in `PostGameplayEffectExecute`.
* Health, Energy, Shield, critical chance, and other numeric attributes are clamped through AttributeSet hooks.
* Player and enemy characters observe MoveSpeed changes and synchronize them to `CharacterMovement.MaxWalkSpeed`.

### Server-Authoritative Damage — Implemented

* `GE_Damage` and `UExecCalc_Damage` resolve BaseDamage, SkillMultiplier, AttackPower, Defense, critical chance, and critical multiplier on the authority side.
* `State.Invincible` prevents normal damage in the execution calculation.
* Shield absorbs incoming damage before Health; reaching zero Health applies replicated `State.Dead`.
* After Shield/Health are actually consumed, `UArenaAttributeSet` asks the source `UArenaAbilitySystemComponent` to route one authoritative `Trigger.OnDamageDealt.*` GameplayEvent. The payload contains actual absorbed damage, the Damage Spec context/objects, source plus damage tags, and a pre-hit target-tag snapshot so status synergies and killing blows use one shared event layer.

### Ability Input and Cooldowns — Implemented

* Enhanced Input routes ability keys through Ability GameplayTags stored on granted AbilitySpecs.
* BasicAttack, Fireball, Dash, Shield, and LightningStorm use GAS cooldown/cost configuration where applicable.
* The HUD observes ASC tags and Active GameplayEffects to display granted/locked state and cooldown time.

### Predicted Ability Networking - Partial

* BasicAttack, Fireball, Dash, Shield, and LightningStorm use `LocalPredicted` activation and run `CommitAbility` on the owning client and server so configured Cost/Cooldown effects can be predicted and reconciled by GAS.
* Dash submits one `FGameplayAbilityTargetData_DashDirection` payload through `AArenaTargetActor_DashDirection`; the server rejects malformed, near-zero, vertical, or multi-entry payloads and both sides execute the normalized accepted direction.
* BasicAttack remains active until its Montage task completes, allowing a rejected or cancelled prediction to stop the local Montage instead of leaving presentation detached from the Ability lifetime.
* Fireball and LightningStorm guard each server activation against duplicate TargetData consumption, while projectile impact and storm per-target/per-tick guards remain authoritative.
* Development CVars `arena.Net.RejectNextAbility` and `arena.Net.AbilityAudit` support one-shot server rejection and execution/spawn/damage auditing. Rejection values are `1 Basic`, `2 Fireball`, `3 Dash`, `4 Shield`, and `5 Storm`.
* Verification pending: narrow Editor build, 150 ms RTT at 2%/5% loss, forced rollback checks, and two-client Dedicated Server PIE.
* Two-client PIE confirmation: Fireball produced one authoritative Projectile per cast and LightningStorm produced one authoritative Area Actor per cast.

## Player Abilities

### BasicAttack — Implemented

* `LMB` activates a LocalPredicted TargetData flow; top-down aims at the cursor and third-person aims at the center reticle.
* The local player predicts facing direction, while the server commits cooldown, performs the melee Sweep, selects a valid target, and applies physical damage through GAS.
* `GA_BasicAttack` supports a configurable predicted Attack Montage, play rate, and start section; the Montage continues after the short targeting Ability ends while damage remains server-authoritative.
* `GA_BasicAttack` is configured with the unarmed DefaultSlot `AM_EnemyMeleeAttack`; it can later be duplicated as `AM_PlayerBasicAttack` when player-specific timing is needed.
* A configurable debug capsule/line/sphere can display the authority attack range.

### Fireball — Implemented

* `Q` gathers view-aware TargetData locally and sends it through GAS prediction/RPC flow.
* The server commits cost/cooldown and spawns one replicated, damage-authoritative fire projectile.
* Projectile collision filters self/dead targets and applies fire damage through `GE_Damage` SetByCaller values.
* The server can now snapshot Fireball-specific upgrade values from the owning PlayerState when spawning the projectile; the base Fireball remains unchanged when no matching upgrade exists.

### Fire Build — Partial

* `FArenaOwnedUpgrade` retains its source `UArenaUpgradeDataAsset`, and `AArenaPlayerState` can aggregate numeric values by target Ability, damage type, and upgrade tag without hard-coding Upgrade IDs in abilities.
* `UArenaGameplayEffect_Burning` defines a four-second, one-second-period, three-stack `AggregateBySource` fire status that grants `Status.Burning`, refreshes duration/period on reapplication, removes itself on `State.Dead`, and drives `GameplayCue.Status.Burning.Active`.
* `UExecCalc_BurningDamage` applies fixed `SetByCaller.Damage.Burning × StackCount` damage on the authority side, skips dead/invincible targets, and reuses the shared Shield-to-Health Damage meta-attribute pipeline.
* Fireball direct damage reads `Upgrade.Fireball.Damage`; Fireball applies Burning only when the source owns `Upgrade.Fireball.Burning` and the direct hit leaves the target alive and non-invincible. Hits against `State.Invincible` still consume the projectile without creating a delayed Burning status.
* `DA_Upgrade_FireballDamage`, `DA_Upgrade_FireballBurning`, `GE_Status_Burning`, `GCN_Burning_Active`, and `GA_Fireball` provide the current editor-configured Fire Build assets.
* Missing verification: complete single-player/two-player PIE checks for upgrade eligibility, damage scaling, stack refresh, death cleanup, and replicated Cue presentation.

### Dash — Implemented

* `E` runs as LocalPredicted so the owning client immediately plays the dash Montage/root motion, while the server confirms cooldown, replicated state tags, and final movement.
* Dash prioritizes current movement input, making it compatible with both world-relative and camera-relative movement.
* The server's ASC Montage state replicates the presentation to other clients; only the authority writes replicated loose `State.Dashing` and `State.Invincible` tags.

### Shield — Implemented

* `F` commits configured cost/cooldown and applies Shield through a GameplayEffect.
* Incoming damage consumes Shield before Health through the shared AttributeSet damage pipeline.

### LightningStorm — Implemented

* `R` gathers a cursor or center-reticle target point and the server clamps it to maximum cast range.
* The server spawns one replicated area Actor that periodically queries nearby Pawns and applies lightning damage through GAS.
* Damage uses a strict two-dimensional center-distance check, skips source/dead targets, and supports configurable radius, duration, and tick interval.
* `NS_LightningStorm` and the Blueprint area subclass provide persistent visual composition; a development debug circle can display the true damage radius.

### Lightning Build — Partial

* LightningStorm snapshots `Upgrade.LightningStorm.Damage` from the owning PlayerState when its authority Area is spawned, increasing the skill multiplier without hard-coding an Upgrade ID.
* Unlocking `Upgrade.LightningStorm.Shocked` lets authority Storm ticks apply a four-second, shared `AggregateByTarget` Shocked status after damage, so the first hit creates the state and later Lightning hits benefit from it.
* `UExecCalc_Damage` recognizes `Damage.Lightning`, finds the target's active `Status.Shocked` GameplayEffect, and applies the highest `SetByCaller.Status.Shocked.LightningDamageBonus` value before critical and Defense modifiers.
* `UArenaGameplayEffect_Shocked` supplies the non-stacking refresh behavior, death removal rule, granted status tag, and persistent `GameplayCue.Status.Shocked.Active` hook in native defaults.
* Build-asset automation lives under `Content/Python/build_assets`, with shared tools, category generators for Upgrade DataAssets, native GameplayEffect/GameplayAbility Blueprint children, looping/burst GameplayCues, and a separate Ability/GameMode/wave link step. Root-level `setup_build_assets.py` remains as the single one-click entry point.
* Fire/Lightning Upgrade, status GE and persistent Cue assets are present at their existing paths. The refactored category scripts and both orchestrator entry points still require an Unreal Editor idempotency regression pass.
* Verification is deferred for damage stacks, first-hit ordering, shared two-player vulnerability, refresh/death cleanup, and replicated Cue presentation.

### Fire + Lightning Overload — Partial

* `UArenaGameplayAbility_Overload` is a ServerOnly event-triggered passive that listens for typed Lightning damage, requires `Upgrade.Combo.Overload`, checks the pre-hit `Status.Burning` snapshot, and ignores `Damage.Secondary` to prevent recursive explosions.
* The passive reads explosion damage from its granted Upgrade DataAsset `SourceObject`; `AArenaGameMode` now preserves that SourceObject when granting upgrade abilities, so runtime behavior does not hard-code an Upgrade ID.
* A successful trigger emits `GameplayCue.Combo.Overload` at the enemy location and applies `Damage.Lightning + Damage.Secondary` through the existing `GE_Damage` pipeline to living, non-invincible `AArenaEnemyCharacter` targets within 300 units. Burning is not consumed, killing Lightning hits remain eligible, and the explosion inherits AttackPower, Crit, Defense, Shocked vulnerability, and Shield-first handling.
* `UArenaGameplayEffect_OverloadLockout` uses one-second `AggregateBySource` active effects on each target, allowing different players to trigger independently while limiting each source/target pair.
* The build-asset generator configs create `GA_Overload`, `GE_Status_OverloadLockout`, `DA_Upgrade_Overload`, an independent placeholder icon asset, and `GCN_Overload_Explosion`; they also connect the Ability classes, append the legendary upgrade to UpgradePool, and configure prototype Wave 4 when run in the editor.
* Missing verification: compile/UHT, generator execution and idempotency, single-player trigger/lockout/killing-blow behavior, replicated burst Cue, two-source lockout independence, and the four-wave progression.

## UI and Combat Feedback

### GameplayCue Routing - Partial

* Native Cue tags and server-confirmed dispatch exist for all five player abilities, enemy melee activation, and physical/fire/lightning damage hits.
* LightningStorm Cast and Active Cues clear the activation prediction key inside their server-confirmed dispatch scope, so the owning client and simulated clients both receive the authoritative storm presentation exactly once.
* Shield, Dash, and LightningStorm use paired server Add/Remove Cue lifetimes; Shield only adds on the zero-to-positive transition and removes on depletion or death. Dash and Shield explicitly attach to the Avatar root instead of the Manny skeletal mesh so imported mesh rotation/location offsets cannot displace directional or centered Niagara effects.
* Successful damage emits one type-specific hit Cue from the authoritative AttributeSet Damage meta-attribute path, using HitResult data when available and target location as fallback.
* GameplayCue Notify assets exist under `/Game/GAS/GameplayCues` and reference the current Niagara systems. Dash/Shield attached-effect placement and Niagara local-space/loop tuning still require PIE verification; remove any duplicate Niagara component from `BP_ArenaLightningStormArea` after the Storm Active Cue is confirmed.
* `ProjectArcaneArenaEditor` provides a Niagara lifecycle bridge used by project Python tooling to copy verified looping System/Emitter State values and enable Local Space on converted shield effects; this editor-only module has no packaged-game runtime ownership.
* Two-client PIE confirmation: after clearing the server activation prediction key for Storm Cast/Active dispatch, both the owning client and the other client can see the same LightningStorm presentation.

### Player HUD — Implemented

* Health, Shield, and Energy displays observe GAS attribute delegates.
* Skill slots display ability availability and cooldown remaining time.
* Third-person mode displays a center reticle; the C++ HUD creates a fallback reticle if the Widget Blueprint does not provide one.
* Optional `PhaseText`, `WaveText`, `RemainingEnemiesText`, and `DefeatText` bindings observe replicated GameState values without owning game rules.
* If the Blueprint omits phase/wave/enemy-count bindings, the C++ HUD creates a compact top-center fallback so Combat, Upgrade, Victory, wave index, and remaining enemies remain visible during prototype testing.
* The HUD displays the authority-generated match upgrade seed in the top-right through an optional `RandomSeedText` binding or a native fallback, updating from replicated GameState events without Tick.

### Enemy Health Bar and Damage Numbers — Implemented

* Enemy health bars observe the enemy AttributeSet and hide during death handling.
* Local, non-replicated damage-number Actors display observed Health loss without owning damage state.

## Enemies and Game Loop

### Local Mass Visual Cluster — Implemented

* `AMyMassClusterActor` creates a lightweight HISM-backed Mass Entity cluster for presentation only; it does not own collision, damage, or authoritative gameplay state.
* `UMyMassMovementProcessor` executes in standalone, server, and client Worlds so every local player sees the cluster move without replicating per-instance transforms.
* Initial positions, velocities, and maximum speeds use a configurable fixed `RandomSeed`, giving each World the same starting cluster state while simulation remains local.
* Verification pending: two-player PIE should confirm both windows animate the cluster and begin from matching layouts.

### Enemy GAS Character and Melee AI — Partial

* `AArenaEnemyCharacter` owns a replicated ASC and AttributeSet, grants configured startup abilities on the server, exposes a death delegate, and handles collision/movement/UI shutdown on death.
* `AArenaEnemyAIController` runs a low-frequency server-only target/chase/attack loop, selects the nearest living player from GameState PlayerArray, and freezes path movement while `State.Attacking` is active.
* Target validity is checked before the attacking-state freeze; when the locked player dies, the server cancels that melee attack, clears focus, and immediately selects the nearest remaining living player.
* Chase movement disables overlap-expanded acceptance so the AI reaches the same center-to-center distance used by the authoritative attack range check.
* `UArenaGameplayAbility_EnemyMeleeAttack` commits cooldown at attack start, plays a replicated Montage, and applies physical damage through `GE_Damage` after a server `HitDelay` revalidates target, range, line of sight, and death state.
* Attacks that lose their target during windup miss without refunding cooldown; death or stun cancels the active Montage/Delay and removes `State.Attacking`.
* Dead or stunned enemies stop movement and ability execution; MoveSpeed remains GAS-driven.
* `ABP_ArenaEnemy` uses replicated GroundSpeed for movement state, while `AM_EnemyMeleeAttack` and `AM_EnemyDeath` provide attack/death presentation through the existing DefaultSlot.
* `GA_EnemyMeleeAttack` is configured with `AM_EnemyMeleeAttack`, Montage Play Rate `1.0`, and Hit Delay `0.35`.
* PIE smoke verification: three melee enemies spawned, chased, attacked through the server damage path, and all three emitted server death notifications. In two-player PIE, enemies abandoned a dead player and continued attacking the remaining living player.
* Q/Fireball, E/Dash, replicated attack presentation, and the remote-client Dash Montage have been observed in multiplayer PIE.
* Missing: ranged, elite, and boss archetypes.

### Gameplay State Control — Partial

* `State.Dead`, `State.Stunned`, `State.Invincible`, `State.Dashing`, and `State.Casting` native tags exist.
* `State.Attacking` is owned for the active enemy attack lifetime and replicated for presentation/debugging.
* `State.Dead` drives player/enemy death, stops movement, cancels abilities, and blocks further movement/ability input.
* `State.Stunned` suppresses player/enemy movement and active abilities, then restores Walking only if the character is not dead.
* `UArenaGameplayEffect_Stunned` provides a two-second Duration GE that grants `State.Stunned`; a `GE_Status_Stunned` Blueprint may inherit it for data tuning.
* `State.Invincible` is used by Dash and damage execution.
* Missing: active `State.Casting` behavior and completed PIE/network verification.

### Waves and Phases — Partial

* `UArenaWaveDataAsset` stores enemy entries/counts, spawn interval, boss marker, and reward count per wave.
* Server-owned `AArenaWaveManager` discovers `ATargetPoint` actors tagged `EnemySpawn`, spawns configured enemies, tracks successful spawns through enemy death delegates, and writes replicated state to GameState.
* Clearing a non-final wave enters Upgrade; the production flow waits for explicit `StartNextWave`, while clearing the final configured wave enters Victory.
* WaveManager retains a configurable three-second prototype fallback, but GameMode now disables it when the formal upgrade-selection system binds to the Upgrade entry.
* Missing configuration never counts as wave completion; failed spawns keep Combat active and emit `LogArenaWaves` errors.
* `DA_Waves_Prototype`, its `BP_ArenaGameMode` reference, three tagged EnemySpawn TargetPoints, and a covering NavMeshBoundsVolume are configured in project assets.
* Missing: boss content.
* Verification: the currently saved three-wave asset completed the formal single-player `Wave 1 -> choice -> Wave 2 -> choice -> Wave 3 -> Victory` flow. The build-asset link script now configures a fourth nine-enemy wave to provide a third upgrade phase; that generated asset change is pending editor execution and PIE verification.

## Phase 4 Configured Assets

* `GE_Status_Stunned`: Blueprint child of `UArenaGameplayEffect_Stunned`; the native parent supplies a two-second Duration and `State.Stunned`.
* `GE_Cooldown_EnemyMeleeAttack`: configured with Duration `1.2` and granted tag `Cooldown.Enemy.MeleeAttack`.
* `GA_EnemyMeleeAttack`: configured with `GE_Damage`, `GE_Cooldown_EnemyMeleeAttack`, `AM_EnemyMeleeAttack`, Montage Play Rate `1.0`, and Hit Delay `0.35`.
* `BP_ArenaEnemyCharacter`: `GA_EnemyMeleeAttack` is configured in Startup Abilities. Native defaults set `AArenaEnemyAIController` and `Placed in World or Spawned` possession.
* `BP_ArenaEnemyCharacter`: `K2_OnDeathStarted` plays `AM_EnemyDeath`; its existing three-second lifespan remains the cleanup owner.
* `DA_Waves_Prototype`: the currently saved asset has three entries using `BP_ArenaEnemyCharacter`, counts `3`, `5`, and `7`, each with `0.5` spawn interval. `configure_build_asset_links.py` updates index four to count `9`, interval `0.5`, non-boss, without duplicating it on repeat runs.
* `BP_ArenaGameMode`: `DA_Waves_Prototype` is assigned to Wave Data.
* `Lvl_TopDown`: a NavMeshBoundsVolume and three TargetPoints with Actor Tag `EnemySpawn` are configured.
* `WBP_PlayerHUD`: optionally add TextBlocks named `PhaseText`, `WaveText`, `RemainingEnemiesText`, and `DefeatText`; set `DefeatText` initial visibility to Collapsed.

### Roguelike Upgrade Foundation — Partial

* `UArenaUpgradeDataAsset` defines upgrade identity, text/icon presentation, rarity, eligibility tags, granted GE/Ability, routing tags, numeric metadata, and stack limits.
* `AArenaPlayerState` owns permanent upgrade stack records, replicated selection completion, and OwnerOnly candidate arrays; local UI observes replicated state delegates.
* `AArenaGameMode` generates up to three unique eligible choices per player from a configured pool, validates the submitted ID against that player's candidates, applies the GameplayEffect/Ability/tags on the server, and waits for every participating PlayerState before starting the next wave.
* Upgrade Candidate V2 uses configurable `Common/Rare/Epic/Legendary` weights of `100/40/15/5` for deterministic weighted draws without replacement. Players who already own `Build.Fire` or `Build.Lightning` receive one eligible same-build candidate when available; dual-build players share one combined guaranteed pool, and all slots are shuffled by the same authority-owned random stream.
* `UpgradeRandomSeedOverride` can pin a positive server seed for reproducible candidate sequences while its default `0` preserves per-session random seeding. RequiredTags, BlockedTags, duplicate IDs, stackability, and MaxStacks remain server-side filters before weighted selection and are revalidated when a choice is submitted.
* Stackable tag/data-driven upgrades can be selected repeatedly until `MaxStacks`; after the first stack grants its persistent tags, later stacks are accepted as PlayerState-owned numeric/behavior metadata instead of being rejected for not re-granting the same tag.
* First-stack upgrade tags are written to both the authority ASC's local TagMap and replicated loose-tag map, so server eligibility checks can unlock dependent upgrades such as Fireball Burning and LightningStorm Shocked on later upgrade phases.
* After a player completes a validated upgrade choice or is auto-completed because no eligible choice exists, the server applies an Instant GAS recovery effect that fills Health and Energy to their latest maxima. Restoring Health removes `State.Dead`, re-enables the existing Pawn, resets the death lifecycle for future deaths, and invokes `K2_OnRevived`; Shield remains unaffected.
* Each match now uses one authority-generated random upgrade seed instead of a fixed seed. The server owns the only candidate `FRandomStream`, while `AArenaGameState` replicates the seed to every machine for consistent session diagnostics and late joins.
* `AArenaWaveManager` broadcasts the formal Upgrade entry and disables its three-second prototype auto-advance while the upgrade system is connected.
* `UArenaUpgradeSelectionWidget` provides a native usable three-button fallback plus optional Blueprint bindings; `AArenaPlayerController` owns UI input mode and sends only the selected ID through a reliable Server RPC. Candidate cards now receive a local presentation snapshot and show rarity plus the resulting `Lv. N/Max`; existing WBP layouts without dedicated rarity/stack TextBlocks fall back to including both values in the main text.
* Upgrade `UIOnly` mode focuses the first enabled/focusable choice button instead of the non-focusable UserWidget container, preventing Slate focus errors while retaining mouse and keyboard selection.
* Seven transparent 512x512 UI Texture2D assets under `/Game/UI/UpgradeIcons` provide a consistent faceted gemstone/metal visual set for AttackPower, MaxHealth, MoveSpeed, Fireball Damage, Burning, LightningStorm Damage, and Shocked upgrades. The repeatable `import_upgrade_icons.py` tool imports them with UI texture settings, writes each matching DataAsset `Icon`, and the native/fallback selection Widget reads that reference into its corresponding `UImage`.
* The native fallback wraps each upgrade `UImage` in a `USizeBox` so parent layout pressure cannot shrink the icon; `UpgradeIconSize` defaults to `220x220` and `UpgradePanelSize` defaults to `1200x460`, with both exposed as configurable layout properties for Blueprint subclasses.
* Entering the upgrade UI flushes pressed keys, ignores movement input, consumes the controlled Pawn's pending movement vector, and stops its movement component immediately; leaving the UI clears keys again before gameplay input is restored, preventing stale Enhanced Input state from keeping the character moving.
* Empty or invalid eligible pools remain fail-visible through `LogArenaUpgrades` errors, but the affected PlayerState now completes without a reward and still receives inter-wave resource recovery so it cannot block Upgrade progression. Upgrade-phase late joins trigger the same candidate preparation and an immediate all-player completion recheck.
* `DA_Upgrade_AttackPower`, `DA_Upgrade_MaxHealth`, and `DA_Upgrade_MoveSpeed` are configured with their corresponding upgrade GameplayEffects and included in the active upgrade pool.
* Verification: AttackPower increases later attack damage, MoveSpeed immediately updates CharacterMovement, upgrades persist across waves, and upgrades stop appearing after reaching `MaxStacks`.
* Verification: in two-player PIE, each player receives an independent candidate set and the next wave starts only after both players complete their selections.
* Missing: Blueprint visual pass, broader ability variants and trigger upgrades, additional build synergies, statistical rarity tuning, and explicit hostile/forged selection RPC testing.

## Verification Notes

* `git diff --check` passed after the Phase 3/4 source implementation.
* Full UBT builds require the project build-safety checks in `AGENTS.md` because the project may use a source-built engine association.
* The current EngineAssociation points to `E:/Unreal engine/UnrealEngine`. The user-started Editor build completed successfully after fixing the `C4458` shadowing error and the native stunned GameplayEffect default-subobject construction.
* The latest Editor DLL was built after the melee acceptance-radius, dead-target retarget, and predicted Dash changes; two-player Listen Server PIE starts without Phase 4 runtime errors.
* Phase 4 acceptance exposed a transient multiplayer Defeat bug: a living PlayerState was skipped when its Pawn association was temporarily null. Defeat now evaluates PlayerState ASC death tags without requiring a Pawn.
* Live Coding succeeded for the final GameMode fix. In the follow-up two-player Listen Server PIE run, Player 1 died, enemies retargeted Player 2 and continued applying damage, and Defeat was deferred until Player 2 also died.
* A single-player slow-motion smoke pass verified BasicAttack input/cooldown, Shield cost/cooldown and absorption, LightningStorm cost/cooldown/area damage, enemy server damage, and two enemy death notifications. The configured melee result remains `8 BaseDamage + 5 AttackPower = 13` before Defense/Crit modifiers.
* Fireball, Dash, replicated attacks, and the remote-client Dash Montage completed their requested multiplayer observation pass.
* `GameplayCueNotifyPaths=/Game/GAS` was added to project config. After restarting the editor, the latest session log no longer reported the previous missing GameplayCue path warning.
* The network-polish UHT pass generated reflection code successfully. Its first C++ pass exposed private `FGameplayAbilitySpecHandle::Handle` audit access, which has been replaced with the public `ToString()` API; a build retry remains pending because the same run also hit Windows page-file error `C3859/C1076`.
* Upgrade UI movement-stop behavior passed the single-player held-input acceptance flow: entering Upgrade while holding movement stops immediately, and releasing the key before selecting does not resume stale movement afterward.
* The configured three-wave single-player loop reaches Victory after both upgrade phases; two-player PIE confirms independent choices and the all-players-selected gate before wave advancement.
* Deferred verification: confirm the authority-generated upgrade seed changes between PIE sessions, remains identical on the Listen Server and every client (including late join), and is displayed consistently by each HUD's top-right seed text.
* Deferred verification: complete the Fire Build single-player/two-player checks for upgrade eligibility, direct-damage stacks, Burning stack/refresh timing, Shield-first periodic damage, death cleanup, and replicated Burning GameplayCue removal.
* Deferred verification: complete the Lightning Build single-player/two-player checks for upgrade eligibility, damage scaling, first-hit Shocked ordering, global refresh behavior, death cleanup, shared Lightning vulnerability, and replicated Shocked GameplayCue removal.
* Deferred verification: compile and run the Overload asset generator twice, then validate Burning retention, one-second per-source/per-target lockout, killing-blow explosions, Secondary recursion prevention, Shocked amplification, burst Cue replication, two-player source independence, and the generated four-wave Victory flow.
* A feature must explicitly say `Verified` before this log should be treated as proof of completed PIE, multiplayer, or packaged-build testing.
