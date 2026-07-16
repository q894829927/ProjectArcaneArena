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

	// 服务器 AI 写入当前战斗目标，Ability 激活后仍会重新校验该目标。
	void SetCombatTarget(AActor* NewCombatTarget);
	AActor* GetCombatTarget() const { return CombatTarget.Get(); }
	// 激活 StartupAbilities 中第一个 EnemyAttackBase 子类，供近战与远程 AI 共用。
	bool TryActivatePrimaryAttack();
	// 返回主攻击 CDO 的距离，AI 将它与主攻击路径共同用于追击和停步决策。
	float GetPrimaryAttackRange() const;
	// 使用主攻击自身的视线或弹道规则判断当前目标是否可攻击。
	bool HasPrimaryAttackPath(AActor* TargetActor);
	// 取消当前主攻击实例，目标死亡或失效时阻止迟到命中和 Projectile。
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

	// 蓝图死亡表现入口，后续可接死亡动画、Niagara 和音效。
	UFUNCTION(BlueprintImplementableEvent, Category = "Arena|Enemy")
	void K2_OnDeathStarted();

	// Health 变化表现入口，UI 和反馈只观察属性，不拥有玩法状态。
	UFUNCTION(BlueprintImplementableEvent, Category = "Arena|Enemy")
	void K2_OnHealthChanged(float OldHealth, float NewHealth, float MaxHealth);

	// Health 受损表现入口，后续可接闪白、音效；伤害数字由确认伤害 Cue 独立驱动。
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
	// Health 变化只负责血条和受击表现；死亡与伤害数字分别由标签和 Cue 驱动。
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
