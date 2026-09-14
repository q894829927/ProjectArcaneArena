---
name: project-change-plan
description: Plan a ProjectArcaneArena gameplay change before implementation. Use when the user wants to add or change an ability, enemy, upgrade, boss mechanic, inventory feature, networking behavior, UI flow, or other gameplay feature and wants architecture impact analysis without modifying gameplay code yet.
---

# ProjectArcaneArena Change Plan

Use this skill before implementing a gameplay change.

The default output is an implementation plan and learning analysis, not code changes.

## Read first

Read:

- `AGENTS.md`
- `IMPLEMENTED_FEATURES.md`
- `PENDING_VERIFICATION.md`
- `Docs/ProjectLearning/LEARNING_RULES.md`
- relevant learning notes;
- real current source/config/assets related to the requested feature.

Confirm current branch, HEAD and working-tree status.

## Plan from the existing flow

First identify the current flow that the requested change extends.

Example:

```text
Current Fireball
→ target gathering
→ ability activation
→ server projectile
→ hit
→ damage
→ upgrade/status hooks
→ cue/presentation
```

Then identify the smallest correct insertion points.

## Required output

### 1. 需求理解

Restate the desired player-visible behavior and any important constraints.

### 2. 现有实现

Show the current relevant call chain and ownership boundaries.

### 3. 修改点

List concrete affected areas:

- C++ classes / functions;
- GameplayAbility / GameplayEffect / GameplayCue;
- DataAsset / Blueprint / Config;
- GameplayTags / Events / TargetData;
- Python Editor automation if asset generation is involved;
- UI if player feedback changes.

### 4. GAS 影响

When applicable explain:

```text
Activation
Cost / Cooldown
Attributes
GameplayEffect
GameplayTag
GameplayEvent
TargetData
Prediction
GameplayCue
```

### 5. Multiplayer 影响

Explicitly state:

```text
Local / prediction
Server validation / authority
Replication
Remote presentation
```

Do not introduce client-authoritative damage or state changes.

### 6. 数据驱动方案

Prefer extending existing DataAsset / GE / SourceObject / tag-driven patterns rather than hard-coded upgrade IDs or duplicated ability logic when the current architecture supports it.

### 7. 风险

Check for:

- duplicate predicted/server execution;
- stale TargetData;
- lifecycle/cancellation problems;
- authority violations;
- double GameplayCue/VFX;
- upgrade stacking interactions;
- status/trigger recursion;
- death/stun/invincible edge cases;
- wave/boss phase ownership;
- Blueprint/config mismatch.

### 8. 验证方案

Choose relevant levels:

```text
Compile
PIE
Standalone
Listen Server
Dedicated Server
Network Emulation
Packaged Build
```

State expected evidence for each test.

### 9. 推荐实施顺序

Give small, reviewable steps with dependencies.

## Notes

If useful, record architecture insights in `Docs/ProjectLearning/`, but do not alter gameplay implementation while this skill is active unless the user explicitly asks to proceed from planning into implementation.
