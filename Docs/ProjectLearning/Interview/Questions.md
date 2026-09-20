# 面试问题（Interview Questions）

> 高价值问题按主题累积。Day 1 六连问为**面试口述版**：按口语逻辑链组织，可直接开口讲，不是要点清单。
> 证据标签：【源码确认】【项目文档】【推测】。口述版后的「必备落点」用于自查是否漏掉关键点。

## Day 1 — 项目全局架构

### Q1（功能）说出 GameMode / GameState / PlayerController / PlayerState / Character 各自负责什么，哪些类客户端上根本不存在？

**面试口述版：**

我把我们项目的框架按四层来说。

第一层是规则层。AArenaGameMode 只存在于服务器，它不参与复制，客户端上 GetGameMode 拿到的都是空。它负责这局比赛的所有规则：开局要生成哪几个 GameState、PlayerState、Pawn 类，什么时候开始波次，波次结束怎么生成升级候选，玩家选的升级到底合不合法，以及全员死亡判负、击杀 Boss 判胜。它是服务器唯一的裁判。

第二层是状态层。AArenaGameState 服务器写、复制到所有客户端，是一个所有人共享的比分牌。我们项目里阶段（Waiting / Combat / Upgrade / BossIntro / …）、当前波次、剩余敌人数、当前 Boss，都是复制属性，复制到客户端后触发 OnRep 广播委托，UI 和角色只订阅这些委托。

第三层是每玩家的控制层。AArenaPlayerController 每个玩家一个，注意服务器端也各有一个 PC——服务器端的 PC 是用来接收 Server RPC 做校验的。Owning Client 的 PC 负责本地 UI 和输入，比如 HUD、升级三选一、背包、ESC 菜单，然后把 UI 意图转成 Server RPC 提交上去。

第四层是玩家的长期状态层。AArenaPlayerState 每玩家一个，存放跨 Pawn 生命周期存在的玩家数据，最重要的是玩家自己的 AbilitySystemComponent 和 AttributeSet，还有升级的拥有和堆叠记录。为什么放 PlayerState 不放 Character，是因为 PlayerState 在角色死亡重生时不会销毁。

最后是身体表现层，Character。注意 Character 本身就是 Pawn 的子类，它只是被 Controller 控制的可替换的身体，负责移动表现、相机、蒙太奇，以及把本地按键输入转成 GAS 的输入 Tag。

所以客户端上根本不存在的是 GameMode 和它生成的 WaveManager；GameState 是所有端共享的。

**必备落点：** GameMode 只存服务器；GameState RepNotify 全端；PC 两端都有（服务器端收 RPC）；PlayerState 跨 Pawn 存活；Character 就是 Pawn 的子类；客户端没有 GameMode/WaveManager。

---

### Q2（调用链）玩家从登录到能按左键放技能，服务器和 Owning Client 各执行了哪几步？InitAbilityActorInfo 为什么两端都要调用、参数是什么？

**面试口述版：**

这条链我分服务器和客户端两条来说。

服务器端：玩家登录后，GameMode 走 PostLogin，然后 RestartPlayer 生成 Pawn 并 Possess。Character 被 Possess 之后回调 PossessedBy，我们在里面调用 InitializeAbilityActorInfo，核心就是一行 ASC->InitAbilityActorInfo(PlayerState, this)。参数是两个：OwnerActor 是 PlayerState，AvatarActor 是当前这个 Character。紧接着在 HasAuthority 分支里，ApplyDefaultAttributes 用默认属性的 GameplayEffect 初始化属性，GrantStartupAbilities 把启动技能逐个 GiveAbility 授予，并把技能 CDO 上配的 InputTag 写进 AbilitySpec 的动态源标签里。这些都是服务器做的。

客户端：服务器把技能和属性复制过来，客户端等 PlayerState 复制到位后触发 OnRep_PlayerState，里面同样调用 InitializeAbilityActorInfo，但客户端没有授权逻辑，它只是把 ASC 的 Avatar 指针指向本端这个 Character，让技能系统能在本地跑预测和表现。

玩家按左键之后，Enhanced Input 触发 Input_BasicAttack，转成一个 GameplayTag，交给 ASC::AbilityInputTagPressed，ASC 遍历所有可激活的 AbilitySpec，按动态源标签匹配到 BasicAttack，然后 TryActivateAbility 进入正式的激活流程。

至于为什么两端都要调用：ASC 的 Owner 在 PlayerState 上，而 Avatar 必须指向本端视角下的那个身体。服务器在 Possess 的时候知道这个身体，客户端要等 PlayerState 复制到位才知道，两个时机不一样，所以两端各做一次。如果漏了客户端那一次，典型症状就是技能按了没反应、特效不播——因为 Avatar 是空的。

