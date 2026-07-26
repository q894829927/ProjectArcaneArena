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
* Enemy Capsule and Mesh components ignore `ECC_Camera` at runtime, so third-person SpringArm collision remains responsive to walls without snapping when the player Dashes through an enemy.
* Holding either Shift key enables sprint in both camera modes. The local player predicts the speed change, while a server RPC applies the same validated `MoveSpeed × SprintSpeedMultiplier` authority value; death and stun force sprint off.
* Verification: implementation and API paths inspected; two-player independent-view PIE is not recorded as verified.

## GAS and Attributes

### Shared Attributes — Implemented

* Health, MaxHealth, Shield, Energy, MaxEnergy, AttackPower, Defense, MoveSpeed, CritChance, and CritDamage use replicated GAS attributes with RepNotify.
* Damage and Healing are transient meta attributes consumed in `PostGameplayEffectExecute`.
* Health, Energy, Shield, critical chance, and other numeric attributes are clamped through AttributeSet hooks.
* Player and enemy characters observe MoveSpeed changes and synchronize them to `CharacterMovement.MaxWalkSpeed`.

### Server-Authoritative Damage — Implemented

* `GE_Damage` and `UExecCalc_Damage` resolve BaseDamage, SkillMultiplier, AttackPower, Defense, critical chance, and critical multiplier on the authority side. A successful roll adds `Damage.Critical` to that target's independent Damage Spec, so downstream systems reuse the one authoritative result.
* `State.Invincible` prevents normal damage in the execution calculation.
* Shield absorbs incoming damage before Health; reaching zero Health applies replicated `State.Dead`.
* After Shield/Health are actually consumed, `UArenaAttributeSet` asks the source `UArenaAbilitySystemComponent` to route one authoritative `Trigger.OnDamageDealt.*` GameplayEvent. The payload contains actual absorbed damage, the Damage Spec context/objects, source plus damage tags, and a pre-hit target-tag snapshot so status synergies and killing blows use one shared event layer.
* The same authority route emits `Trigger.OnCrit` for actual critical damage and `Trigger.OnKill` when the target changes from alive before the hit to `State.Dead` after settlement. Event order is typed OnDamage, OnCrit, then OnKill; the outcomes are captured before synchronous passives run so a critical killing blow can trigger both result events exactly once.
* Each authority damage settlement emits one `LogArenaDamage` entry containing the source Avatar, target Avatar, resolved skill/status label, and actual Shield plus Health loss after clamping. Burning and Overload are labeled explicitly; unknown sources fall back to their source/effect class name.

### Ability Input and Cooldowns — Implemented

* Enhanced Input routes ability keys through Ability GameplayTags stored on granted AbilitySpecs.
* BasicAttack, Fireball, Dash, Shield, and LightningStorm use GAS cooldown/cost configuration where applicable.
* The HUD observes ASC tags and Active GameplayEffects to display granted/locked state and cooldown time.

### OnAbilityCast Trigger and Arcane Flow — Partial

* `UArenaAbilitySystemComponent::NotifyAbilityCommit` now emits one authority-only `Trigger.OnAbilityCast` after GAS successfully commits Cost/Cooldown for a player-owned active Ability. Client prediction, failed commits, passive Abilities, and enemy ASCs do not create the authoritative event.
* BasicAttack, Fireball, Dash, Shield, and LightningStorm carry `Ability.Type.PlayerActive`; only Fireball, Shield, and LightningStorm additionally carry `Ability.Type.EnergySkill`. The event payload includes the committed Ability plus ASC and Ability asset tags so passives can filter without per-skill event code.
* `UArenaGameplayAbility_EnergyOnAbilityCast` is a ServerOnly event passive for the stackable Rare `DA_Upgrade_EnergyOnAbilityCast`. It reads its Upgrade DataAsset from AbilitySpec `SourceObject`, restores `5/10/15 Energy` after an EnergySkill commit through the existing SetByCaller Energy restore GE, and never commits itself, preventing event recursion.
* Build-asset automation creates `GA_EnergyOnAbilityCast`, `GE_Trigger_EnergyOnAbilityCast`, the Upgrade DataAsset and an independent placeholder icon, connects the restore effect, and appends the upgrade to GameMode UpgradePool idempotently. The narrow Editor target is up to date, two consecutive commandlet generation passes completed with zero errors/warnings, and direct asset inspection confirmed the native parents, GE binding, core DataAsset fields, single UpgradePool entry, absence of duplicate assets, and all five active Ability Blueprint CDO tag classifications. Commit-boundary behavior, prediction rejection, and two-player ownership remain pending PIE verification.

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
* `UArenaGameplayEffect_Burning` defines a four-second, one-second-period, three-stack `AggregateBySource` fire status that grants `Status.Burning`, refreshes duration/period on reapplication, removes itself on `State.Dead`, and drives `GameplayCue.Status.Burning.Active`. Its periodic Spec intentionally omits the Fireball's initial `HitResult`, so repeated fire-hit Cue bursts resolve from the moving target's current Avatar location instead of the stale impact point.
* `UExecCalc_BurningDamage` applies fixed `SetByCaller.Damage.Burning × StackCount` damage on the authority side, skips dead/invincible targets, and reuses the shared Shield-to-Health Damage meta-attribute pipeline.
* Fireball direct damage reads `Upgrade.Fireball.Damage`; Fireball applies Burning only when the source owns `Upgrade.Fireball.Burning` and the direct hit leaves the target alive and non-invincible. Hits against `State.Invincible` still consume the projectile without creating a delayed Burning status.
* `DA_Upgrade_FireballDamage`, `DA_Upgrade_FireballBurning`, `GE_Status_Burning`, `GCN_Burning_Active`, and `GA_Fireball` provide the current editor-configured Fire Build assets.
* Missing verification: complete single-player/two-player PIE checks for upgrade eligibility, damage scaling, stack refresh, death cleanup, and replicated Cue presentation.

### Dash — Implemented

* `E` runs as LocalPredicted so the owning client immediately plays the dash Montage/root motion, while the server confirms cooldown, replicated state tags, and final movement.
* Dash prioritizes current movement input, making it compatible with both world-relative and camera-relative movement.
* The server's ASC Montage state replicates the presentation to other clients; only the authority writes replicated loose `State.Dashing` and `State.Invincible` tags.
* Dash uses `ServerOnlyTermination`: the owning client may stop its predicted movement/presentation locally but cannot end the authority instance early. A normally completed authority Dash emits one `Trigger.OnDashEnd` event containing the actual server start/end locations; prediction rejection, target-data cancellation, death, Stun, and other cancellation paths do not emit it.

