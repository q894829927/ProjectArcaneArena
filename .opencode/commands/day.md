---
description: 学习 ProjectArcaneArena 指定天数的课程
---

执行 ProjectArcaneArena **第 $1 天**学习计划。

以下文件是本次任务的依据：

@Docs/ProjectLearning/DAILY_PLAN.md

@Docs/ProjectLearning/LEARNING_RULES.md

@Docs/ProjectLearning/LEARNING_PROGRESS.md

@IMPLEMENTED_FEATURES.md

@PENDING_VERIFICATION.md

当前 Git 状态：

```text
Branch: !`git branch --show-current`
HEAD: !`git rev-parse HEAD`
Working tree:
!`git status --short`
```

## 执行规则

在 `Docs/ProjectLearning/DAILY_PLAN.md` 中找到 **Day $1**，严格按照当天计划执行。

不要把文档里的描述直接当作最终事实。读取当天相关的当前 ProjectArcaneArena 源码、配置以及必要的资产关联，验证重要结论。

以当前 checkout 为准，不默认 `main` 或 `develop` 一定是最新实现。

## 当天输出结构

按以下顺序生成：

# Day $1

## 1. 今日目标

说明今天结束时必须真正掌握什么，以及它为什么值得学习。

## 2. 前置概念

只解释 Day $1 需要的 UE5 / GAS / Multiplayer 理论，不扩展到与今天无关的知识。

## 3. 源码导航

给出推荐阅读顺序：

```text
文件
→ 类
→ 函数
→ 为什么先看这里
```

必须尽量给出真实：

- 文件路径
- 类名
- 函数名

不要逐文件、逐行翻译源码。

## 4. 核心架构

回答：

1. 这个模块解决什么问题？
2. 核心类有哪些？
3. 谁创建它？
4. 谁调用它？
5. 它调用谁？
6. 生命周期如何？
7. 它在 ProjectArcaneArena 整体架构中的位置是什么？

## 5. 完整调用链

至少追踪一条 ProjectArcaneArena 当前源码中的真实端到端 Flow。

涉及网络时标记：

```text
[Local]
[Owning Client]
[Predicted]
[Server]
[Replicated]
[Simulated Client]
[Presentation]
```

示例格式：

```text
入口
↓
Class::Function()
↓
Class::Function()
↓
Gameplay System
↓
最终结果
```

不要用“GAS 自动处理”替代具体调用关系。

## 6. 数据流

说明：

```text
数据从哪里产生
→ 以什么类型存在
→ 谁传递
→ 谁验证
→ 谁修改
→ 谁消费
→ 是否复制
```

涉及 GAS 时，根据当天主题解释实际用到的：

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

不要为了凑概念把当天没有涉及的机制全部讲一遍。

## 7. 网络角色

如果 Day $1 涉及 Multiplayer，明确说明：

### Local / Owning Client

做什么？

### Server

做什么？为什么必须由服务器做？

### Simulated Client

看到什么？

### Replication / RPC / Prediction

哪些数据或行为通过什么机制同步？

特别注意 Listen Server 中 Host 可能同时满足 Local Control 和 Authority。

## 8. UE / GAS 原理

结合当前项目代码解释原理，不要只给教科书定义。

必须说明这些原理为什么会影响 ProjectArcaneArena 当前设计。

## 9. 为什么这样设计

至少分析三个设计取舍。

每个使用：

```text
当前方案
→ 为什么
→ 替代方案
→ 替代方案的代价/问题
```

## 10. 当前边界与待验证项

结合源码、`IMPLEMENTED_FEATURES.md` 和 `PENDING_VERIFICATION.md`。

严格区分：

- `Implemented`
- `Partial`
- `Verified`
- `Pending Verification`

不得把规划、推测、仅存在的资产名写成已经完整验证的功能。

无法从源码确认的 Blueprint / Asset 内部行为标记：

- `【待编辑器验证】`
- `【推测】`

## 11. 面试追问

生成至少 5 个问题，难度按以下顺序增加：

```text
功能
→ 调用链
→ 网络
→ UE/GAS 原理
→ 设计取舍
→ 边界/重构
```

## 12. 我的复述

到这里停止。

让我自己回答问题并复述今天最重要的调用链。

不要直接宣布 Day $1 已完成，也不要自动进入 Day $2。

## 用户回答后的收尾

等我回答以后再：

1. 判断每个回答为 `正确` / `部分正确` / `错误`；
2. 修正关键误解；
3. 按 `DAILY_PLAN.md` 的 Day $1 映射创建或更新对应主题笔记；
4. 把完整 Flow 写入 `Docs/ProjectLearning/Flows/Gameplay_Flow_Atlas.md`；
5. 把高价值问题写入 `Docs/ProjectLearning/Interview/Questions.md`；
6. 有真实 Bug / Tradeoff 时更新 `Docs/ProjectLearning/Interview/Bugs_and_Tradeoffs.md`；
7. 创建或更新 `Docs/ProjectLearning/Daily/DayNN.md`；
8. 更新 `Docs/ProjectLearning/LEARNING_PROGRESS.md`。

`MY_UNDERSTANDING.md` 只能记录我本人明确表达过的理解，禁止把 AI 自己的总结写成我的理解。

## 写入边界

本命令默认是学习模式。

除非我明确要求实现或修复代码，否则不要修改：

- `Source/`
- `Config/`
- `Content/`

学习结束时只维护 `Docs/ProjectLearning/` 下的笔记。