“服务器端，玩家登录进来以后，GameMode 先走 PostLogin，确认当前阶段和是否需要初始化首波。之后 RestartPlayer 生成 Pawn，并在 Possess 之后触发 PossessedBy。我们在 PossessedBy 里立刻做 InitializeAbilityActorInfo，核心就是一行 ASC->InitAbilityActorInfo(PlayerState, this)。这里的 Owner 是 PlayerState，Avatar 是当前 Character，这样 ASC 属于玩家而不是某个临时 Pawn。然后在 HasAuthority 分支里，ApplyDefaultAttributes 初始化默认属性，GrantStartupAbilities 把启动技能逐个 GiveAbility 授给这个玩家。客户端则等 PlayerState 复制到位后走 OnRep_PlayerState，同样调用 InitializeAbilityActorInfo，但这里不重复授予能力，只是把本地 Character 和 ASC 绑定起来，让本地预测和表现能够正常运行。之后玩家按左键，输入转成 GameplayTag，ASC 通过 AbilityInputTagPressed 在所有 AbilitySpec 中按 InputTag 匹配到对应技能，然后 TryActivateAbility 进入正式激活。两端都要做 InitAbilityActorInfo，是因为服务器和客户端的绑定时机不同，只有两者都绑定好，ASC 才能稳定地知道自己属于哪个玩家、绑定到哪个 Avatar。否则本地技能就会没有 Avatar，表现就会缺失。”

**必备落点：** PossessedBy（服务器）/ OnRep_PlayerState（客户端）；InitAbilityActorInfo(PlayerState, this)；授予只在 HasAuthority；输入链路 LMB → Input_BasicAttack → InputTag → AbilityInputTagPressed → TryActivateAbility；漏客户端 Init 的症状。

---

### Q3（网络）升级候选为什么 COND_OwnerOnly，而 bHasSelectedUpgrade 全端复制？反过来会出什么问题？

**面试口述版：**

这两个数据的用途完全不一样。

升级候选和已拥有升级是每个玩家私有的卡池和构筑信息。比如服务器从卡池抽了三个候选给我的界面，这三张只有我自己该看到，所以复制条件用 COND_OwnerOnly，只复制给这个 PlayerState 的拥有者。一方面防止别的客户端窥探我的候选池，另一方面省带宽，别人不需要知道我手里是哪三张。

而 bHasSelectedUpgrade 这个 bool，服务器自己写、自己也知道，它复制给所有客户端不是为了服务器，是为了 UI。多人模式里我要在界面上显示"其他玩家已经选完了、还在等你"，这是别人的选择状态，必须复制到我的客户端我才看得到，所以它全端复制。

反过来会出什么问题？如果候选全端复制，等于把服务器随机抽卡的结果广播给所有人，别人能通过看到你的卡池去作弊或者读信息。如果 bHasSelectedUpgrade 改成 OwnerOnly，每个玩家只知道自己选没选，但 GameMode 要等所有人选完才能开下一波，UI 上就没法显示别人的等待状态了。

**必备落点：** 候选/构筑=私有（防窥探+省带宽）；bHasSelectedUpgrade 复制给 UI 显示等待（服务器自己不需要复制给自己用）；两个反例。

---

### Q4（UE/GAS 原理）为什么默认属性要通过 GameplayEffect 应用而不是直接 SetHealth？PreAttributeChange 和 PostGameplayEffectExecute 各自做什么？

**面试口述版：**

核心原因是要让所有属性变化都走同一条 GAS 管线，而不是开一条绕路。

如果我直接在初始化里 SetHealth(100)，就绕过了 GAS 的属性复制、clamp、属性变化委托和后处理钩子。属性变化如果有 UI 要监听、有 clamp 要执行、有后续逻辑要跑，直接 Set 就得自己在外面手动补一套。而且用 GE 的话，初始值本身变成可配置的数据，换一个 GE 资产就能改初始血量，不用改代码。

GE 本身不是"回调函数集合"，它是一个数据载体：Modifiers 定义改哪个属性、怎么改、改多久；Executions 可以挂 ExecutionCalculation 去跑自定义结算逻辑；GrantedTags 用来授予状态标签。真正执行修改的是 ASC，属性值变化发生在 AttributeSet 里。

然后两个钩子。PreAttributeChange 是在属性 CurrentValue 被修改之前调用，我们在这里统一做 clamp：Health 不能超过 MaxHealth、不能低于 0，Energy clamp 在 0 到 MaxEnergy，Shield 不能低于 0，保证数值进入系统之前就是合法的。