### Dash Build — Partial

* `DA_Upgrade_DashCooldown` establishes `Build.Dash` and contributes `15%` cooldown reduction per stack, capped at three stacks. `UArenaGameplayAbility_Dash::ApplyCooldown()` builds the same predicted/authority Cooldown Spec and scales its actual configured duration, with a `0.25s` safety floor.
* `DA_Upgrade_DashLightningTrail` is a one-stack Rare follow-up requiring `Build.Dash`. It grants the ServerOnly `UArenaGameplayAbility_DashLightningTrail`, which consumes the actual authority path from `Trigger.OnDashEnd` and spawns exactly one replicated `AArenaDashTrailArea`.
* The trail lasts `2s`, uses a `120`-unit strict two-dimensional point-to-segment radius, and applies base `6` Lightning damage immediately and every `0.5s`. Each target is settled at most once per tick through `GE_Damage`; the damage Spec carries `Ability.Passive.DashLightningTrail`, and authoritative damage logging prefers the source Upgrade DataAsset's `TargetAbilityTag`.
* Each replicated Trail Area locally drives its own paired `GameplayCue.Ability.Dash.Trail` lifecycle at the path midpoint and direction. Using the Area as Cue target keeps overlapping trails independent instead of sharing `UAbilitySystemComponent::RemoveGameplayCue(Tag)` on the player. `BP_ArenaDashTrailArea` also exposes replicated path geometry through `K2_OnTrailGeometryChanged` for a later exact-length Blueprint/Niagara presentation pass.
* During `GA_Dash`, the predicted client and authority temporarily change the player capsule's `Pawn` response to `Overlap`, allowing the dash to pass through enemies and other Pawns. Before RootMotion starts, a Pawn-ignoring capsule sweep truncates travel against world obstacles, then the endpoint solver prefers a safe location beyond an overlapping enemy and falls back in front when wall clearance is insufficient. RootMotion keeps the configured speed and scales its duration to the resolved distance; end-of-dash depenetration remains only an emergency guard for dynamic collision changes or prediction disagreement.
* Dash invincibility is an explicit predicted/authority window controlled by `InvincibilityDuration` (default `0.15s`). The server-side damage executions reject targets with `State.Invincible`; the window can expire before movement ends and is always removed on cancellation or prediction rejection.
* Build candidate continuity now recognizes `Build.Dash` alongside Fire, Lightning, Crit, and Shield.
* The asset setup generated `GA_DashLightningTrail`, `BP_ArenaDashTrailArea`, both Upgrade DataAssets, two independent placeholder icons, and `GCN_DashLightningTrail_Active`. The link step connected `GE_Damage`, the Area class, and both GameMode UpgradePool entries; the 2026-07-16 setup run completed successfully and Content Validation covered all seven Dash assets without errors.
* Missing verification: generator idempotency, cooldown values, cancellation boundaries, trail geometry/damage cadence, Cue placement/lifetime, overlapping Cue independence, dual-view presentation, prediction reconciliation, and two-player authority/uniqueness. These runtime checks remain intentionally deferred in `PENDING_VERIFICATION.md`.

### Shield — Implemented

* `F` commits configured cost/cooldown and applies a runtime Shield amount through the SetByCaller-driven `UArenaGameplayEffect_ShieldGrant`.
* Incoming damage consumes Shield before Health through the shared AttributeSet damage pipeline.

### Shield Build — Partial

* `DA_Upgrade_ShieldAmount` grants `Build.Shield` and adds `20%` to future Shield casts per stack, capped at three stacks. `UArenaGameplayAbility_Shield` reads permanent PlayerState upgrade data on the predicted client and authority, floors the final `30 × (1 + bonus)` value, and does not retroactively change current Shield.
* When authority-side Damage changes Shield from positive to zero and the owner survives, `UArenaAttributeSet` routes one `Trigger.OnShieldBreak` event to the victim ASC. Direct Shield attribute edits, hits against an already empty Shield, and lethal hits do not produce the event.
* `UArenaGameplayAbility_ShieldBreakBlast` is a ServerOnly passive granted by `DA_Upgrade_ShieldBreakBlast`. It uses the upgrade DataAsset as its AbilitySpec SourceObject and applies `Damage.Physical + Damage.Secondary` through `GE_Damage` to living, non-invincible enemies within 300 units, preserving AttackPower, Crit, Defense, Shield-first, OnCrit, and OnKill behavior.
* Generator configs create the Shield grant GE, passive Ability, two upgrade DataAssets, independent placeholder icons, and `GCN_ShieldBreak_Burst`; they connect `GA_Shield` and append both upgrades to UpgradePool idempotently. C++ compilation, editor generation, PIE, multiplayer, prediction reconciliation, and Cue presentation remain pending.

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
* Build-asset automation lives under `Content/Python/build_assets`, with shared tools, category generators for Upgrade DataAssets, native GameplayEffect/GameplayAbility Blueprint children, looping/burst/damage-number GameplayCues, and a separate Ability/GameMode/wave link step. Root-level `setup_build_assets.py` remains as the single one-click entry point.
* Fire/Lightning Upgrade, status GE and persistent Cue assets are present at their existing paths. The refactored category scripts and both orchestrator entry points still require an Unreal Editor idempotency regression pass.
* Verification is deferred for damage stacks, first-hit ordering, shared two-player vulnerability, refresh/death cleanup, and replicated Cue presentation.

### Fire + Lightning Overload — Partial

* `UArenaGameplayAbility_Overload` is a ServerOnly event-triggered passive that listens for typed Lightning damage, requires `Upgrade.Combo.Overload`, checks the pre-hit `Status.Burning` snapshot, and ignores `Damage.Secondary` to prevent recursive explosions.
* The passive reads explosion damage from its granted Upgrade DataAsset `SourceObject`; `AArenaGameMode` now preserves that SourceObject when granting upgrade abilities, so runtime behavior does not hard-code an Upgrade ID.
* A successful trigger emits `GameplayCue.Combo.Overload` at the enemy location. Its dedicated `GCN_Overload_Explosion` burst combines enlarged Fire and Lightning Niagara systems at the same world-space point so Overload is visually distinct from a normal Lightning hit. The ability applies `Damage.Lightning + Damage.Secondary` through the existing `GE_Damage` pipeline to living, non-invincible `AArenaEnemyCharacter` targets within 300 units. Burning is not consumed, killing Lightning hits remain eligible, and the explosion inherits AttackPower, Crit, Defense, Shocked vulnerability, and Shield-first handling.
* `UArenaGameplayEffect_OverloadLockout` uses one-second `AggregateBySource` active effects on each target, allowing different players to trigger independently while limiting each source/target pair.
* The build-asset generator configs create `GA_Overload`, `GE_Status_OverloadLockout`, `DA_Upgrade_Overload`, an independent placeholder icon asset, and `GCN_Overload_Explosion`; they also connect the Ability classes, append the legendary upgrade to UpgradePool, and configure prototype Wave 4 when run in the editor.
* Burst Cue generation and an idempotent second generator run are verified in the editor commandlet. Single-player PIE has verified Fireball applying `Status.Burning`, LightningStorm triggering the combined Fire + Lightning Overload burst on the primary dummy, and the 300-unit radius boundary damaging the 250-unit dummy while excluding the 350-unit dummy. Missing verification remains for lockout cadence, killing-blow behavior, damage-formula inheritance, replicated dual-element burst presentation, two-source lockout independence, and the four-wave progression.

