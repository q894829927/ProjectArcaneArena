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
* `AArenaGameMode` evaluates player deaths on the server and enters Defeat only after all participating players are dead.
* Verification: static inspection completed; current full build and two-player PIE status are not recorded as verified.

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

### Ability Input and Cooldowns — Implemented

* Enhanced Input routes ability keys through Ability GameplayTags stored on granted AbilitySpecs.
* BasicAttack, Fireball, Dash, Shield, and LightningStorm use GAS cooldown/cost configuration where applicable.
* The HUD observes ASC tags and Active GameplayEffects to display granted/locked state and cooldown time.

## Player Abilities

### BasicAttack — Implemented

* `LMB` activates a LocalPredicted TargetData flow; top-down aims at the cursor and third-person aims at the center reticle.
* The local player predicts facing direction, while the server commits cooldown, performs the melee Sweep, selects a valid target, and applies physical damage through GAS.
* `GA_BasicAttack` supports a configurable predicted Attack Montage, play rate, and start section; the Montage continues after the short targeting Ability ends while damage remains server-authoritative.
* Editor setup pending after the reflection build: assign an unarmed DefaultSlot Montage such as `AM_EnemyMeleeAttack` to `GA_BasicAttack`, then duplicate it as `AM_PlayerBasicAttack` when player-specific timing is needed.
* A configurable debug capsule/line/sphere can display the authority attack range.

### Fireball — Implemented

* `Q` gathers view-aware TargetData locally and sends it through GAS prediction/RPC flow.
* The server commits cost/cooldown and spawns one replicated, damage-authoritative fire projectile.
* Projectile collision filters self/dead targets and applies fire damage through `GE_Damage` SetByCaller values.

### Dash — Implemented

* `E` applies `State.Dashing` and `State.Invincible` during a server-authoritative root-motion dash.
* Dash prioritizes current movement input, making it compatible with both world-relative and camera-relative movement.
* Optional montage playback is presentation-only.

### Shield — Implemented

* `F` commits configured cost/cooldown and applies Shield through a GameplayEffect.
* Incoming damage consumes Shield before Health through the shared AttributeSet damage pipeline.

### LightningStorm — Implemented

* `R` gathers a cursor or center-reticle target point and the server clamps it to maximum cast range.
* The server spawns one replicated area Actor that periodically queries nearby Pawns and applies lightning damage through GAS.
* Damage uses a strict two-dimensional center-distance check, skips source/dead targets, and supports configurable radius, duration, and tick interval.
* `NS_LightningStorm` and the Blueprint area subclass provide persistent visual composition; a development debug circle can display the true damage radius.

## UI and Combat Feedback

### Player HUD — Implemented

* Health, Shield, and Energy displays observe GAS attribute delegates.
* Skill slots display ability availability and cooldown remaining time.
* Third-person mode displays a center reticle; the C++ HUD creates a fallback reticle if the Widget Blueprint does not provide one.
* Optional `PhaseText`, `WaveText`, `RemainingEnemiesText`, and `DefeatText` bindings observe replicated GameState values without owning game rules.

### Enemy Health Bar and Damage Numbers — Implemented

* Enemy health bars observe the enemy AttributeSet and hide during death handling.
* Local, non-replicated damage-number Actors display observed Health loss without owning damage state.

## Enemies and Game Loop

### Enemy GAS Character and Melee AI — Partial

