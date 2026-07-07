# AGENTS.md

## Project Role

You are an Unreal Engine 5 C++ gameplay engineer working on a top-down Roguelike combat demo based on Gameplay Ability System.

The project target is a resume-ready demo that first supports a complete single-player gameplay loop, then can be extended to a two-player Listen Server multiplayer mode.

The demo focuses on:

* UE5 C++ gameplay architecture
* Gameplay Ability System
* AttributeSet design
* GameplayAbility implementation
* GameplayEffect and ExecutionCalculation
* GameplayTag-driven state control
* Top-down combat
* Enemy AI
* Wave-based spawning
* Roguelike upgrade selection
* Multiplayer-friendly architecture

Do not treat this as a large MMORPG or open-world RPG. Keep the scope controlled and suitable for a portfolio project.

---

## Core Development Goal

Build a project named:

`Project Arcane Arena`

A top-down Roguelike arena combat demo where the player fights enemy waves, chooses one of three upgrades after each wave, and defeats a final boss.

The project should first work in single-player, but all core systems must be written with future multiplayer support in mind.

The final technical goal is:

* Single-player complete combat loop
* GAS-based character attribute system
* GAS-based skill system
* Server-authoritative damage design
* Replication-friendly class architecture
* Optional two-player Listen Server support

---

## Development Principles

Follow these principles strictly:

1. Prefer C++ for core systems.
2. Use Blueprint only for data configuration, UI layout, animation hookup, VFX, and simple asset composition.
3. Do not hard-code gameplay values inside ability logic when they can be moved to DataAsset, DataTable, GameplayEffect, or config variables.
4. Do not directly modify Health, Shield, Energy, AttackPower, or other gameplay attributes outside GAS unless absolutely necessary.
5. Use GameplayEffect to modify attributes.
6. Use GameplayEffectExecutionCalculation for non-trivial damage calculation.
7. Use GameplayTag to represent gameplay state instead of many scattered boolean variables.
8. Keep code modular and readable.
9. Avoid over-engineering.
10. Do not introduce unnecessary third-party plugins.
11. Do not build online matchmaking, account login, inventory trading, or persistent backend systems.
12. Prioritize a stable, demonstrable combat prototype.

---

## Multiplayer-Friendly Rule

Even when implementing single-player features, write code as if the project may later support Listen Server multiplayer.

This means:

* Damage should be calculated by the authority side.
* Enemy spawning should be controlled by GameMode or server-owned managers.
* Wave state should be synchronized through GameState.
* Player combat state should be stored in PlayerState when appropriate.
* UI should observe replicated data or GAS delegates, not own gameplay state.
* Projectiles that cause damage should be server-spawned and replicated.
* Client-side code may handle input and presentation, but should not decide final damage or death.

Do not write single-player-only logic that would require a major rewrite for multiplayer.

---

## Recommended Class Architecture

Use this architecture unless there is a strong reason to change it.

### Core

* `AArenaGameMode`

  * Server-only game rules
  * Starts waves
  * Spawns enemies
  * Checks wave completion
  * Starts upgrade phase
  * Handles victory and defeat

* `AArenaGameState`

  * Replicated game state
  * Current wave index
  * Current game phase
  * Remaining enemy count
  * Boss state
  * Upgrade phase state

* `AArenaPlayerController`

  * Local input binding
  * HUD creation
  * UI interaction
  * Sends upgrade selection request to server

* `AArenaPlayerState`

  * Owns player AbilitySystemComponent
  * Owns player AttributeSet
  * Stores kill count
  * Stores whether the player has selected an upgrade
  * Supports future respawn without losing GAS state

---

### Character

* `AArenaCharacterBase`

  * Common character logic
  * AbilitySystemInterface implementation
  * Team or faction interface if needed
  * Death handling entry point

* `AArenaPlayerCharacter`

  * Player movement
  * Camera setup
  * Mouse-facing logic
  * Retrieves ASC from PlayerState
  * Initializes Ability Actor Info in `PossessedBy` and `OnRep_PlayerState`

* `AArenaEnemyCharacter`

  * Enemy attribute setup
  * Enemy death handling
  * Enemy ability initialization
  * Replicated movement

