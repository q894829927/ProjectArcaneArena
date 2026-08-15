# Ranged Enemy Asset Setup

`setup_ranged_enemy.py` creates and configures the editor assets required by the
server-authoritative ranged enemy implementation. It does not load, create, save,
or switch maps.

## Run

1. Compile the project C++ and restart Unreal Editor so the new reflected classes are available.
2. Open any normal project level. The script does not modify the open map.
3. Run from the Unreal console:

```text
py "../../../../../UE_DEMO/ProjectArcaneArena/Content/Python/ranged_enemy/setup_ranged_enemy.py"
```

The script creates or reuses:

- `/Game/CombatMagicAnims/Animations/AM_EnemyRangedAttack`
- `/Game/GAS/GameplayEffect/GE_Cooldown_EnemyRangedAttack`
- `/Game/GAS/GameplayAbility/GA_EnemyRangedAttack`
- `/Game/GAS/Projectile/BP_ArenaEnemyProjectile`
- `/Game/Characters/ArenaEnemy/BP_ArenaRangedEnemy`

`AS_ManaCastShot` and the project Manny use separate Skeleton assets with the
same mannequin hierarchy. The script registers the animation-pack Skeleton in
the project Manny Skeleton's one-way `Compatible Skeletons` list, so the source
animation and generated Montage remain unchanged while `ABP_ArenaEnemy` can
play them.

It then replaces only the `Enemies` arrays in the first four prototype waves:

- Wave 1: `3 Melee`
- Wave 2: `3 Melee + 2 Ranged`
- Wave 3: `4 Melee + 3 Ranged`
- Wave 4: `5 Melee + 4 Ranged`

Existing `SpawnInterval`, `RewardCount`, and `BossWave` values are preserved.