### Upgrade and Overload Test Harness — Partial

* `AArenaGameMode` retains editor-only ordered `DebugStartingUpgrades` and now exposes `TryGrantDebugUpgrade()` for test actors. Both paths execute the formal eligibility, GameplayEffect, AbilitySpec SourceObject, replicated loose-tag, PlayerState stack-recording, and resource-restoration rules on the authority; production defaults remain disabled.
* `AArenaUpgradeTestPickupActor` is a replicated, persistent editor test pickup. It renders an enlarged rotating sphere plus camera-facing ASCII upgrade name/effect text on each local client, and applies distinct stat/build colors to both text and a per-instance sphere MID to avoid the default TextRender font's missing CJK glyphs. It grants only to living players on the server, preserves the actor after success or rejection for stack/dependency testing, and tracks a short per-player overlap guard. The base Blueprint exists; the revised presentation and complete 15-instance level placement remain pending compile/script/PIE verification.
* `UArenaGameplayEffect_OverloadTestAttributes` initializes a test enemy through GAS with `5000 Health`, zero Shield/Defense/MoveSpeed/AttackPower, allowing repeated Burning, Storm, Overload lockout, and radius checks without modifying attributes directly.
* `Content/Python/overload_test/setup_overload_test.py` idempotently configures `BP_ArenaGameMode_OverloadTest`, `GE_Test_OverloadDummyAttributes`, `BP_ArenaEnemy_OverloadDummy`, and `BP_ArenaUpgradeTestPickup`. With `Lvl_OverloadTest` already open, it disables automatic starting upgrades, scans a deterministic candidate grid for 15 non-overlapping positions that sweep to the same floor band, cleans partial results on failure, and rebuilds the grounded primary/250/350 stationary dummies.
* The generator deliberately never creates, duplicates, loads, or switches maps. The user creates `Lvl_OverloadTest` through the editor's native `Save Current Level As` flow, then reruns the script while that map is open. This avoids the previous Inactive World and World Partition External Actor lifetime failures.
* The original three-dummy single-player PIE pass remains verified for Burning, the visible Overload burst, and the 250/350-unit damage boundary. The new generic pickup grid, stack limits, dependency rejection/retry, and two-player independent ownership remain pending.

### OnKill Energy Recovery — Partial

* `UArenaGameplayAbility_EnergyOnKill` is a ServerOnly event-triggered passive that accepts authoritative `Trigger.OnKill` events for `AArenaEnemyCharacter` targets and remains blocked while the source player is dead.
* The passive reads its Upgrade DataAsset from the AbilitySpec `SourceObject`, looks up the permanent stack count on `AArenaPlayerState`, and applies `NumericValue × StackCount` through `UArenaGameplayEffect_EnergyRestore` and `SetByCaller.Recovery.Energy` instead of writing the attribute directly.
* Generator configs define `DA_Upgrade_EnergyOnKill`, `GA_EnergyOnKill`, `GE_Trigger_EnergyOnKill`, an independent placeholder icon, and an idempotent UpgradePool entry. The Common upgrade restores `10/20/30` Energy across three stacks and has no damage-type routing requirement.
* Missing verification: compile/UHT, generator idempotency, direct/periodic/Secondary kill attribution, stack scaling, Energy clamp behavior, duplicate prevention, and two-player last-hit ownership.

### Crit Build — Partial

* `UExecCalc_Damage` performs the only authority-side critical roll using strict `< CritChance`, then marks the current independent Spec with `Damage.Critical`; Burning's fixed periodic execution remains outside this critical path.
* `UArenaAbilitySystemComponent` routes `Trigger.OnCrit` only after Shield or Health actually loses value. BasicAttack, Fireball, LightningStorm, Overload Secondary, and pure Shield absorption are eligible, while each target in a multi-target hit resolves independently.
* `DA_Upgrade_CritChance` applies `+0.05 CritChance` per stack through `UArenaGameplayEffect_CritChanceUpgrade` and a generic `SetByCaller.Upgrade.NumericValue`, grants `Build.Crit`, and caps at five stacks.
* `UArenaGameplayAbility_EnergyOnCrit` reads `DA_Upgrade_EnergyOnCrit` from its AbilitySpec SourceObject and restores `5/10/15 Energy` through the existing Instant Energy recovery GE. The passive is ServerOnly, requires the permanent PlayerState upgrade stack, and accepts Secondary critical damage by design.
* Authoritative damage now carries actual Shield plus Health loss through the project batch feedback payload. `UArenaHitReactionComponent` creates the existing local non-replicated number Actor once per settlement; critical numbers use a larger gold presentation and Health delegates no longer create duplicate numbers.
* Generator configs define both Crit upgrades, their GE/Ability Blueprint children, placeholder icons, two legacy damage-number Cue assets, Ability binding, and idempotent UpgradePool entries. The narrow Editor build/UHT succeeds, and the unified generator completed two consecutive passes over all 57 generated or linked assets without a Python exception or duplicate asset. Manual field inspection, PIE behavior, multiplayer ownership, and presentation verification remain pending.

## UI and Combat Feedback

### GameplayCue Routing - Partial

