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

	UPROPERTY(BlueprintAssignable, Category = "Arena|Enemy")
	FArenaEnemyDeathSignature OnEnemyDeath;

protected:
	// 初始化敌人 GAS、属性和表现委托。
	virtual void BeginPlay() override;
	// 解绑 GAS 委托，避免销毁时留下无效回调。
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TSubclassOf<UGameplayEffect> DefaultAttributeEffect;

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

	// 受击表现入口，后续可接闪白、音效或伤害数字。
	UFUNCTION(BlueprintImplementableEvent, Category = "Arena|Enemy")
	void K2_OnDamaged(float DamageAmount, float NewHealth, float MaxHealth);

private:
	// 以敌人自身作为 OwnerActor 和 AvatarActor 初始化 ASC。
	void InitializeAbilityActorInfo();
	// 通过默认 GameplayEffect 初始化敌人属性，保持 GAS 数据流一致。
	void ApplyDefaultAttributes();
	// 绑定死亡标签和 Health 属性变化，用事件驱动死亡与反馈。
	void BindAbilitySystemDelegates();
	// 解绑死亡标签和 Health 属性变化委托。
	void UnbindAbilitySystemDelegates();
	// State.Dead 标签变化是死亡逻辑的唯一入口。
	void HandleDeadTagChanged(const FGameplayTag CallbackTag, int32 NewCount);
	// Health 变化只负责 UI 和受击表现，不直接触发死亡。
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	// 执行一次性死亡处理，并为后续 WaveManager 通知留出广播点。
	void HandleDeath();
	// 初始化或刷新敌人头顶血条显示。
	void RefreshHealthBar();
	// 将 GAS 属性值同步到血条 Widget。
	void SetHealthBarValues(float Health, float MaxHealth);
	// 本地生成伤害数字 Actor，不参与复制和伤害结算。
	void SpawnDamageNumber(float DamageAmount);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UArenaAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UArenaAttributeSet> AttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetComponent> HealthBarWidgetComponent;

	FDelegateHandle DeadTagDelegateHandle;
	FDelegateHandle HealthChangedDelegateHandle;

	bool bAppliedDefaultAttributes = false;
	bool bDeathHandled = false;
};
