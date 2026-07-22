#pragma once

#include "CoreMinimal.h"
#include "Character/ArenaCharacterBase.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "ArenaEnemyCharacter.generated.h"

class AArenaEnemyCharacter;
class AArenaDamageNumberActor;
class UArenaAbilitySystemComponent;
class UArenaAttributeSet;
class UArenaEnemyHealthBarWidget;
class UGameplayEffect;
class UGameplayAbility;
class UAbilitySystemComponent;
class UWidgetComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FArenaEnemyDeathSignature, AArenaEnemyCharacter*, Enemy);

UCLASS()
class PROJECTARCANEARENA_API AArenaEnemyCharacter : public AArenaCharacterBase
{
	GENERATED_BODY()

public:
	AArenaEnemyCharacter();

	// 敌人 ASC 直接挂在敌人身上，便于 AI 和伤害系统访问。
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	// 返回项目 ASC 具体类型，供 Boss HUD 等只读观察层注册委托。
	UArenaAbilitySystemComponent* GetArenaAbilitySystemComponent() const { return AbilitySystemComponent; }
	// 返回敌人 AttributeSet，调用方只应读取或注册 GAS 属性委托。
	UArenaAttributeSet* GetArenaAttributeSet() const { return AttributeSet; }

	// 服务器 AI 写入当前战斗目标，Ability 激活后仍会重新校验该目标。
	void SetCombatTarget(AActor* NewCombatTarget);
	AActor* GetCombatTarget() const { return CombatTarget.Get(); }
	// 激活 StartupAbilities 中第一个 EnemyAttackBase 子类，供近战与远程 AI 共用。
	bool TryActivatePrimaryAttack();
	// 返回主攻击 CDO 的距离，AI 将它与主攻击路径共同用于追击和停步决策。
	float GetPrimaryAttackRange() const;
	// 使用主攻击自身的视线或弹道规则判断当前目标是否可攻击。
	bool HasPrimaryAttackPath(AActor* TargetActor);
	// 取消所有正在运行的 EnemyAttackBase 实例，供多技能 Boss 和普通敌人共享异常清理。
	void CancelPrimaryAttack();
	// 由 AIController 通过 AbilityTag 请求激活近战技能。
	bool TryActivateMeleeAttack();
	// AI 使用 Ability CDO 的攻击距离决定追击接受半径，最终命中仍由 Ability 校验。
	float GetMeleeAttackRange() const;
	bool IsDeadOrStunned() const;
	// AI 查询当前攻击窗口，攻击期间只停止寻路，不清空已锁定目标。
	bool IsAttacking() const;
	// 由伤害数字 GameplayCue 在本地生成表现，不参与复制或伤害结算。
	void SpawnDamageNumber(float DamageAmount, bool bCriticalHit);
	// 向公共受击组件提供现有敌人蓝图配置，避免资产迁移后数字丢失。
	virtual TSubclassOf<AArenaDamageNumberActor> GetDamageNumberActorClassForFeedback() const override;
	virtual FVector GetDamageNumberSpawnOffsetForFeedback(const FVector& ComponentDefault) const override;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Enemy")
	FArenaEnemyDeathSignature OnEnemyDeath;

protected:
	// 初始化敌人 GAS、属性和表现委托。
	virtual void BeginPlay() override;
	// 解绑 GAS 委托，避免销毁时留下无效回调。
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TSubclassOf<UGameplayEffect> DefaultAttributeEffect;

	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Death", meta = (ClampMin = "0.0"))
	float DeathLifeSpan = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Feedback")
	TSubclassOf<AArenaDamageNumberActor> DamageNumberActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Feedback")
	FVector DamageNumberSpawnOffset = FVector(0.0f, 0.0f, 130.0f);

	// 控制敌人头顶血条是否显示，Boss 可关闭后改由玩家 HUD 统一展示。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arena|Feedback")
	bool bShowWorldHealthBar = true;

	// 蓝图死亡表现入口，后续可接死亡动画、Niagara 和音效。
	UFUNCTION(BlueprintImplementableEvent, Category = "Arena|Enemy")
	void K2_OnDeathStarted();

	// Health 变化表现入口，UI 和反馈只观察属性，不拥有玩法状态。
	UFUNCTION(BlueprintImplementableEvent, Category = "Arena|Enemy")
	void K2_OnHealthChanged(float OldHealth, float NewHealth, float MaxHealth);

	// 旧蓝图兼容入口；统一 DamageFeedback 生效后不再由 Health Delegate 自动调用。
	UFUNCTION(BlueprintImplementableEvent, Category = "Arena|Enemy")
	void K2_OnDamaged(float DamageAmount, float NewHealth, float MaxHealth);

private:
	// 以敌人自身作为 OwnerActor 和 AvatarActor 初始化 ASC。
	void InitializeAbilityActorInfo();
	// 通过默认 GameplayEffect 初始化敌人属性，保持 GAS 数据流一致。
	void ApplyDefaultAttributes();
	// 服务器授予敌人启动技能，敌人生命周期内只执行一次。
	void GrantStartupAbilities();
	// 按 StartupAbilities 固定顺序查找第一个通用敌人攻击类，保持数据配置确定性。
	TSubclassOf<UGameplayAbility> FindPrimaryAttackAbilityClass() const;
	// 绑定死亡标签和 Health 属性变化，用事件驱动死亡与反馈。
	void BindAbilitySystemDelegates();
	// 解绑死亡标签和 Health 属性变化委托。
	void UnbindAbilitySystemDelegates();
	// State.Dead 标签变化是死亡逻辑的唯一入口。
	void HandleDeadTagChanged(const FGameplayTag CallbackTag, int32 NewCount);
	void HandleStunnedTagChanged(const FGameplayTag CallbackTag, int32 NewCount);
	void HandleMoveSpeedChanged(const FOnAttributeChangeData& Data);
	void RefreshMovementState();
	// Health 变化只负责血条和数值通知；死亡与完整受击表现分别由标签和反馈批次驱动。
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	// 执行一次性死亡处理，并为后续 WaveManager 通知留出广播点。
	void HandleDeath();
	// 初始化或刷新敌人头顶血条显示。
	void RefreshHealthBar();
	// 将 GAS 属性值同步到血条 Widget。
	void SetHealthBarValues(float Health, float MaxHealth);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UArenaAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UArenaAttributeSet> AttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetComponent> HealthBarWidgetComponent;

	FDelegateHandle DeadTagDelegateHandle;
	FDelegateHandle StunnedTagDelegateHandle;
	FDelegateHandle HealthChangedDelegateHandle;
	FDelegateHandle MoveSpeedDelegateHandle;
	TWeakObjectPtr<AActor> CombatTarget;

	bool bAppliedDefaultAttributes = false;
	bool bGrantedStartupAbilities = false;
	bool bDeathHandled = false;
};
