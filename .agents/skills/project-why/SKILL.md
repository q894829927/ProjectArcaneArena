---
name: project-why
description: Explain why a ProjectArcaneArena architecture or implementation choice exists. Use when the user asks why the project uses PlayerState, GAS, meta attributes, TargetData, server authority, DataAssets, GameplayTags, delegates, GameplayCues, or another specific design instead of an alternative.
---

# ProjectArcaneArena Design Reasoning

Use this skill for questions of the form “为什么这里这样设计，而不是另一种写法？”

## Read first

Read `Docs/ProjectLearning/LEARNING_RULES.md`, the current implementation around the design choice, and relevant project docs. Confirm current branch and HEAD.

## Required structure

### 1. 当前实现

State exactly what the current code does, with file/class/function evidence.

### 2. 这个设计解决什么问题

Explain the concrete project problem rather than giving only generic UE theory.

### 3. UE / GAS / Multiplayer 原因

Explain relevant engine-framework reasons such as lifetime, ownership, replication, prediction, authority, data-driven design, decoupling, or presentation separation.

### 4. 为什么不采用常见替代方案

Compare the current implementation with the most relevant alternative, for example:

```text
PlayerState vs Character
GameplayTag vs bool
GameplayEffect vs direct attribute write
TargetData vs raw client RPC
GameMode vs GameState
GameplayCue vs direct Niagara spawn
DataAsset vs hard-coded IDs
Delegate vs Widget polling
```

Do not create false tradeoffs when no meaningful alternative exists.

### 5. 对 ProjectArcaneArena 的具体价值

Tie the answer back to this project’s top-down/third-person compatibility, GAS, Roguelike builds, server-authoritative combat, wave/boss flow, or multiplayer goals when relevant.

### 6. 代价和限制

Every important architecture choice has costs. Identify additional complexity, lifecycle requirements, replication overhead, asset configuration, testing burden, or debugging cost where real.

### 7. 结论可信度

Classify important reasoning as:

- `【源码确认】` when directly evidenced;
- `【项目文档】` when stated by project guidance;
- `【推测】` when inferring developer intent.

Never claim inferred intent as source-confirmed fact.

## Notes

If useful, add the reasoning to the relevant module note in `Docs/ProjectLearning/` under a `为什么这样设计` section.

Do not modify gameplay code in why mode.
