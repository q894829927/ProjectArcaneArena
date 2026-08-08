# ProjectArcaneArena OpenCode Learning Rules

This file is loaded by `opencode.json` and defines the OpenCode-specific entry points for the ProjectArcaneArena learning system.

## Natural-language day trigger

When the user says an expression equivalent to any of the following, treat it as a request to execute that numbered day from `Docs/ProjectLearning/DAILY_PLAN.md`:

- `第一天`
- `第1天`
- `第 1 天`
- `开始第一天`
- `开始第6天`
- `继续第12天`
- `复习第21天`
- `Day 1`
- `Day 6`

Support Day 1 through Day 28. Convert Chinese numerals such as `第一天`、`第六天`、`第二十八天` to the corresponding day number.

If the requested day exists in `DAILY_PLAN.md`, do not ask the user what they want to learn. Start that day's learning workflow directly.

## Required reading for a day request

Before teaching Day N, read:

1. `AGENTS.md`
2. `Docs/ProjectLearning/DAILY_PLAN.md`
3. `Docs/ProjectLearning/LEARNING_RULES.md`
4. `Docs/ProjectLearning/LEARNING_PROGRESS.md`
5. `IMPLEMENTED_FEATURES.md`
6. `PENDING_VERIFICATION.md`
7. existing topic notes associated with Day N

Then inspect the current source/config/assets relevant to that day's topic.

Confirm the current checkout with:

```bash
git branch --show-current
git rev-parse HEAD
git status --short
```

Always study the current checkout. Do not assume `main` or `develop` contains the code currently being studied.

## Day N response contract

After locating Day N in `DAILY_PLAN.md`, produce the day's learning content in this order:

1. `今日目标` — what the user must understand by the end of the day.
2. `前置概念` — only the UE5 / GAS / Multiplayer theory needed today.
3. `源码导航` — recommended reading order with file paths, classes, and functions.
4. `核心架构` — what the subsystem solves, core classes, who creates/calls whom, lifecycle.
5. `完整调用链` — at least one real end-to-end ProjectArcaneArena flow.
6. `数据流` — where data is created, represented, sent, validated, modified, consumed, and replicated.
7. `网络角色` — Local / Owning Client / Server / Simulated Client responsibilities when relevant.
8. `UE / GAS 原理` — explain the concepts as they are actually used in this repository.
9. `为什么这样设计` — at least three design tradeoffs and alternatives.
10. `边界与验证` — distinguish implementation from verification debt.
11. `面试追问` — at least five questions from basic to deep follow-up.
12. `我的复述` — stop and let the user answer before declaring the day complete.

Do not turn a day into a passive article that skips the user's recall step.

## Source-grounding rules

Important conclusions should include, whenever available:

- source file path
- class name
- function name

Do not merely restate `AGENTS.md`, `IMPLEMENTED_FEATURES.md`, or `PENDING_VERIFICATION.md`. Verify important behavior against the current implementation.

Do not invent Blueprint graph internals that cannot be inspected. Mark uncertain items as `【待编辑器验证】` or `【推测】`.

Distinguish clearly between:

- `Implemented`
- `Partial`
- `Verified`
- `Pending Verification`

Planning text is not implementation evidence.

## Network notation

When networking is involved, use labels where applicable:

```text
[Local]
[Owning Client]
[Predicted]
[Server]
[Replicated]
[Simulated Client]
[Presentation]
```

Explicitly explain authority, RPC, replication, prediction, and validation boundaries instead of saying only that "UE/GAS handles it automatically".

## GAS notation

When GAS is involved, explain the actual role of the relevant concepts in the current flow:

- ASC
- AbilitySpec
- GameplayAbility
- GameplayEffect
- AttributeSet
- GameplayTag
- GameplayEvent
- GameplayCue
- TargetActor
- TargetData
- PredictionKey
- CommitAbility

Only discuss concepts that are relevant to the current Day N topic.

## User recall and correction

At the end of the teaching portion:

1. ask 3-5 understanding questions;
2. let the user answer first;
3. classify each answer as `正确` / `部分正确` / `错误`;
4. correct only the important misunderstandings and missing links;
5. do not automatically move to the next day.

## Note update protocol

After the user completes the recall step, update the relevant files under `Docs/ProjectLearning/`.

Use the mapping in `DAILY_PLAN.md` to decide which topic notes to create or update.

General placement rules:

- framework knowledge -> `Docs/ProjectLearning/Framework/`
- networking -> `Docs/ProjectLearning/Network/`
- GAS -> `Docs/ProjectLearning/GAS/`
- abilities -> `Docs/ProjectLearning/Abilities/`
- gameplay systems -> `Docs/ProjectLearning/Gameplay/`
- call chains -> `Docs/ProjectLearning/Flows/Gameplay_Flow_Atlas.md`
- interview questions -> `Docs/ProjectLearning/Interview/Questions.md`
- bugs/tradeoffs -> `Docs/ProjectLearning/Interview/Bugs_and_Tradeoffs.md`
- daily process -> `Docs/ProjectLearning/Daily/DayNN.md`
- progress -> `Docs/ProjectLearning/LEARNING_PROGRESS.md`

Do not create empty placeholder notes in advance. Create a topic file when that topic is first studied.

`MY_UNDERSTANDING.md` may only contain understanding explicitly stated by the user. Never write AI-generated explanations there as if they were the user's own understanding.

## Learning-mode write boundary

Unless the user explicitly asks to implement or fix gameplay code, day-based learning is a source-reading and documentation workflow.

Do not modify:

- `Source/`
- `Config/`
- `Content/`

Only update files under `Docs/ProjectLearning/` during learning mode.

## Explicit command equivalent

The project also provides:

```text
/day N
```

through `.opencode/commands/day.md`.

A natural-language request such as `第 6 天` and the explicit command `/day 6` should follow the same Day 6 learning contract.
