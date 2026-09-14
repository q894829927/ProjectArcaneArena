#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "ArenaEnemyProjectile.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UParticleSystem;
class UParticleSystemComponent;
class UProjectileMovementComponent;
class USphereComponent;

UCLASS(Blueprintable)
class PROJECTARCANEARENA_API AArenaEnemyProjectile : public AActor
{
	GENERATED_BODY()

public:
	// 创建服务器生成、移动复制的无重力直线 Projectile。
	AArenaEnemyProjectile();

	// 在延迟生成完成前写入来源 ASC 与伤害快照，客户端只观察飞行表现。
	void InitializeProjectile(
		UAbilitySystemComponent* InSourceASC,
		AActor* InSourceActor,
		TSubclassOf<UGameplayEffect> InDamageEffectClass,
		FGameplayTag InDamageTypeTag,
		float InBaseDamage,
		float InSkillMultiplier);

protected:
	// 应用蓝图可调的半径、速度、寿命和 Cascade 飞行表现。
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena|Projectile")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena|Projectile")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena|Projectile")
	TObjectPtr<UParticleSystemComponent> ProjectileVisual;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Projectile|Visual")
	TObjectPtr<UParticleSystem> ProjectileEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Projectile", meta = (ClampMin = "0.0"))
	float InitialSpeed = 900.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Projectile", meta = (ClampMin = "0.0"))
	float ProjectileLifeSpan = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Projectile", meta = (ClampMin = "0.0"))
	float SphereRadius = 16.0f;

private:
	// Pawn overlap 只在服务器消费存活玩家；敌人和无效 Pawn 不会阻挡 Projectile。
	UFUNCTION()
	void OnProjectileOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	// 撞到世界阻挡物时由服务器销毁 Projectile，不执行玩家伤害。
	UFUNCTION()
	void OnProjectileHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		FVector NormalImpulse,
		const FHitResult& Hit);

	// 只允许任意存活玩家拦截，Dash 无敌玩家仍会消耗 Projectile。
	bool CanDamagePlayer(AActor* TargetActor, UAbilitySystemComponent* TargetASC) const;
	// 通过 GE_Damage 和 Physical Tag 结算一次权威伤害。
	void ApplyDamageToTarget(UAbilitySystemComponent* TargetASC, const FHitResult& HitResult);
	// 关闭碰撞并销毁服务器 Actor，bHasImpacted 防止 overlap/hit 重复消费。
	void FinishProjectile();

	TWeakObjectPtr<UAbilitySystemComponent> SourceAbilitySystemComponent;
	TWeakObjectPtr<AActor> SourceActor;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	FGameplayTag DamageTypeTag;
	float BaseDamage = 6.0f;
	float SkillMultiplier = 1.0f;
	bool bHasImpacted = false;
};