PostGameplayEffectExecute 是 GE 执行完之后调用，我们在这里消费两个元属性：Damage 和 Healing。收到 Damage 就走护盾优先、再扣血、检查死亡更新 State.Dead 标签、把伤害反馈排进队列、最后路由权威伤害事件给被动系统。简单说，PreAttributeChange 管"数值合不合法"，PostGameplayEffectExecute 管"伤害生效之后发生什么"。

**必备落点：** 统一管线（clamp/复制/委托/后处理）+ 可配置；GE 是数据载体不是回调；PreAttributeChange=clamp；PostGameplayEffectExecute=元属性消费/护盾优先/死亡/事件。

---

### Q5（设计取舍）如果把 ASC 放到 Character 上，死亡重生流程要额外处理哪些事情？本项目用什么机制防止重复授予技能和重复初始化属性？

**面试口述版：**

如果 ASC 放在 Character 上，最大的问题是生命周期。角色死亡或者被销毁的时候 Character 就没了，挂在它上面的 ASC、所有已授予技能、正在生效的 GameplayEffect、属性数值，全都跟着没了。重生的时候你就得把技能重新授予一遍、属性重新初始化一遍，而且玩家的长期状态，比如升级买的技能、构筑标签，你还得先从别处搬回来。这是很大的迁移成本，也很容易丢状态。

我们的方案是把 ASC 和 AttributeSet 放在 PlayerState 上，PlayerState 在角色死亡重生时不会销毁，ASC 就一直活着。重生其实只是换 Avatar——新的 Character 再调一次 InitAbilityActorInfo(PlayerState, this)，把 ASC 的 Avatar 指到新身体上，不需要"保存状态再恢复"，因为状态根本没被销毁。

防重复的机制：角色在服务器上会被 PossessedBy 触发初始化，客户端那边 OnRep_PlayerState 也会触发一次。我们在 PlayerState 上放了两个布尔守卫，bGrantedStartupAbilities 和 bAppliedDefaultAttributes。ApplyDefaultAttributes 和 GrantStartupAbilities 一进来先检查，授予成功才置位，所以不管触发几次，服务器最多实际执行一次。再加上授予逻辑本身只在 HasAuthority 的分支里，客户端那条路径根本没有授予代码，双重保险。

**必备落点：** ASC 随 Character 销毁=状态全丢；换 Avatar 而非恢复；守卫位 bGrantedStartupAbilities / bAppliedDefaultAttributes；HasAuthority 分支只服务器执行。

---

### Q6（边界/重构）Listen Server 的 Host 同时触发 PossessedBy 和 OnRep_PlayerState，为什么不会把启动技能授予两次？新增第六个主动技能需要改哪些文件？

**面试口述版：**

Listen Server 的 Host 很特殊，它同时是服务器也是本地玩家，所以两条初始化路径都会走：服务器这边的 PossessedBy，和客户端视角的 OnRep_PlayerState。但不会重复授予，原因有两个。

第一，授予代码放在 HasAuthority 的分支里。Host 进程 HasAuthority 是真的，但 OnRep_PlayerState 里我们只是调用同一个 InitializeAbilityActorInfo，授权那部分只在 HasAuthority 分支执行；而远程客户端的 OnRep 路径根本没有授权逻辑。第二，即使服务器端因为各种原因重复触发，PlayerState 上的 bGrantedStartupAbilities 守卫位保证 GiveAbility 最多执行一次。

新增第六个主动技能，需要改的地方很少：第一，新建一个继承 UArenaGameplayAbility 的 GA 类，里面配上 InputTag 和技能相关 Tag；第二，把这个技能类加进 StartupAbilities 配置数组。就这两个。

Character 的输入绑定不用改，ASC 的路由也不用改，因为输入路由是按 AbilitySpec 上动态源标签通用匹配的循环，只要技能 CDO 配了 InputTag，GiveAbility 的时候它自动写进 Spec 的动态源标签，一按就能匹配上。这就是为什么我们坚持用 Tag 路由，而不是在 Character 里硬编码每个技能。

**必备落点：** Host=Authority+Local 双路径；不重复=HasAuthority 分支+守卫位；新增技能=GA 类（InputTag）+ StartupAbilities；不用改 Character/ASC 路由。

---

## 待积累

- Day 2：网络基础相关（Authority / Proxy / RPC / Replication / RepNotify）。
- Day 6：BasicAttack 预测链相关。
- Day 12：Prediction Rollback 相关。