* Native Cue tags and server-confirmed dispatch exist for all five player abilities, enemy melee activation, physical/fire/lightning hits, and normal/critical damage numbers.
* LightningStorm Cast and Active Cues clear the activation prediction key inside their server-confirmed dispatch scope, so the owning client and simulated clients both receive the authoritative storm presentation exactly once.
* Shield, Dash, and LightningStorm use paired server Add/Remove Cue lifetimes; Shield only adds on the zero-to-positive transition and removes on depletion or death. Dash and Shield explicitly attach to the Avatar root instead of the Manny skeletal mesh so imported mesh rotation/location offsets cannot displace directional or centered Niagara effects.
* Successful damage preserves every settlement as an independent `FArenaDamageFeedbackData` item but sends all items queued for the same target and Tick through one unreliable batch Multicast. Each client explicitly plays the existing physical/fire/lightning Cue, then one result Cue, then one local number/presentation pass; this keeps fixed ordering without relying on tag-container iteration and prevents concurrent direct, periodic, and synergy hits from exhausting the default two-RPC-per-net-update limit.
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

### Damage Feedback Foundation — Partial

* `UArenaAttributeSet` classifies each authority-confirmed Damage Meta Attribute settlement as `ShieldOnly`, `ShieldBreak`, `HealthOnly`, or `ShieldBreakWithHealthDamage` from the actual pre/post Shield and Health values. Zero-loss and invincible results do not enter the feedback path, while gameplay event order and lethal `Trigger.OnShieldBreak` suppression remain unchanged.
* `FArenaDamageFeedbackData` carries actual Shield/Health loss, a server-clamped Health damage ratio, source-location validity, and the existing GameplayCue context. `UArenaAbilitySystemComponent` retains one unreliable per-target/per-Tick batch Multicast and dispatches the element/result/presentation sequence locally.
* `UArenaHitReactionComponent` is created by `AArenaCharacterBase` for players, enemies, and Bosses. It lazily reuses Mesh MIDs, exposes configurable shield/health flash parameters, sound and CameraShake references, generates one existing damage-number Actor with resource-specific colors, and offsets concurrent numbers across five lanes. Dedicated Server paths create no presentation objects.
* Only a locally controlled damaged player reaches `AArenaPlayerController` and `UArenaPlayerHUDWidget`. The controller converts the captured source position through the local camera yaw; the HUD reuses optional `DamageDirectionIndicator` and `ShieldBreakText` controls, creates lightweight Canvas fallbacks when they are absent, and exposes `On Damage Feedback` for Blueprint animation without owning gameplay state.
* Native result tags and idempotent Burst Cue configs exist for ShieldHit, ShieldBreak, HealthHit, and the combined ShieldBreakHealthHit. `ProjectArcaneArenaEditor Win64 Development` builds successfully after adding the explicit `SlateCore` dependency required by the public damage-number Widget API. The unified asset generator completed twice without duplicate assets, created the four result Cue assets, and connected the shared damage-number Actor class on Player, melee/ranged Enemy, and Boss Blueprints. Audio, CameraShake classes, material parameter support, HUD widget bindings, PIE behavior, and multiplayer ownership remain unverified.
* Runtime verification is intentionally deferred for the current development pass. The complete single-player, periodic-damage, Boss, Listen Server, Dedicated Server, ownership, and presentation checks remain recorded in `PENDING_VERIFICATION.md`; this feature therefore stays `Partial` rather than `Verified`.

### Enemy Health Bar and Damage Numbers — Partial

* Enemy health bars observe the enemy AttributeSet and hide during death handling.
* Local, non-replicated damage-number Actors are spawned by server-confirmed batched feedback and display actual Shield plus Health loss without owning damage state. Critical numbers use a larger gold style; Shield, Health, break, and combined outcomes use distinct base colors, and Health delegates remain responsible only for bars/value notifications.

## Enemies and Game Loop

### Local Mass Visual Cluster — Implemented

* `AMyMassClusterActor` creates a lightweight HISM-backed Mass Entity cluster for presentation only; it does not own collision, damage, or authoritative gameplay state.
* `UMyMassMovementProcessor` executes in standalone, server, and client Worlds so every local player sees the cluster move without replicating per-instance transforms.
* Initial positions, velocities, and maximum speeds use a configurable fixed `RandomSeed`, giving each World the same starting cluster state while simulation remains local.
* Verification pending: two-player PIE should confirm both windows animate the cluster and begin from matching layouts.

### Enemy GAS Character and Primary Attack AI — Partial

* `AArenaEnemyCharacter` owns a replicated ASC and AttributeSet, grants configured startup abilities on the server, exposes a death delegate, and handles collision/movement/UI shutdown on death.
* `AArenaEnemyAIController` runs a low-frequency server-only target/chase/primary-attack loop, selects the nearest living player from GameState PlayerArray, reads attack range and attack-path eligibility from the configured primary Ability CDO, and freezes path movement while `State.Attacking` is active. Attackable targets use the attack range as the path acceptance radius; blocked targets use a `5`-unit acceptance radius so enemies continue navigating around walls instead of treating an in-range but hidden player as already reached.
* `AArenaEnemyCharacter` treats the first `UArenaGameplayAbility_EnemyAttackBase` subclass in `StartupAbilities` as its primary attack. Generic activation, range lookup, and cancellation support melee and ranged enemies while the previous melee-specific interfaces remain available for Blueprint/code compatibility.
* Target validity is checked before the attacking-state freeze; when the locked player dies, the server cancels the active primary attack, clears focus, and immediately selects the nearest remaining living player.
* Chase movement disables overlap-expanded acceptance so the AI reaches the same center-to-center distance used by the authoritative attack range check.
* `UArenaGameplayAbility_EnemyAttackBase` owns the shared ServerOnly target lock, Commit, replicated `State.Attacking`, Montage, authority release delay, release-time revalidation, cancellation, and cleanup lifecycle. `UArenaGameplayAbility_EnemyMeleeAttack` keeps its existing serialized tuning fields and applies one physical `GE_Damage` hit through the shared release path.
* `UArenaGameplayAbility_EnemyRangedAttack` uses a `950` range, `1.5x` Montage play rate, Montage-end authority release, no post-animation fixed delay, and a `1.6s` cooldown. AI stop/attack decisions and Ability activation share the same `16`-unit Sphere Sweep from the actual Projectile spawn offset to the target aim offset, ignoring players while detecting scene blockers. This prevents the controller from stopping on a generic sight result when the real Projectile path is obstructed. The server launches when the Montage enters `OnBlendOut`; the locked target must remain alive, while subsequent movement and obstruction are resolved by Projectile travel and collision.
* `AArenaEnemyProjectile` travels at `900`, expires after `3s`, blocks on world obstacles, ignores enemies, and can be intercepted by any living `AArenaPlayerCharacter`. It applies at most one `Damage.Physical` `GE_Damage` hit; an invincible Dash player consumes the Projectile while the existing ExecCalc rejects damage.
* Melee attacks that lose range or sight during windup miss without refunding cooldown. Ranged attacks guarantee launch after a valid committed windup while still cancelling if the locked target dies; enemy death or Stun cancels the active Montage/Delay and removes `State.Attacking`.
* Dead or stunned enemies stop movement and ability execution; MoveSpeed remains GAS-driven.
* `ABP_ArenaEnemy` uses replicated GroundSpeed for movement state, while `AM_EnemyMeleeAttack` and `AM_EnemyDeath` provide attack/death presentation through the existing DefaultSlot.
* `GA_EnemyMeleeAttack` is configured with `AM_EnemyMeleeAttack`, Montage Play Rate `1.0`, and Hit Delay `0.35`.
* PIE smoke verification: three melee enemies spawned, chased, attacked through the server damage path, and all three emitted server death notifications. In two-player PIE, enemies abandoned a dead player and continued attacking the remaining living player.
* Q/Fireball, E/Dash, replicated attack presentation, and the remote-client Dash Montage have been observed in multiplayer PIE.
* Ranged C++ gameplay and generated editor assets are implemented. Single/two-player PIE behavior remains unverified; the Elite archetype is still missing.

