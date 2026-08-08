# ProjectArcaneArena Learning Workspace

本目录用于持续学习 `ProjectArcaneArena` 的 UE5 源码与项目架构。

目标不是逐文件翻译代码，而是建立一套可以长期维护的项目知识库，最终做到：

- 不看源码也能讲清项目整体架构；
- 能从任意 Gameplay 功能入口追到最终结果；
- 能说明核心数据、状态、GAS、AI、Wave、Upgrade、Multiplayer 与表现层如何协作；
- 能判断功能应该修改 C++、DataAsset、GameplayEffect、Blueprint、Config 还是 Python 自动化；
- 能在现有架构上独立增加相似功能。

## Codex 原生入口

仓库提供 repo-scoped skills，位于：

```text
.agents/skills/
```

在 Codex CLI / IDE 中可以使用 `/skills` 查看，也可以直接输入 `$` 选择技能。

### 日常总命令

```text
$project-learn
```

用于按学习进度继续分析当前项目，并把结论写入 `Docs/ProjectLearning/`。

### 子命令

```text
$project-trace
$project-explain
$project-why
$project-change-plan
$project-review-notes
```

详细使用方式见 [COMMANDS.md](COMMANDS.md)。

## 核心文件

- `LEARNING_RULES.md`：ProjectArcaneArena 专用总提示词与学习规则。
- `COMMANDS.md`：日常总命令与专项子命令说明。
- `LEARNING_PROGRESS.md`：持续记录学习进度、关键调用链、待确认问题与对应源码版本。

## 计划生成的学习笔记

```text
00_Project_Map.md
01_Gameplay_Framework.md
02_Player_Lifecycle.md
03_GAS_Core.md
04_Attributes_and_Damage.md
05_Ability_System.md
06_Input_Targeting_and_View.md
07_Enemy_AI.md
08_Wave_and_GamePhase.md
09_Boss_System.md
10_Roguelike_Upgrade.md
11_Status_Trigger_and_Build.md
12_Multiplayer.md
13_UI_Inventory_and_Pickup.md
14_GameplayCue_and_VFX.md
15_DataAsset_and_Asset_Config.md
16_Python_Asset_Automation.md
17_Debug_and_Verification.md
18_Source_Code_Index.md
19_Gameplay_Flow_Atlas.md
20_Final_Summary.md
```

不存在或尚未实现的系统不应生成空洞内容。

## 学习原则

统一采用：

```text
项目地图
  ↓
系统职责
  ↓
Gameplay Flow
  ↓
调用链
  ↓
数据流
  ↓
生命周期
  ↓
网络权限
  ↓
设计原因
  ↓
修改入口
```

而不是：

```text
.h → .cpp → 下一个 .h → 下一个 .cpp
```

`IMPLEMENTED_FEATURES.md`、`PENDING_VERIFICATION.md` 和 `AGENTS.md` 是重要导航材料，但最终学习结论仍应以当前 checkout 分支的真实源码、配置和可确认资产关系为依据。
