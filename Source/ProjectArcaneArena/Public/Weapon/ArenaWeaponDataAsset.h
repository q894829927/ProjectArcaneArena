#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ArenaWeaponDataAsset.generated.h"

class UGameplayEffect;

// P4 武器静态定义；只保存设计期数据，运行中的实例身份与攻击轮次由 WeaponLoadoutComponent 管理。
UCLASS(BlueprintType)
class PROJECTARCANEARENA_API UArenaWeaponDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Weapon")
	FName WeaponID;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Weapon")
	FText WeaponName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Weapon|Tags")
	FGameplayTagContainer WeaponTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Weapon|Attack", meta = (ClampMin = "0.05"))
	float FireInterval = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Weapon|Attack", meta = (ClampMin = "0.0"))
	float TargetRange = 1400.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Weapon|Projectile", meta = (ClampMin = "0.0"))
	float ProjectileSpeed = 1800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Weapon|Projectile", meta = (ClampMin = "0.05"))
	float ProjectileLifetime = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Weapon|Projectile", meta = (ClampMin = "0.0"))
	float ProjectileRadius = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Weapon|Projectile")
	float ProjectileSpawnHeight = 50.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Weapon|Projectile", meta = (ClampMin = "0.0"))
	float ProjectileForwardOffset = 60.0f;

	// P4-C 起用于一轮散射；P4-A/B 只读取默认值 1，不提前生成多颗 Projectile。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Weapon|Pattern", meta = (ClampMin = "1", ClampMax = "64"))
	int32 ProjectilesPerAttack = 1;

	// 水平扇形总夹角；多 Pellet 在 [-Spread/2,+Spread/2] 内确定性均匀展开，便于复现与测试。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Weapon|Pattern", meta = (ClampMin = "0.0", ClampMax = "360.0"))
	float SpreadAngleDegrees = 0.0f;

	// 同一 AttackInstanceID 对同一目标的后续 Pellet 伤害倍率按 Pow(Falloff, HitIndex) 递减；1 表示不衰减。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Weapon|Pattern", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SameTargetPelletFalloff = 0.75f;

	// 同一轮霰弹多次命中同一目标时的最低伤害倍率，防止后续 Pellet 无限衰减到接近零。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Weapon|Pattern", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinPelletDamageMultiplier = 0.25f;

	// 可额外穿过的目标数量；0 表示命中第一个目标后回收。真正 Pierce 去重在 P4-D 实现。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Weapon|Pattern", meta = (ClampMin = "0", ClampMax = "64"))
	int32 PierceCount = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Weapon|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Weapon|Damage")
	FGameplayTag DamageTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Weapon|Damage", meta = (ClampMin = "0.0"))
	float BaseDamage = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Weapon|Damage", meta = (ClampMin = "0.0"))
	float SkillMultiplier = 1.0f;
};