### Boss Foundation — Partial

* `AArenaBossCharacter` reuses the replicated enemy ASC, AttributeSet, server AI, movement, damage feedback and tag-driven death lifecycle while exposing a configurable Boss display name and disabling the duplicate world-space health bar by default.
* `UArenaGameplayEffect_BossAttributes` supplies the first-stage GAS baseline of `1200 Health`, `10 AttackPower`, `5 Defense`, `300 MoveSpeed`, zero Shield/Energy and zero CritChance. `UArenaGameplayEffect_BossGroundSlamCooldown` supplies a five-second Duration and `Cooldown.Enemy.Boss.GroundSlam`.
* `UArenaGameplayAbility_BossGroundSlam` extends the shared enemy attack lifecycle. It commits on authority, locks a fixed ground location, owns a replicated warning Cue, and applies one independent `Damage.Physical` `GE_Damage` Spec to each eligible player still inside the `300` radius at the `1.2s` release time.
* GroundSlam continues to use `State.Attacking`, server Montage timing and the existing death/stun/target-loss cancellation path. Its warning Cue is removed on impact, cancellation or Ability end so delayed damage and presentation do not survive an interrupted attack.
* `AArenaGameState` now replicates `ActiveBoss`; each local `AArenaPlayerController` binds the player HUD to the Boss ASC. The HUD observes Health/MaxHealth delegates and `State.Dead`, supports optional Blueprint controls, and creates a top-center fallback Boss bar when those controls are absent.
* Boss waves now validate exactly one `AArenaBossCharacter` entry with count one before entering Combat. WaveManager sets/clears `ActiveBoss`, skips the ordinary Health/Energy Pickup DropTable for Boss deaths, and retains the existing final-wave Victory owner.
* `Content/Python/boss/setup_boss_foundation.py` is an idempotent post-build setup flow for copied Manny/Wukong/Niagara direct assets, Boss GA/GE/Cue/Character Blueprints and one final Boss wave after all existing normal waves. It preserves an expanded normal-wave flow, appends the Boss when absent, and updates an existing unique final Boss wave in place. Editor asset generation and consecutive-run idempotency remain pending verification.
* Two-player Listen Server verification confirmed one authoritative Boss, independent per-player GroundSlam range results, retargeting after the current player target dies, and matching Boss Health, Cue, death and Victory state on Host/Client. GroundSlam remained readable in both top-down and third-person views. Single-player full-flow, invalid Boss-wave configuration and interrupted-windup cleanup checks remain pending.

### Boss Behavior Tree Decision Skeleton — Partial

* `AArenaBossAIController` is a server-only Boss controller that starts its configured Behavior Tree only during `Combat` while the Boss can act. Death, stun, terminal phases, unpossession and destruction stop the Brain and movement, clear Focus/Blackboard/CombatTarget, and cancel the active primary attack. Regular melee and ranged enemies retain `AArenaEnemyAIController`.
* Boss AI now has a configurable `InitialAbilityDelay` of `3.0s`. The Behavior Tree may acquire a target and Chase immediately, while every GAS Ability branch remains ineligible until the authority-side opening buffer expires; this does not consume or fabricate an Ability cooldown.
* Boss locomotion preserves the Behavior Tree's existing `Move To` path behavior while disabling character controller-yaw and enabling CharacterMovement orientation at `720 deg/s`, so NavMesh movement faces the current path velocity; the attack Task still snaps horizontal facing to the validated target before GroundSlam activation.
* `UBTService_ArenaBossUpdateTarget` selects the nearest living PlayerState Pawn every `0.2s`, retains a valid current target until another player is at least `150` units closer, locks a living target during an active attack, and cancels the old attack before replacing an invalid target. Blackboard `TargetActor` stays synchronized with the Boss `CombatTarget`.
* `UBTDecorator_ArenaBossCanActivateAbility` resolves exactly one granted AbilitySpec by exact AssetTag, then checks the Combat phase, target validity, Boss state, attack range/path and GAS `CanActivateAbility()`. Its interval reevaluation supports aborting a lower-priority Chase branch when GroundSlam becomes available.
* Instanced `UBTTask_ArenaBossActivateAbility` activates the exact Spec Handle, waits only for that Spec's `OnAbilityEnded`, treats cancellation as failure, handles synchronous end during activation, and unbinds before canceling on Behavior Tree abort.
* Single-player PIE has verified the core Behavior Tree loop: `TargetActor` acquisition enters Chase/MoveTo, path-facing locomotion does not remain sideways/backwards or stall, GroundSlam interrupts Chase in range, cooldown returns the Boss to Chase before reevaluation, and wall obstruction continues NavMesh pathfinding instead of idling. Target invalidation, interruption cleanup and two-player hysteresis remain pending.
* `Content/Python/boss/setup_boss_decision.py` created and saved `BP_ArenaBossAIController`, `BB_ArenaBoss`, `BT_ArenaBoss` and the updated Boss Character, linking every reference exposed by the active Python reflection layer without overwriting a manually authored tree graph. This UE Python build hides Blackboard Key types and `BehaviorTree.BlackboardAsset`, so those two settings use the documented manual fallback. The manually connected stage-two-A GroundSlam/Chase/Wait graph is now exercised by the verified single-player core loop; generator idempotency remains pending.

### Boss Charge — Partial

