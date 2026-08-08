---
name: project-learn
description: Continue structured source-code learning for the ProjectArcaneArena UE5 project. Use when the user wants to study the project, continue project notes, understand the next subsystem, or build and maintain Docs/ProjectLearning without changing gameplay code.
---

# ProjectArcaneArena Daily Learning

Use this as the main daily learning workflow.

## Read first

Before analysis, read:

- `AGENTS.md`
- `IMPLEMENTED_FEATURES.md`
- `PENDING_VERIFICATION.md`
- `Docs/ProjectLearning/LEARNING_RULES.md`
- `Docs/ProjectLearning/LEARNING_PROGRESS.md`
- relevant existing files under `Docs/ProjectLearning/`

Then inspect Git state:

```bash
git branch --show-current
git rev-parse HEAD
git status --short
```

Treat the current checkout, including clearly identified working-tree changes, as the source under study. Never assume `main` is newest.

## Choose the next learning target

Unless the user explicitly names a topic, choose one highest-value unfinished module using this order:

1. Project Map
2. Gameplay Framework
3. Player Lifecycle
4. GAS Core
5. Attributes and Damage
6. Ability System
7. Input / Targeting / Dual View
8. Enemy AI
9. Wave / GamePhase
10. Roguelike Upgrade
11. Status / Trigger / Build
12. Multiplayer
13. Boss
14. UI / Inventory / Pickup
15. GameplayCue / VFX
16. DataAsset / Asset Config
17. Python Asset Automation
18. Debug / Verification
19. Gameplay Flow Atlas
20. Final Summary

Do not repeat a completed module unless current source changes invalidate the existing notes.

## Analysis requirements

For the selected module answer:

1. What problem does it solve?
2. What are the core classes/assets?
3. Who creates/owns it?
4. Who calls it?
5. What does it call?
6. How does data/state flow?
7. What lifecycle matters?
8. Why is it designed this way?
9. Which other systems does it connect to?
10. Where should a developer enter to modify it?

For GAS, also map Ability / GE / Attribute / Tag / Event / Cue / TargetData / Prediction.

For networking, distinguish Local, Owning Client, Server, Simulated Client and replication/prediction boundaries.

## Required call chain

Produce at least one real call chain grounded in current code:

```text
Entry
↓
Class::Function()
↓
Class::Function()
↓
Gameplay system
↓
Result
```

Mark steps where useful:

```text
[Local] [Predicted] [Server] [Replicated] [Presentation]
```

Include file paths, class names and function names. Mark uncertain conclusions as `【推测】` or `【待编辑器验证】`.

## Write notes

Update the matching Markdown file under `Docs/ProjectLearning/`. Reuse and improve existing notes instead of generating duplicate files.

Always update `Docs/ProjectLearning/LEARNING_PROGRESS.md` with:

- current branch;
- current commit;
- working-tree state;
- what was completed this round;
- new key call chains;
- unresolved questions;
- recommended next topic.

## Safety boundary for learning mode

Do not modify gameplay implementation while using this skill unless the user explicitly asks to leave learning mode.

By default only modify:

```text
Docs/ProjectLearning/
```

Do not modify:

```text
Source/
Config/
Content/
```

## Final report

Return a concise report with:

- 本轮学了什么
- 最关键的调用链
- 最值得记住的三个设计点
- 还有什么没搞懂
- 下一步应该学什么