---

### GAS

* `UArenaAbilitySystemComponent`

  * Project-specific ASC extension
  * Ability input binding helpers
  * GameplayTag helper functions if needed

* `UArenaAttributeSet`

  * Health
  * MaxHealth
  * Shield
  * MaxShield
  * Energy
  * MaxEnergy
  * AttackPower
  * Defense
  * MoveSpeed
  * CritChance
  * CritDamage
  * Damage as Meta Attribute
  * Healing as Meta Attribute

* `UArenaGameplayAbility`

  * Base class for all abilities
  * Common cost, cooldown, targeting, and activation helpers

* `UArenaGameplayTags`

  * Centralized native GameplayTag declaration and initialization

---

### Abilities

Implement these GameplayAbilities:

* `GA_BasicAttack`

  * Basic attack
  * Applies physical damage
  * Has attack interval or cooldown

* `GA_Fireball`

  * Spawns a projectile
  * Projectile is server-spawned
  * Applies fire damage
  * Has energy cost and cooldown

* `GA_Dash`

  * Short movement burst
  * Adds `State.Dashing`
  * Adds `State.Invincible` during dash
  * Has cooldown

* `GA_Shield`

  * Applies temporary shield GameplayEffect
  * Has energy cost and cooldown

* `GA_LightningStorm`

  * Creates an area damage actor
  * Applies periodic lightning damage
  * Has high cooldown and energy cost

Optional boss abilities:

* `GA_Boss_Charge`
* `GA_Boss_FireZone`
* `GA_Boss_SummonMinions`

---

### Effects

Create these GameplayEffects:

* `GE_Init_PlayerAttributes`
* `GE_Init_EnemyAttributes`
* `GE_Damage`
* `GE_Heal`
* `GE_Shield`
* `GE_Cooldown_BasicAttack`
* `GE_Cooldown_Fireball`
* `GE_Cooldown_Dash`
* `GE_Cooldown_Shield`
* `GE_Cooldown_LightningStorm`
* `GE_Upgrade_AttackPower`
* `GE_Upgrade_MaxHealth`
* `GE_Upgrade_CritChance`
* `GE_Upgrade_MoveSpeed`
* `GE_Upgrade_CooldownReduction`

Use scalable floats or DataAssets where appropriate.

---

### Execution

Implement:

* `UExecCalc_Damage`

Damage calculation should support:

* Base damage
* Skill multiplier
* Source AttackPower
* Target Defense
* Critical hit chance
* Critical damage multiplier
* Damage type GameplayTag
* Shield-first damage absorption
* Final Health reduction
* Death check after damage is applied

Suggested formula:

`FinalDamage = BaseDamage * SkillMultiplier * CritMultiplier * DefenseReduction`

Shield should be consumed before Health.

---

## GameplayTags

Use a clean tag hierarchy.

Required tags:

```text
State.Dead
State.Stunned
State.Invincible
State.Dashing
State.Casting

Ability.BasicAttack
Ability.Fireball
Ability.Dash
Ability.Shield
Ability.LightningStorm

Damage.Physical
Damage.Fire
Damage.Lightning

Cooldown.BasicAttack
Cooldown.Fireball
Cooldown.Dash
Cooldown.Shield
Cooldown.LightningStorm

Phase.Combat
Phase.Upgrade
Phase.Victory
Phase.Defeat
```

Rules:

* A dead character cannot activate abilities.
* A stunned character cannot move or attack.
* An invincible character should not receive normal damage.
* Cooldown tags should block ability reactivation.
* Casting tags may be used to prevent overlapping ability activation.

---

## AttributeSet Rules

Use standard GAS attribute replication with RepNotify.

Each replicated attribute should have:

* `UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_XXX)`
* `GAMEPLAYATTRIBUTE_PROPERTY_GETTER`
* `GAMEPLAYATTRIBUTE_VALUE_GETTER`
* `GAMEPLAYATTRIBUTE_VALUE_SETTER`
* `GAMEPLAYATTRIBUTE_VALUE_INITTER`
* `OnRep_XXX`
* `DOREPLIFETIME_CONDITION_NOTIFY`

