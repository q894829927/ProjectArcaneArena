---
name: project-review-notes
description: Review and repair ProjectArcaneArena learning notes against the current source. Use after the project has changed, after a long learning gap, or when the user wants stale, duplicated, unsupported, or network-incomplete notes corrected without changing gameplay implementation.
---

# ProjectArcaneArena Learning Notes Review

Use this skill to keep `Docs/ProjectLearning/` trustworthy as the project evolves.

## Read first

Read:

- `AGENTS.md`
- `IMPLEMENTED_FEATURES.md`
- `PENDING_VERIFICATION.md`
- `Docs/ProjectLearning/LEARNING_RULES.md`
- `Docs/ProjectLearning/LEARNING_PROGRESS.md`
- all relevant existing learning notes;
- current source/config/assets for the notes under review.

Confirm:

```bash
git branch --show-current
git rev-parse HEAD
git status --short
```

## Review checklist

Check for:

1. **Stale source references** — classes/functions/files moved, renamed or removed.
2. **Broken call chains** — old flow no longer matches current implementation.
3. **Implementation/verification confusion** — do not equate `Implemented`, code understanding and `Verified`.
4. **Planning mistaken for implementation** — `AGENTS.md` goals must not be recorded as runtime facts without evidence.
5. **Asset assumptions** — Blueprint/DataAsset internals not actually confirmed.
6. **Missing network roles** — Multiplayer notes should identify Local/Owning Client/Server/Simulated Client where relevant.
7. **Missing GAS relationships** — Ability/GE/Attribute/Tag/Event/Cue/TargetData/Prediction should be connected rather than listed separately.
8. **Unsupported design claims** — inferred intent should be marked `【推测】`.
9. **Duplicate notes** — merge repeated explanations into the most appropriate module note.
10. **Missing modification entry points** — major modules should say where a developer would start to change them.
11. **Missing verification debt** — reflect relevant `PENDING_VERIFICATION.md` items without copying it wholesale.
12. **Source version drift** — notes must state or update the branch/commit context where useful.

## Repair strategy

For each issue:

```text
Find current source
→ verify new behavior
→ replace stale statement
→ update call chain/data flow
→ update evidence classification
→ preserve useful historical context only when it helps understanding
```

Do not keep contradictory old statements merely because they existed in previous notes.

## Update progress

Always update `Docs/ProjectLearning/LEARNING_PROGRESS.md` with:

- branch and HEAD used for review;
- which notes were reviewed;
- important corrections;
- unresolved editor/PIE verification items;
- next learning priority.

## Safety boundary

This is a documentation maintenance workflow. Do not modify `Source/`, `Config/` or `Content/`.

## Final report

Summarize:

- 修正了哪些过期内容；
- 哪些结论仍然可信；
- 哪些内容仍需编辑器/PIE/网络测试验证；
- 下一次最值得重新学习的模块。