* `UArenaGameplayAbility_BossCharge` implements a ServerOnly, locked-direction charge with a default `0.8s` fixed world-space telegraph, `1200` speed GAS RootMotion, `350-900` activation band, `150` target overshoot and `1050` maximum travel distance. Commit fixes the start, direction and endpoint; later target movement does not steer the Boss.
* Charge temporarily changes the Boss capsule's Pawn response to overlap, continuously sweeps the travelled path with a `110` radius and applies at most one independent `Damage.Physical` `GE_Damage` Spec per living player. Players do not stop the charge, while a blocking world hit ends it and emits a wall Impact Cue.
* Charge cleanup is centralized in `EndAbility`: telegraph/sweep timers, replicated Telegraph and Active Cues, temporary collision response, residual movement, RootMotion task state, Montage task state, hit cache and `State.Attacking` are cleared for normal completion, wall impact, stun, death, terminal phase or cancellation.
* `UArenaGameplayAbility_EnemyAttackBase` now exposes a reusable authority validation/Commit/state entry and optional minimum attack range. The Boss BT activation Decorator checks both minimum and maximum ranges, and `AArenaEnemyCharacter::CancelPrimaryAttack()` cancels every active `EnemyAttackBase` Spec so multi-skill Boss cleanup is not limited to the first Startup Ability.
* Native tags now include `Ability.Enemy.Boss.Charge`, `Cooldown.Enemy.Boss.Charge` and Telegraph/Active/Impact GameplayCues. `AArenaGameplayCueNotify_BossChargeTelegraph` converts replicated fixed location, direction and distance parameters into a world-space Niagara transform; the runtime module now explicitly depends on Niagara.
* `Content/Python/boss/setup_boss_charge.py` provides idempotent post-build creation and configuration for the copied Wukong evade animation, no-Root-Motion Montage, Charge GA/GE, three Boss-local Niagara/Cue assets and one StartupAbilities entry. The first editor run created and saved the Charge assets and updated the Boss Character. The generator now creates new Active/Impact Cues directly from native Looping/Burst classes so future clean runs cannot be transiently registered under template tags; a post-fix rerun and network verification remain pending.
* The Charge branch is manually wired and its single-player core behavior is partially verified: the `350-900` band, locked dodgeable path, one-hit-per-player guard, wall/endpoint completion and cooldown fallback work. AbilitySystem debugging confirmed `State.Attacking` exists only during Charge and clears before the Boss resumes Chase/Attack, with no persistent RootMotion or BT Task blockage. Follow-up source changes ignore walkable-floor collision hits, enlarge/raise the telegraph, and require both a direct NavMesh corridor and a reduced-capsule collision sweep before Commit, preventing obvious upper/lower-floor or wall-separated targets from consuming the Charge; these path changes still require PIE verification.

### Boss FireZone — Partial

* `UArenaGameplayAbility_BossFireZone` adds a ServerOnly fixed-location fire-area attack with a `600-1200` activation band. Authority resolves the target's ground point before Commit, locks it for a `1.0s` telegraph, and keeps the committed location even if the original target moves, dies or later loses line of sight.
* FireZone uses the shared enemy attack lifecycle for Commit, Montage, cooldown and `State.Attacking`, while adding replicated `State.Casting` only during the windup. Stun, Boss death, terminal phase, Montage interruption and BT abort cancel an uncommitted release without spawning a delayed Area.
* `AArenaBossFireZoneArea` is a replicated static Actor whose server immediately applies the first `Damage.Fire` tick and then runs a `0.5s` timer for up to ten ticks across its five-second lifetime. Strict horizontal radius, vertical tolerance, living-player filtering and one-target-per-tick guards precede the standard `GE_Damage` Shield/Defense/Crit/death pipeline.
* Overlapping FireZone Areas remain independent. Each replicated Area drives its own local non-replicated Active Cue using itself as the Cue target, while source death, source destruction or leaving `Combat` destroys every associated Area and removes only that Area's presentation.
* Native tags, the eight-second cooldown GE and `AArenaGameplayCueNotify_BossFireZoneRadius` support dynamically scaled Telegraph and Active Niagara. The Active Cue replays its fire Niagara every `0.8s` as well as when the component completes, covering systems whose internal burst ends while the System remains active; an always-visible ring mesh uses a persistent unlit glow material and the authoritative radius. `Content/Python/boss/setup_boss_fire_zone.py` provides idempotent post-build creation for animation/Montage, GA/GE, Area Blueprint, two Boss-local Niagara/Cue assets and one Boss StartupAbilities entry without modifying the Behavior Tree graph.
* The editor setup run completed without Python errors and saved the animation, Montage, both Niagara systems, GA, cooldown, Area, both Cue Blueprints and updated Boss StartupAbilities. C++/UHT compilation, two consecutive generator runs and manual `GroundSlam -> Charge -> FireZone -> Chase -> Wait` wiring are verified. Single-player checks passed for fixed placement, immediate first tick, `0.5s` periodic damage, five-second lifetime, Shield/Dash interaction and cancellation cleanup; multiplayer authority, overlapping-area independence, remaining target/height edge cases and dual-view VFX readability remain pending.

### Boss Phase And Player Scaling System — Partial

