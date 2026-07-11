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
* Verification: static inspection completed; current full build and two-player PIE status are not recorded as verified.

### Dual Top-Down / Third-Person View — Implemented

* `0` and `NumPad0` toggle between top-down and third-person views with a short SpringArm blend.
* Top-down uses a visible cursor and world-relative movement; third-person uses mouse-look, camera-relative movement, camera collision, and a center reticle.
* Camera mode and reticle state are local presentation state and are not replicated.
* A shared view-aware TargetActor uses cursor hits in top-down and a center-screen trace that ignores the owning Avatar in third-person.
* Verification: implementation and API paths inspected; two-player independent-view PIE is not recorded as verified.

## GAS and Attributes

### Shared Attributes — Implemented

* Health, MaxHealth, Shield, Energy, MaxEnergy, AttackPower, Defense, MoveSpeed, CritChance, and CritDamage use replicated GAS attributes with RepNotify.
* Damage and Healing are transient meta attributes consumed in `PostGameplayEffectExecute`.
* Health, Energy, Shield, critical chance, and other numeric attributes are clamped through AttributeSet hooks.

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

### Enemy Health Bar and Damage Numbers — Implemented

* Enemy health bars observe the enemy AttributeSet and hide during death handling.
* Local, non-replicated damage-number Actors display observed Health loss without owning damage state.

## Enemies and Game Loop

### Enemy GAS Character — Partial

* `AArenaEnemyCharacter` owns a replicated ASC and AttributeSet, applies default attributes on the server, exposes an enemy death delegate, and handles collision/movement/UI shutdown on death.
* Missing: AIController, target acquisition, chase/attack behavior, enemy GameplayAbilities, and enemy archetypes.

### Gameplay State Control — Partial

* `State.Dead`, `State.Stunned`, `State.Invincible`, `State.Dashing`, and `State.Casting` native tags exist.
* `State.Dead` drives enemy death and blocks player abilities; `State.Invincible` is used by Dash and damage execution.
* Missing: complete player death handling, movement suppression while stunned, MoveSpeed-to-CharacterMovement synchronization, and active `State.Casting` behavior.

### Waves and Phases — Partial

* Enemy death broadcasting provides an integration point for a future WaveManager.
* Missing: `AArenaWaveManager`, wave data assets, replicated GameState phase/wave/enemy counts, spawning, wave completion, upgrade phase, victory, and defeat flow.

### Roguelike Upgrades — Not Implemented

* The intended upgrade architecture is documented in `AGENTS.md`, but no runtime upgrade system currently exists.
* Missing: UpgradeDataAsset, candidate generation, server validation, PlayerState ownership/stacks, upgrade UI, and build synergies.

## Verification Notes

* `git diff --check` is the minimum static verification after source or documentation changes.
* Full UBT builds require the project build-safety checks in `AGENTS.md` because the project may use a source-built engine association.
* A feature must explicitly say `Verified` before this log should be treated as proof of completed PIE, multiplayer, or packaged-build testing.