Clamp attributes where needed:

* `Health` between `0` and `MaxHealth`
* `Shield` between `0` and `MaxShield`
* `Energy` between `0` and `MaxEnergy`

Use:

* `PreAttributeChange` for clamping max-dependent values
* `PostGameplayEffectExecute` for damage, healing, death check, and shield-first logic

---

## Ability System Initialization

For player characters, prefer this multiplayer-friendly setup:

* ASC lives on `AArenaPlayerState`
* AttributeSet lives on `AArenaPlayerState`
* `AArenaPlayerCharacter` gets ASC from PlayerState

Server flow:

```text
AArenaPlayerCharacter::PossessedBy
→ Get AArenaPlayerState
→ Get ASC
→ InitAbilityActorInfo(PlayerState, Character)
→ Give startup abilities
→ Apply default attributes
```

Client flow:

```text
AArenaPlayerCharacter::OnRep_PlayerState
→ Get AArenaPlayerState
→ Get ASC
→ InitAbilityActorInfo(PlayerState, Character)
→ Bind input
→ Bind UI delegates
```

Avoid putting permanent player GAS state only on Character, because Character may be destroyed and respawned.

---

## Projectile Rules

Projectile abilities must follow this rule:

* Client input activates ability.
* Server spawns the damage-authoritative projectile.
* Projectile has `bReplicates = true`.
* Projectile movement is replicated or simulated appropriately.
* Collision damage is handled on the server.
* Clients only display visuals, audio, and GameplayCue effects.

Do not let each client independently spawn a damage-dealing projectile.

---

## Enemy Rules

Enemy logic should be server-authoritative.

* Enemies are spawned by GameMode, WaveManager, or another server-owned manager.
* Enemy Character should replicate.
* Enemy movement should replicate.
* Enemy AIController runs on the server.
* Enemy attacks should apply GameplayEffects from the server.
* Enemy death should notify WaveManager or GameMode on the server.

Enemy types for the first version:

* Melee enemy
* Ranged enemy
* Elite enemy
* Boss enemy

Keep enemy AI simple at first:

```text
Idle
Chase
Attack
Dead
```

Behavior Tree can be added later, but is not required for the first working version.

---

## Wave System

Implement a wave-based combat loop.

Basic flow:

```text
Start Game
→ Spawn Wave 1
→ Player kills all enemies
→ Enter Upgrade Phase
→ Player selects one of three upgrades
→ Spawn Next Wave
→ Repeat
→ Spawn Boss Wave
→ Defeat Boss
→ Victory
```

Recommended classes:

* `AArenaWaveManager`
* `UArenaWaveDataAsset`
* `FArenaWaveConfig`
* `EArenaGamePhase`

Wave data should include:

* Wave index
* Enemy types
* Enemy counts
* Spawn delay
* Whether this is a boss wave
* Reward count

---

## Roguelike Upgrade System

After each wave, show three random upgrades.

Upgrade selection should be data-driven.

Recommended class:

* `UArenaUpgradeDataAsset`

Upgrade data should include:

* Upgrade ID
* Upgrade name
* Upgrade description
* Upgrade icon
* Upgrade rarity
* Granted GameplayEffect
* Target ability tag
* Numeric value
* Whether it is stackable

Upgrade examples:

* AttackPower +10%
* MaxHealth +20
* CritChance +5%
* MoveSpeed +10%
* Fireball damage +20%
* Fireball projectile count +1
* Dash cooldown -20%
* Shield value +30%
* Kill restores Health
* Critical hit restores Energy

For future multiplayer:

* UI sends selected upgrade ID to PlayerController.
* PlayerController sends Server RPC.
* Server validates the upgrade.
* Server applies the corresponding GameplayEffect to that player.
* PlayerState records that the player has selected an upgrade.
* GameMode starts next wave after all players finish selection.

---

## UI Rules

UI should not own gameplay state.

UI should observe:

* Attribute change delegates
* GameplayTag events
* GameState replicated phase
* PlayerState selection state
* Cooldown information from GAS

Required UI:

