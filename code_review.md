# Code Review Guidelines

## Review Purpose

Reviews for Project Arcane Arena are bug-, risk-, and regression-first. Treat every review as a check against the project's UE5 C++ gameplay architecture, GAS correctness, and future Listen Server readiness.

The goal is not to restyle code or redesign unrelated systems. The goal is to identify concrete issues that could break gameplay, damage authority, replication, GAS state, editor workflows, or the resume-ready quality of the demo.

## Review Workflow

1. Inspect the relevant files before commenting.
2. Understand how the change fits the current architecture in `AGENTS.md`.
3. Use the local `ue56-gameplayabilities` skill guidance when reviewing GAS, AttributeSet, GameplayEffect, GameplayAbility, GameplayCue, TargetData, prediction, or replication code.
4. Prefer primary evidence from code, config, assets, or known UE/GAS behavior over speculation.
5. Report findings before summaries, praise, or implementation notes.
6. Avoid broad rewrites unless the issue cannot be fixed safely with a smaller change.

## Response Format

Use this format for review responses:

```text
Findings
- P1 - Short issue title.
  file/path:line - Explain the bug or risk, why it matters, and the expected fix.

Open Questions
- Only include questions that affect correctness or scope.

Summary
- Briefly mention what was reviewed and any residual test gaps.
```

Rules:

- Findings must be ordered by severity.
- Every finding must include a file and line reference when possible.
- If no issues are found, say that clearly and still mention untested areas or residual risk.
- Do not bury findings under a long summary.
- Do not report purely stylistic preferences unless they create a real maintainability or correctness risk.

## Severity Scale

- `P0`: Compile break, editor load failure, crash, data loss, or a change that blocks basic project use.
- `P1`: Broken gameplay loop, authority/security bug, invalid damage/death result, severe replication failure, or a regression that makes a core feature unusable.
- `P2`: Multiplayer-readiness risk, GAS lifecycle risk, prediction/TargetData risk, AttributeSet replication issue, or architecture drift that will be expensive to unwind.
- `P3`: Maintainability, testability, cleanup, or clarity issue that is worth fixing but does not currently break behavior.

## UE / GAS Review Checklist

Check these areas when they apply:

- Authority: damage, death, spawning, wave progression, upgrade application, and enemy AI must be decided by authority-side code.
- ASC ownership: player ASC and persistent player attributes should live on `AArenaPlayerState` unless there is a deliberate exception.
- ActorInfo: player GAS setup should initialize Owner/Avatar correctly in server possession and client `OnRep_PlayerState` flows.
- Attributes: replicated attributes should use `FGameplayAttributeData`, RepNotify, `GAMEPLAYATTRIBUTE_REPNOTIFY`, and standard lifetime replication.
- Attribute mutation: Health, Shield, Energy, AttackPower, Defense, and similar values should be modified through GameplayEffects, not direct gameplay-side setters.
- Damage: non-trivial damage should flow through GameplayEffect and ExecutionCalculation or a documented GAS-compatible path; shield-first and clamp behavior should be verified.
- GameplayEffects: cost, cooldown, upgrades, buffs, and debuffs should be data-driven through GE assets/specs where practical.
- GameplayTags: state, cooldown, ability identity, damage type, and phase logic should use tags rather than scattered booleans.
- Abilities: activation should handle commit failure, end/cancel cleanup, cost/cooldown, task lifecycle, and tag requirements.
- Prediction and TargetData: client-provided target data, hit results, yaw, and locations must be treated as requests and validated on the server before authority effects.
- Projectiles: damage-dealing projectiles should be server-spawned and replicated; clients should only drive input and presentation.
- GameplayCue: cues should be presentation-only and have correct Execute/Add/Remove lifecycle behavior.
- UI: UI should observe GAS delegates, tags, GameState, and PlayerState data; it should not own gameplay state.
- Replication mode: ASC replication choices should match who needs to see ActiveGE, tags, cues, and attributes.

## Testing Expectations

Recommend tests based on risk:

- Run a C++ build check after source changes.
- Test single-player PIE for movement, aiming, ability activation, damage, death, waves, and upgrades touched by the change.
- Test two-player PIE when a change touches ASC ownership, replicated attributes, TargetData, prediction, projectiles, GameState, PlayerState, upgrades, or authority decisions.
- For GAS changes, verify the relevant path directly: ability activation/commit/end, GE application/removal, AttributeSet callbacks, tag changes, cue lifecycle, and UI delegate updates.
- Mention when full multiplayer, asset, or PIE validation was not run.

## Non-Goals

Do not use a review to push:

- Account login, matchmaking, dedicated server deployment, or backend systems.
- Large inventory, trading, quest, open-world, or full animation framework work.
- Unrelated refactors or renames.
- Engine source edits unless explicitly requested.
- New third-party plugins unless explicitly approved.