* `AArenaBossCharacter` now owns an authority-only Health-driven phase state machine. It initializes `Boss.Phase.One`, advances at `70%` and `35%` MaxHealth, never regresses after healing, and resolves multi-threshold damage directly to the final reached phase without replaying intermediate transitions.
* GroundSlam requires the parent `Boss.Phase` tag and remains valid in all phases. Charge and FireZone require the same parent tag but are blocked by `Boss.Phase.One`, so the existing Behavior Tree and GAS `CanActivateAbility()` path enforce phase unlocks without graph changes or false cooldown consumption.
* `UArenaGameplayEffect_BossEnrage` is an Infinite GE that multiplies AttackPower by `1.30`, MoveSpeed by `1.20`, grants `Boss.State.Enraged`, and owns `GameplayCue.Boss.Enraged.Active`. The Boss stores its ActiveEffectHandle and removes the GE, Cue and phase tags on death, destruction or leaving `Combat`.
* Phase 2 and Phase 3 execute one authoritative `GameplayCue.Boss.Phase.Transition` with the resulting phase number in `RawMagnitude`. `AArenaGameplayCueNotify_BossEnraged` attaches the active Niagara to the Boss and can restart non-looping internal bursts while the GE remains active.
* Boss HUD observes the three replicated phase tags. An optional `BossPhaseText` displays phase-specific text and color; existing HUD Blueprints fall back to merging the phase into `BossNameText`, while the runtime fallback panel includes a separate phase row.
* `Content/Python/boss/setup_boss_phase_system.py` provides idempotent post-build creation and linking for `GE_Boss_Enrage`, two Boss-local Niagara systems, Transition/Enrage Cue Blueprints and Boss threshold/effect configuration without modifying `BT_ArenaBoss`. The editor loaded the new native types and two consecutive setup runs saved the same six assets without `_1/_2` duplicates or script errors.
* Single-player PIE has verified the primary phase path: Phase 1 only permits GroundSlam, reaching `70%` unlocks Charge and FireZone, reaching `35%` enters Phase 3 with visible Enraged presentation, and one large hit can skip Phase 2 and enter Phase 3 directly.
* Active GroundSlam/Charge/FireZone is not interrupted by a threshold crossing. Phase 3 has verified AttackPower `10 -> 13`, MoveSpeed `300 -> 360`, and one Enrage GE/Tag/Cue instance. Boss death and Victory remove the phase state and Enrage presentation without residue.
* Direct lethal damage from Phase 1/2 has been verified to skip the Phase 3 transition and Enrage entirely.
* Phase 3B source now snapshots all connected `AArenaPlayerState` instances with a valid ASC when the server spawns the Boss. Dead players and players temporarily missing a Pawn still count; the snapshot is applied once before `ActiveBoss` is published and is not recalculated after death, respawn, disconnect or Pawn changes.
* `UArenaGameplayEffect_BossPlayerCountScaling` uses `SetByCaller.Boss.MaxHealthDelta` to add the difference between baseline and scaled MaxHealth. `AArenaBossCharacter` then reuses `UArenaGameplayEffect_HealthRestore` to refill Health through the Healing meta attribute. Defaults are `1.0x` for one player and `1.75x` for two or more players in the current two-player demo.
* Initial scaling suppresses the Boss Health phase callback while MaxHealth and Health are changing, preventing the transient `1200/2100` state from entering Phase 2. Repeated initialization attempts are rejected to prevent stacked scaling.
* Initial wave startup now waits for `PostLogin` and resets its grace timer when another player joins during `Waiting`. This prevents Dedicated/Listen PIE from spawning and snapshotting the Boss before the configured local clients have registered their PlayerStates.
* `Content/Python/boss/setup_boss_player_scaling.py` provides idempotent creation and linking for `GE_Boss_PlayerCountScaling` and the Boss multiplier settings without modifying Behavior Tree, StartupAbilities or wave data.
* Phase 3B compilation, asset setup and the two-player core path are verified: the initial wave starts after both clients register, the server snapshots two valid PlayerStates, and Host/Client observe `2100/2100` Boss Health/MaxHealth. Single-player counting, duplicate-call rejection, snapshot immutability and scaled phase thresholds remain pending; Phase 3A Stun/Defeat/destruction cleanup and the full two-player Boss regression also remain pending.

### Boss Summon Minions — Verified

* `UArenaGameplayAbility_BossSummonMinions` is a Phase 3-only `ServerOnly` Boss attack. It checks target/path, remaining capacity and NavMesh/capsule-valid spawn points before Commit, then locks up to two positions, plays a `0.9s` casting window and spawns configured melee/ranged enemies in fixed order.
* `AArenaBossCharacter` owns a non-replicated weak summon registry with a default live limit of four. Death and destruction callbacks release capacity; Boss death, destruction or leaving Combat destroys every remaining summon and removes delegates/tags.
* Summons are spawned directly by the Boss and never enter `AArenaWaveManager::AliveEnemies`, so they do not change `RemainingEnemyCount`, block Boss Victory or use the normal-wave pickup path. Clients observe the existing replicated enemy Actors and server AI.
* `Enemy.Summoned`, `Enemy.Summoned.Trigger.OnKill` and `Enemy.Summoned.Trigger.OnCrit` make passive-event eligibility explicit. The authoritative damage route leaves ordinary targets unchanged and only emits summon OnKill/OnCrit events when the corresponding leaf tag is present; elemental/status and typed damage events remain unchanged.
* Native cooldown and Cast/Spawn GameplayCue tags, `setup_boss_summon_minions.py`, the four unique Boss StartupAbilities and the manual `GroundSlam -> Charge -> FireZone -> SummonMinions -> Chase -> Wait` BT order are configured and exercised in PIE.
* Verification confirmed Phase 3-only activation, one melee plus one ranged summon per cast, a four-summon cap and immediate capacity release on summon death. Summons do not change `RemainingEnemyCount` or drop ordinary recovery pickups.
* Boss death immediately enters Victory and removes every surviving summon. Stun, Boss death and terminal-phase cancellation during the windup do not produce delayed summons.
* Burning, Shocked, Overload, OnKill, OnCrit, Dash Trail and ShieldBreakBlast have been verified against summoned enemies.

### Boss Intro Synchronization — Verified

* `EArenaGamePhase::BossIntro` and replicated `FArenaBossIntroTiming` provide one authority-owned five-second Intro clock based on `AGameStateBase::GetServerWorldTimeSeconds()`. Boss waves publish the scaled `ActiveBoss` and enemy count before entering Intro; normal waves still enter Combat directly.
* Intro freezes player CharacterMovement and Sprint on authority and clients, ignores local move/look/view-toggle input, cancels active `Ability.Type.PlayerActive` abilities, blocks new player-active and `Ability.Enemy` activations in the shared Ability base, and rejects Damage meta-attribute consumption without removing passive upgrades or persistent Shield. Boss phase initialization and Health-threshold evaluation are explicitly limited to Combat, so Intro cannot publish `Boss.Phase.One` early.
* Each local `AArenaPlayerController` independently selects the closest placed `CameraActor` tagged `BossIntroCamera`, blends in and returns to its current Pawn during the replicated `0.6s` tail. Camera transforms and top-down/third-person state are never replicated; missing cameras retain the player view and log one warning.
* Holding Space sends only held/released intent. The server owns the two-second timer, revalidates the participating PlayerState, phase and living Boss, then shortens the shared end time while retaining the blend-out window. Duplicate completed requests are idempotent.
* `UArenaPlayerHUDWidget` exposes optional Intro name/countdown/prompt/progress controls and creates a native fallback when the current WBP omits them. `AArenaBossAIController` applies a `0.5s` post-Intro ability grace before existing BT attack branches can activate.
* Boss death, direct destruction, Defeat, manager destruction and normal completion clear authority timers; local phase/Boss/timing delegate cleanup restores ViewTarget, cursor, reticle and input.
* Verification confirmed the tagged level CameraActor, authority Intro clock, movement/ability/damage freeze, Space hold skip with the retained blend-out window, local camera restoration and transition into Phase 1 Combat.

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
* Boss-wave validation and replicated `ActiveBoss` ownership are implemented in C++; the final Boss runtime flow has passed two-player Listen Server verification, while invalid configuration and setup-script idempotency checks remain pending.
* Verification: the previously saved three-wave asset completed the formal single-player `Wave 1 -> choice -> Wave 2 -> choice -> Wave 3 -> Victory` flow. The ranged-enemy setup and build-asset link scripts preserve existing per-wave metadata while replacing the first four `Enemies` arrays with `3M / 3M+2R / 4M+3R / 5M+4R`; editor execution and the expanded normal-wave flow followed by the final Boss remain pending.

