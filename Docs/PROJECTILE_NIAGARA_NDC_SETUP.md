# P5 Shared Niagara / Niagara Data Channel Setup

本页对应 Data Projectile 的 P5-A 批量表现桥。C++ 已负责从 `UArenaProjectileSimulationSubsystem` 批量发布 Projectile Snapshot 与 Impact Event；Niagara 资产需要在 Editor 中手工创建，因为 Niagara Graph 属于视觉资产配置，不由运行时代码伪造。

## 1. 固定资产路径

创建目录：

`/Game/Projectile/VFX`

必须使用以下三个资产名，否则 C++ 默认路径不会加载：

- `NDC_ArenaProjectiles`
- `NDC_ArenaProjectileImpacts`
- `NS_ArenaProjectiles_Shared`

两个 Data Channel 首版都使用 **Global Niagara Data Channel**。C++ 当前使用默认 SearchParams，因此不要在 P5-A 改成 Islands / GameplayBurst。

## 2. NDC_ArenaProjectiles 变量

按下面名称和类型创建变量，名称必须完全一致：

| Name | Niagara Type | 用途 |
|---|---|---|
| Position | Position | 权威 Data Projectile 当前世界位置 |
| Velocity | Vector | 当前速度，可用于朝向、拖尾长度 |
| Radius | Float | Projectile 逻辑半径，可映射视觉尺寸 |
| RemainingLife | Float | 剩余逻辑寿命 |
| VisualType | Int | WeaponDataAsset 的 ProjectileVisualTypeID |
| ProjectileSlot | Int | Data Pool Slot |
| Generation | Int | Slot 代次；与 ProjectileSlot 组成稳定视觉身份 |
| WeaponRuntimeID | Int | 当前装备 Runtime 身份 |
| AttackInstanceID | Int | 本次攻击身份 |
| PelletIndex | Int | 当前 Pellet 下标 |
| PelletCount | Int | 本次攻击 Pellet 总数 |

## 3. NDC_ArenaProjectileImpacts 变量

| Name | Niagara Type | 用途 |
|---|---|---|
| Position | Position | 权威 HitResult ImpactPoint |
| Normal | Vector | 权威 HitResult ImpactNormal |
| VisualType | Int | 武器静态视觉类型 |
| WeaponRuntimeID | Int | 武器 Runtime |
| AttackInstanceID | Int | 攻击轮次 |
| PelletIndex | Int | Pellet 下标 |
| ProjectileHitOrdinal | Int | 同一 Projectile 第几次穿透命中 |

## 4. NS_ArenaProjectiles_Shared

首版建议一个 Niagara System 放两个 Emitter：

1. `ProjectileSnapshot`
   - 读取 `NDC_ArenaProjectiles`。
   - World Space，关闭 Local Space。
   - P5-A 烟测可先按每个 NDC Entry 生成一个短寿命 Sprite/Mesh Particle，并把 Position、Velocity、Radius 写入 Particle 属性。
   - Particle Lifetime 建议略高于一帧，例如 0.04～0.06 秒，先验证批量链路和 Component 数量。
   - 后续 P5-B 再利用 `ProjectileSlot + Generation` 做稳定身份更新，减少“每帧重新生成 Snapshot Particle”的成本；C++ 数据协议无需修改。

2. `ImpactBurst`
   - 读取 `NDC_ArenaProjectileImpacts`。
   - 每个 Entry 只生成一次短 Burst。
   - 使用 Position 放置，用 Normal 调整朝向。
   - 可根据 VisualType 做颜色/材质/Renderer 分支。

System 使用较大的 Fixed Bounds 覆盖测试 Arena，否则共享组件位于世界原点时远处 Projectile 可能被错误裁剪。

## 5. C++ 写入时序

`UArenaProjectileSimulationSubsystem` 在本帧移动、Swept Collision 和 GAS HitCommand 消费完成后广播 `OnSimulationUpdated`。

`UArenaProjectileVisualSubsystem` 在该回调内：

```text
Simulation finished
    ↓
BuildVisualSnapshot
    ↓
NDC_ArenaProjectiles 一次 Batch Write
    ↓
CopyFrameImpactVisualEvents
    ↓
NDC_ArenaProjectileImpacts 一次 Batch Write
```

Dedicated Server 不绑定该表现回调，也不创建 Niagara Component。

当前 P6 尚未实现，因此远端 Client World 还没有 Data Projectile 重建数据。P5-A 先在 Standalone / Listen Server Host 验证；Remote Client 表现留到 P6 Launch Reconstruction。

## 6. 调试 CVar

```text
arena.Projectile.Visual.Enabled 1
arena.Projectile.Visual.LogWrites 1
arena.Projectile.Visual.MaxSamples 10000
arena.Projectile.Visual.MaxImpacts 2048
```

成功时应看到类似：

```text
Projectile Visual NDC snapshot wrote 12/12 active samples.
Projectile Impact NDC wrote 3 events.
```

P5 的视觉预算只允许少显示装饰 Particle，不允许减少服务器 Data Projectile、碰撞或真实 GAS Damage。
