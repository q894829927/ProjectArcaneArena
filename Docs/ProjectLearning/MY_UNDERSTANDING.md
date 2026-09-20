# MY_UNDERSTANDING

> 本文件只记录用户本人明确表达过的理解。AI 的讲解、总结、修正不得写入本文件。
> 每条记录来源日期与 Day，保留用户原话要点；AI 的判定（正确/部分正确/错误）只标注，不替换用户表述。

## Day 1（2026-08-15）复述原话要点

1. GameMode 负责游戏整体规则（例如胜负判断），只存在于服务端。GameState 保存全局游戏状态，服务器写然后复制给客户端。PlayerController 负责获取本地输入以及 UI 创建与更新，客户端提交输入意图由服务器校验并执行。PlayerState 保存角色长期数据（例如 ASC），服务器写、复制给客户端。Character 负责本地按键转化为移动、技能，依赖于 Pawn。

2. InitAbilityActorInfo 分别在客户端和服务端调用一次，对能力系统进行初始化。（具体执行步骤当时不清楚。）

3. 升级候选 OwnerOnly / bHasSelectedUpgrade 全端复制的用途：当时不清楚。

4. GameplayEffect 提供了一系列初始化、回调等函数。PreAttributeChange 是在修改属性前进行上下限限制，确保数值合法性。PostGameplayEffectExecute 是在效果执行之后，处理例如扣血、死亡判定、事件派发等。

5. 如果 ASC 放 Character，需要保留当前 ASC 的各种上下文，等到重生后再把 ASC 进行初始化；将 ASC 绑定到 PlayerState 上解决。

6. Listen Server Host 为什么不会重复授予、以及新增第六个技能要改哪些文件：当时不知道。
