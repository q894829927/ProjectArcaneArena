#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "ArenaFireballProjectile.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UProjectileMovementComponent;
class USphereComponent;

UCLASS()
class PROJECTARCANEARENA_API AArenaFireballProjectile : public AActor
{
	GENERATED_BODY()

public:
	// 创建可复制 projectile 的碰撞体和移动组件。
	AArenaFireballProjectile();

	// 由服务端 Fireball Ability 注入伤害上下文，Projectile 自己不持有永久玩法状态。
	void InitializeProjectile(
		UAbilitySystemComponent* InSourceASC,
		AActor* InSourceActor,
		TSubclassOf<UGameplayEffect> InDamageEffectClass,
		FGameplayTag InDamageTypeTag,
		float InBaseDamage,
		float InSkillMultiplier);

protected:
	// 应用蓝图可调的速度、半径和生命周期。
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena|Projectile")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena|Projectile")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Projectile", meta = (ClampMin = "0.0"))
	float InitialSpeed = 1200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Projectile", meta = (ClampMin = "0.0"))
	float ProjectileLifeSpan = 2.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Projectile", meta = (ClampMin = "0.0"))
	float SphereRadius = 18.0f;

private:
	// Pawn overlap 只在服务端触发伤害结算。
	UFUNCTION()
	void OnProjectileOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	// 撞到阻挡物时结束 projectile 生命周期。
	UFUNCTION()
	void OnProjectileHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		FVector NormalImpulse,
		const FHitResult& Hit);

	// 过滤自身、无 ASC 和死亡目标，避免重复结算或误伤发射者。
	bool CanDamageTarget(AActor* TargetActor, UAbilitySystemComponent* TargetASC) const;

	// 使用 Source ASC 创建 GE_Damage Spec，实际数值计算交给 ExecCalc_Damage。
	void ApplyDamageToTarget(UAbilitySystemComponent* TargetASC, const FHitResult& HitResult);

	// 命中后只由服务端销毁，销毁结果通过 Actor replication 同步给客户端。
	void FinishProjectile();

	TWeakObjectPtr<UAbilitySystemComponent> SourceAbilitySystemComponent;
	TWeakObjectPtr<AActor> SourceActor;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	FGameplayTag DamageTypeTag;
	float BaseDamage = 25.0f;
	float SkillMultiplier = 1.0f;
	bool bHasImpacted = false;
};