### Server-Authoritative Pickup Drops — Partial

* After a tracked enemy is removed from `AArenaWaveManager::AliveEnemies`, the authority uses an independent deterministic random stream derived from the match seed to roll at most one pickup without changing the upgrade-candidate random sequence.
* `UArenaPickupDropTableDataAsset` provides a global drop chance and weighted Pickup Blueprint classes. Missing tables, empty entries, invalid weights, and spawn failures disable or skip drops without blocking enemy counts, Upgrade, or Victory.
* Replicated `AArenaPickupActor` instances use server-only Pawn overlap and shared first-eligible-player ownership. Dead players, full resources, and zero resource maxima do not consume the pickup; successful consumption is replicated through authority destruction and unused pickups expire after a configurable lifespan.
* Health pickups apply `SetByCaller.Recovery.Health` through `UArenaGameplayEffect_HealthRestore` and the existing Healing meta-attribute path. Energy pickups reuse `UArenaGameplayEffect_EnergyRestore`; neither pickup writes attributes directly.
* `Content/Python/pickup_items` contains an idempotent asset setup flow for `BP_HealthPickup`, `BP_EnergyPickup`, `DA_PickupDropTable_Default`, and the `BP_ArenaGameMode` link. C++ compilation, editor asset generation, probability/reproducibility, resource-boundary, lifetime, dual-view, and two-player pickup behavior remain pending verification.

## Phase 4 Configured Assets

* `GE_Status_Stunned`: Blueprint child of `UArenaGameplayEffect_Stunned`; the native parent supplies a two-second Duration and `State.Stunned`.
* `GE_Cooldown_EnemyMeleeAttack`: configured with Duration `1.2` and granted tag `Cooldown.Enemy.MeleeAttack`.
* `GA_EnemyMeleeAttack`: configured with `GE_Damage`, `GE_Cooldown_EnemyMeleeAttack`, `AM_EnemyMeleeAttack`, Montage Play Rate `1.0`, and Hit Delay `0.35`.
* `BP_ArenaEnemyCharacter`: `GA_EnemyMeleeAttack` is configured in Startup Abilities. Native defaults set `AArenaEnemyAIController` and `Placed in World or Spawned` possession.
* `BP_ArenaEnemyCharacter`: `K2_OnDeathStarted` plays `AM_EnemyDeath`; its existing three-second lifespan remains the cleanup owner.
* `Content/Python/ranged_enemy/setup_ranged_enemy.py` is an idempotent, map-independent post-build setup flow for `AM_EnemyRangedAttack`, `GE_Cooldown_EnemyRangedAttack`, `GA_EnemyRangedAttack`, `BP_ArenaEnemyProjectile`, and `BP_ArenaRangedEnemy`. It registers the separate CombatMagic Manny Skeleton as a one-way compatible source on the project Manny Skeleton, then connects `GE_Damage`, the Muriel projectile particle, the ranged Montage, the blue Manny material, and a ranged-only Startup Ability. Editor execution and asset validation completed successfully.
* `DA_Waves_Prototype`: the ranged setup replaces only the first four `Enemies` arrays with `3M / 3M+2R / 4M+3R / 5M+4R`, preserving existing `SpawnInterval`, `RewardCount`, and `BossWave` values. `configure_build_asset_links.py` reapplies the same mix when the ranged Blueprint exists and otherwise warns without changing waves. The generated mix is saved; full-wave PIE verification remains pending.
* `BP_ArenaGameMode`: `DA_Waves_Prototype` is assigned to Wave Data.
* `Lvl_TopDown`: a NavMeshBoundsVolume and three TargetPoints with Actor Tag `EnemySpawn` are configured.
* `WBP_PlayerHUD`: optionally add TextBlocks named `PhaseText`, `WaveText`, `RemainingEnemiesText`, and `DefeatText`; set `DefeatText` initial visibility to Collapsed.

### Roguelike Upgrade Foundation — Partial

* `UArenaUpgradeDataAsset` defines upgrade identity, text/icon presentation, rarity, eligibility tags, granted GE/Ability, routing tags, numeric metadata, and stack limits.
* `AArenaPlayerState` owns permanent upgrade stack records, replicated selection completion, and OwnerOnly candidate arrays; local UI observes replicated state delegates.
* `AArenaGameMode` generates up to three unique eligible choices per player from a configured pool, validates the submitted ID against that player's candidates, applies the GameplayEffect/Ability/tags on the server, and waits for every participating PlayerState before starting the next wave.
* Upgrade Candidate V2 uses configurable `Common/Rare/Epic/Legendary` weights of `100/40/15/5` for deterministic weighted draws without replacement. Players who own any first-version Fire, Lightning, Crit, Shield, or Dash build tag receive one eligible same-build candidate when available; multiple owned builds share one combined guaranteed pool, and all slots are shuffled by the same authority-owned random stream.
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
* Deferred verification: complete Overload Burning retention, one-second per-source/per-target lockout, killing-blow explosions, Secondary recursion prevention, Shocked amplification, burst Cue replication, two-player source independence, and the generated four-wave Victory flow. The single-player Fireball-to-Burning trigger, visible burst, and 250/350-unit radius boundary are verified.
* Deferred verification: compile and generate the Shield Build assets, then verify `30/36/42/48` Shield scaling, damage-only `Trigger.OnShieldBreak`, nonlethal break blasts, standard physical damage inheritance, prediction reconciliation, dual-view Cue placement, and two-player authority/ownership behavior.
* Deferred verification: compile and generate the Dash Build assets, then verify `Trigger.OnDashEnd` cancellation boundaries, three-stack cooldown scaling, one authority Trail Area per completed Dash, four intended damage ticks, Lightning synergy routing, dual-view Cue geometry, and two-player prediction/replication behavior.
* A feature must explicitly say `Verified` before this log should be treated as proof of completed PIE, multiplayer, or packaged-build testing.