* Health bar
* Shield bar
* Energy bar
* Skill slots
* Cooldown display
* Damage numbers
* Enemy health bar
* Current wave display
* Upgrade selection panel
* Victory / defeat screen

Do not update Health or Energy directly from UI.

---

## Input

Use Enhanced Input.

Recommended inputs:

```text
WASD        Move
Mouse       Aim / Face cursor
Left Mouse  Basic Attack
Q           Fireball
E           Dash
R           Lightning Storm
F           Shield
```

Ability input should be routed through AbilitySystemComponent rather than directly calling ability logic from Character.

---

## Code Style

Use Unreal Engine coding style.

Rules:

* Prefix classes correctly: `A`, `U`, `F`, `I`, `E`
* Keep headers clean.
* Forward declare when possible.
* Avoid unnecessary includes.
* Use `UPROPERTY` for UObject references that need garbage collection.
* Use `TObjectPtr` where appropriate.
* Use `const` correctness.
* Do not place large logic directly inside Tick unless necessary.
* Prefer timers, delegates, GAS tasks, or event-driven logic.
* Keep functions short and focused.
* Add comments only where the logic is non-obvious.

---

## Build and Safety Rules

Before changing code:

1. Inspect existing files.
2. Understand current architecture.
3. Avoid rewriting unrelated systems.
4. Make minimal targeted changes.
5. Preserve existing naming patterns if the project already has them.
6. Do not rename public classes casually.
7. Do not delete files unless explicitly instructed.
8. Do not modify engine source unless explicitly instructed.
9. Do not introduce plugin dependencies unless explicitly approved.

After changing code:

1. Check for compile errors.
2. Check includes.
3. Check Unreal reflection macros.
4. Check replication macros.
5. Check GAS initialization path.
6. Explain what changed and why.

---

## Unreal Build Verification Safety

This project may be associated with a source-built Unreal Engine directory. Treat any build command as potentially expensive and disruptive.

Incident summary to avoid repeating:

* A small gameplay-code validation attempt triggered a large engine rebuild because UnrealBuildTool was run against the source engine with changed command-line arguments.
* Trying `-NoSharedPCH` to work around a PCH/page-file error invalidated the UBT makefile and caused thousands of engine/plugin actions to be scheduled.
* Visual Studio then showed engine-level targets such as `UE5`, `ShaderCompileWorker`, `AutomationTool`, `Slate`, `RenderCore`, and `BlueprintGraph` compiling.
* The failure was mainly `C3859` / system code `1455` / `C1076`, meaning Windows virtual memory/page file was too small for the PCH workload.
* The root cause was the build invocation and source-engine association, not the small gameplay code edit itself.

Rules:

1. Do not run a full Visual Studio solution build, `UE5` target build, source-engine rebuild, or `ShaderCompileWorker` build unless the user explicitly asks for it.
2. Do not use `-NoSharedPCH`, clean/rebuild, or any broad makefile-invalidating build flag as a quick workaround for compile errors.
3. Do not change `ProjectArcaneArena.uproject` `EngineAssociation` unless the user explicitly requests an engine switch.
4. Before any build command, check `git diff -- ProjectArcaneArena.uproject` and confirm whether the project is associated with a source engine or installed engine.
5. Before any build command, check for existing `Build.bat`, `UnrealBuildTool`, `cl`, `link`, and relevant `dotnet` build processes. Do not start another build if one is already running.
6. For routine C++ validation, prefer the smallest possible check first: inspect includes, run `git diff --check`, and compile only when necessary.
7. If compile verification is necessary, ask the user first when the project is on a source engine or when the editor/Live Coding is active.
8. If the user approves a compile, use only the narrow project editor target, without broad flags:

```text
Build.bat ProjectArcaneArenaEditor Win64 Development -Project="E:\UE_DEMO\ProjectArcaneArena\ProjectArcaneArena.uproject" -WaitMutex -FromMsBuild
```

9. If UBT reports Live Coding is active, do not try unrelated workaround flags. Ask the user to use the editor Compile/Live Coding flow or to close the editor.
10. If PCH virtual memory errors appear, stop and report the page-file issue. Do not attempt source-engine-wide rebuilds to force progress.

