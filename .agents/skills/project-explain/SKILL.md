---
name: project-explain
description: Explain one important ProjectArcaneArena class, component, DataAsset, subsystem, or gameplay module in project context. Use when the user names a class or system and wants to understand its role, lifecycle, callers, dependencies, data flow, networking, GAS role, or modification entry points.
---

# ProjectArcaneArena Explain

Use this skill for focused study of one class or subsystem.

## Read first

Read `Docs/ProjectLearning/LEARNING_RULES.md`, relevant existing notes, and the real current source for the requested target. Confirm current branch and HEAD.

## Explain in project context

Do not dump every member or translate code line by line.

Use this structure:

1. **它是什么** — one-sentence responsibility in ProjectArcaneArena.
2. **为什么存在** — what architectural problem it solves.
3. **继承/所有权** — important inheritance and owner relationships.
4. **谁创建它** — engine/framework/runtime creation path if confirmable.
5. **生命周期** — only lifecycle functions actually used and why they matter.
6. **核心状态/数据** — only members that drive behavior.
7. **核心函数** — only functions that matter to the system flow.
8. **谁调用它** — incoming dependencies.
9. **它调用谁** — outgoing dependencies.
10. **数据怎么流** — inputs, state mutations, outputs.
11. **网络角色** — Local / Client / Server / Replication where applicable.
12. **GAS 角色** — Ability/ASC/Attribute/GE/Tag/Event/Cue relationships where applicable.
13. **项目中的位置** — show a small architecture/call-flow diagram.
14. **修改入口** — where to start for common changes.
15. **常见误解** — distinguish implementation facts from project plans or asset assumptions.

Include real file paths and function names. Mark uncertain asset behavior `【待编辑器验证】` and architectural inference `【推测】`.

## Notes

If the explanation materially improves the project knowledge base, update the matching file in `Docs/ProjectLearning/` and optionally `18_Source_Code_Index.md`.

Do not modify gameplay code in explain mode.
