---
name: project-trace
description: Trace a concrete ProjectArcaneArena gameplay flow end to end. Use when the user asks how an action travels through input, GAS, AI, networking, damage, wave, upgrade, inventory, boss, UI, or presentation systems.
---

# ProjectArcaneArena Gameplay Trace

Use this skill to answer questions such as:

- Fireball 从按 Q 到敌人扣血；
- 敌人死亡后如何推进 Wave；
- Overload 如何被触发；
- Pickup 如何进入 Inventory；
- Client 的 TargetData 如何到 Server。

## Required context

Read:

- `Docs/ProjectLearning/LEARNING_RULES.md`
- relevant existing notes;
- relevant sections of `IMPLEMENTED_FEATURES.md` and `PENDING_VERIFICATION.md`;
- the real current source and configuration for the requested flow.

Confirm current branch and HEAD before tracing.

## Trace method

Start from the actual user-visible or engine-visible entry point and follow the code until the final gameplay result.

Prefer this structure:

```text
Input / Event
↓
Class::Function()
↓
Class::Function()
↓
GAS / AI / Framework / Inventory / Wave system
↓
Authoritative gameplay result
↓
Replication / delegate
↓
Presentation / UI
```

At every important step capture:

- file path;
- class;
- function;
- input data;
- output/state change;
- network role;
- why this step exists.

Mark steps as appropriate:

```text
[Local]
[Predicted]
[Server]
[Replicated]
[Presentation]
```

Never invent Blueprint Graph internals. Mark them `【待编辑器验证】` when only asset linkage is known.

## Cross-system checks

When relevant, explicitly inspect:

- Enhanced Input / InputTag;
- AbilitySpec / GameplayAbility;
- TargetActor / TargetData / PredictionKey;
- GameplayEffect / ExecutionCalculation / AttributeSet;
- GameplayTag / GameplayEvent / GameplayCue;
- Server RPC / RepNotify / replicated Actor;
- AI target / movement / attack;
- GameMode / GameState / Wave;
- Upgrade / SourceObject / DataAsset;
- Inventory / Pickup / Item DataAsset;
- UI delegate / replicated presentation.

## Write result

Update `Docs/ProjectLearning/19_Gameplay_Flow_Atlas.md` with the verified flow. If the flow substantially improves another module note, update that note too.

Do not modify gameplay code in trace mode.

End with:

- 一句话流程；
- 完整调用链；
- Client/Server 边界；
- 三个最关键节点；
- 仍需编辑器或 PIE 验证的部分。