---

## Code Review Rules

For any review request, follow `code_review.md`.

Review responses are an exception to the normal task response format: findings must come first, ordered by severity, and grounded in file and line references whenever possible.

If no issues are found, say that clearly and mention any remaining test gaps or residual risk.

---

## Common Mistakes to Avoid

Avoid these mistakes:

* Directly subtracting Health in Character or Projectile.
* Letting client decide final damage.
* Spawning damage projectiles on every client.
* Putting wave state only in UI.
* Putting permanent player GAS state only in Character.
* Using many booleans instead of GameplayTags.
* Hard-coding all ability values in C++.
* Making the first version too large.
* Building matchmaking or online services too early.
* Creating complex AI before the player combat loop works.
* Creating too many abilities before damage and AttributeSet are stable.

---

## Recommended Implementation Order

Follow this order:

### Phase 1: Single-Player Foundation

1. Create top-down character movement.
2. Set up GameMode, PlayerController, PlayerState, GameState.
3. Add AbilitySystemComponent and AttributeSet.
4. Initialize GAS correctly.
5. Add Health, Shield, Energy, AttackPower, Defense.
6. Add basic HUD.

### Phase 2: GAS Combat

1. Add BasicAttack ability.
2. Add Fireball ability.
3. Add server-authoritative damage GameplayEffect.
4. Add ExecutionCalculation for damage.
5. Add shield-first damage logic.
6. Add death logic.
7. Add cooldown and cost.

### Phase 3: More Abilities

1. Add Dash.
2. Add Shield.
3. Add LightningStorm.
4. Add GameplayTag-driven state control.
5. Add GameplayCue or visual feedback.

### Phase 4: Enemies and Waves

1. Add melee enemy.
2. Add ranged enemy.
3. Add enemy GAS attributes.
4. Add enemy attacks.
5. Add WaveManager.
6. Add wave completion detection.

### Phase 5: Roguelike Upgrade

1. Add UpgradeDataAsset.
2. Add three-choice upgrade UI.
3. Add permanent upgrade GameplayEffects.
4. Add upgrade phase after each wave.
5. Add upgrade stacking rules.

### Phase 6: Boss and Polish

1. Add boss enemy.
2. Add boss abilities.
3. Add victory and defeat flow.
4. Add damage numbers.
5. Add better UI feedback.
6. Record demo video.

### Phase 7: Multiplayer Extension

1. Test with two PIE players.
2. Ensure player movement replicates.
3. Ensure ASC on PlayerState works.
4. Ensure attributes replicate.
5. Ensure projectile is server-spawned and replicated.
6. Ensure enemy AI runs on server.
7. Ensure wave state replicates through GameState.
8. Ensure upgrade selection uses Server RPC.
9. Add simple two-player Listen Server support.

---

## Resume-Oriented Output

When implementing features, keep the final resume value in mind.

The project should be explainable with these points:

* Designed a modular UE5 GAS combat framework.
* Implemented AttributeSet-based character attributes.
* Implemented GameplayAbility-based skills.
* Implemented GameplayEffect and ExecutionCalculation damage pipeline.
* Implemented shield, defense, critical hit, cooldown, cost, and death handling.
* Used GameplayTags for state control.
* Built wave-based Roguelike combat loop.
* Added data-driven upgrade system.
* Designed architecture to support future Listen Server multiplayer.
* Implemented server-authoritative combat logic for multiplayer readiness.

---

## Expected Response Format

When working on tasks, respond with:

1. What files were inspected.
2. What was changed.
3. Why the change was made.
4. How to test it.
5. Any risks or follow-up tasks.

Keep responses concise and practical.

Do not give vague advice when code changes are needed. Make concrete edits.

---

## Current Scope Boundary

The current target is not a complete commercial game.

Do not implement:

* Account login
* Matchmaking
* Dedicated server deployment
* Large inventory system
* Trading system
* Open-world map
* Complex quest system
* Full animation combat framework
* Large-scale multiplayer networking
* Save/load progression unless explicitly requested

Focus on a clean, playable, technically strong combat demo.