* `AArenaEnemyCharacter` owns a replicated ASC and AttributeSet, grants configured startup abilities on the server, exposes a death delegate, and handles collision/movement/UI shutdown on death.
* `AArenaEnemyAIController` runs a low-frequency server-only target/chase/attack loop, selects the nearest living player from GameState PlayerArray, and freezes path movement while `State.Attacking` is active.
* Chase movement disables overlap-expanded acceptance so the AI reaches the same center-to-center distance used by the authoritative attack range check.
* `UArenaGameplayAbility_EnemyMeleeAttack` commits cooldown at attack start, plays a replicated Montage, and applies physical damage through `GE_Damage` after a server `HitDelay` revalidates target, range, line of sight, and death state.
* Attacks that lose their target during windup miss without refunding cooldown; death or stun cancels the active Montage/Delay and removes `State.Attacking`.
* Dead or stunned enemies stop movement and ability execution; MoveSpeed remains GAS-driven.
* `ABP_ArenaEnemy` uses replicated GroundSpeed for movement state, while `AM_EnemyMeleeAttack` and `AM_EnemyDeath` provide attack/death presentation through the existing DefaultSlot.
* `GA_EnemyMeleeAttack` is configured with `AM_EnemyMeleeAttack`, Montage Play Rate `1.0`, and Hit Delay `0.35`.
* Missing: ranged, elite, boss archetypes and completed PIE/network verification.

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
* Clearing a non-final wave enters Upgrade and waits for explicit `StartNextWave`; clearing the final configured wave enters Victory.
* Missing configuration never counts as wave completion; failed spawns keep Combat active and emit `LogArenaWaves` errors.
* Editor setup pending: create the three-wave `DA_Waves_Prototype` with counts `3 / 5 / 7`, assign it in `BP_ArenaGameMode`, and add tagged TargetPoints plus a covering NavMeshBoundsVolume.
* Missing: upgrade selection UI/runtime, boss content, and completed PIE/network verification.

## Phase 4 Editor Setup Required

* `GE_Status_Stunned`: optional Blueprint child of `UArenaGameplayEffect_Stunned`; the native parent already supplies a two-second Duration and `State.Stunned`.
* `GE_Cooldown_EnemyMeleeAttack`: configured with Duration `1.2` and granted tag `Cooldown.Enemy.MeleeAttack`.
* `GA_EnemyMeleeAttack`: configured with `GE_Damage`, `GE_Cooldown_EnemyMeleeAttack`, `AM_EnemyMeleeAttack`, Montage Play Rate `1.0`, and Hit Delay `0.35`.
* `BP_ArenaEnemyCharacter`: add `GA_EnemyMeleeAttack` to Startup Abilities. Native defaults already set `AArenaEnemyAIController` and `Placed in World or Spawned` possession.
* `BP_ArenaEnemyCharacter`: `K2_OnDeathStarted` plays `AM_EnemyDeath`; its existing three-second lifespan remains the cleanup owner.
* `DA_Waves_Prototype`: three wave entries using `BP_ArenaEnemyCharacter`, counts `3`, `5`, and `7`, each with `0.5` spawn interval.
* `BP_ArenaGameMode`: assign `DA_Waves_Prototype` to Wave Data.
* `Lvl_TopDown`: add a NavMeshBoundsVolume and multiple TargetPoints with Actor Tag `EnemySpawn`.
* `WBP_PlayerHUD`: optionally add TextBlocks named `PhaseText`, `WaveText`, `RemainingEnemiesText`, and `DefeatText`; set `DefeatText` initial visibility to Collapsed.

### Roguelike Upgrades — Not Implemented

* The intended upgrade architecture is documented in `AGENTS.md`, but no runtime upgrade system currently exists.
* Missing: UpgradeDataAsset, candidate generation, server validation, PlayerState ownership/stacks, upgrade UI, and build synergies.

## Verification Notes

* `git diff --check` passed after the Phase 3/4 source implementation.
* Full UBT builds require the project build-safety checks in `AGENTS.md` because the project may use a source-built engine association.
* The current EngineAssociation points to `E:/Unreal engine/UnrealEngine`. The user-started Editor build completed successfully after fixing the `C4458` shadowing error and the native stunned GameplayEffect default-subobject construction.
* A single-player PIE smoke test spawned and navigated three melee enemies. It exposed an overlap-expanded MoveTo acceptance radius that stopped enemies outside the authoritative 170-unit attack range; the chase request now disables that expansion and requires one narrow rebuild before the melee hit loop can be re-verified.
* A feature must explicitly say `Verified` before this log should be treated as proof of completed PIE, multiplayer, or packaged-build testing.
