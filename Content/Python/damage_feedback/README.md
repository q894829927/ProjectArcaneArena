# Damage Feedback Polish

阶段六 A 的脚本只创建和连接伤害反馈表现资产，不修改伤害数值、GameplayTag、升级池或波次。

关闭 PIE，在完成最新 C++ 编译并重启 Unreal Editor 后执行：

```text
py "../../../../../UE_DEMO/ProjectArcaneArena/Content/Python/damage_feedback/setup_damage_feedback_polish.py"
```

成功日志应以 `Damage feedback polish setup completed successfully` 结束。再执行一次相同命令，确认不会创建 `_1`、`_2` 资产。

脚本会配置：

- `/Game/GAS/DamageFeedback/M_ArenaHitFlashOverlay`
- `CS_DamageLight`
- `CS_DamageMedium`
- `CS_DamageHeavy`
- `CS_ShieldBreak`
- `GCN_BasicAttack_Activate` 中唯一的 `SC_Basic_Slash_Cue` 挥击音
- Player、近战敌人、远程敌人和 Boss 的 `HitReactionComponent`

如果脚本提示 C++ 属性不可见，说明编辑器仍在使用旧 DLL。完整编译并重启编辑器后再执行，不要用旧反射数据强行保存角色蓝图。

如果当前 Unreal Python 没有暴露 `PerlinNoiseCameraShakePattern`，手动创建四个 `CameraShakeBase` Blueprint，Root Pattern 选择 `Perlin Noise`，按脚本顶部 `CAMERA_SHAKE_CONFIGS` 设置时长和幅度，再分别写入四类角色的 `HitReactionComponent`。
