#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "ArenaAttributeSet.generated.h"

#define ARENA_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

struct FGameplayEffectModCallbackData;

UCLASS()
class PROJECTARCANEARENA_API UArenaAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UArenaAttributeSet();

	// 注册需要复制的属性，配合 RepNotify 驱动客户端 UI。
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	// 修改 CurrentValue 前统一 clamp，防止属性越界。
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	// 修改 BaseValue 前统一 clamp，覆盖 Instant GE 等基础值变化。
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	// 处理 Damage/Healing 元属性，并在实际伤害生效后派发 Cue 与权威结果事件。
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	ARENA_ATTRIBUTE_ACCESSORS(UArenaAttributeSet, Health);
	ARENA_ATTRIBUTE_ACCESSORS(UArenaAttributeSet, MaxHealth);
	ARENA_ATTRIBUTE_ACCESSORS(UArenaAttributeSet, Shield);
	ARENA_ATTRIBUTE_ACCESSORS(UArenaAttributeSet, Energy);
	ARENA_ATTRIBUTE_ACCESSORS(UArenaAttributeSet, MaxEnergy);
	ARENA_ATTRIBUTE_ACCESSORS(UArenaAttributeSet, AttackPower);
	ARENA_ATTRIBUTE_ACCESSORS(UArenaAttributeSet, Defense);
	ARENA_ATTRIBUTE_ACCESSORS(UArenaAttributeSet, MoveSpeed);
	ARENA_ATTRIBUTE_ACCESSORS(UArenaAttributeSet, CritChance);
	ARENA_ATTRIBUTE_ACCESSORS(UArenaAttributeSet, CritDamage);
	ARENA_ATTRIBUTE_ACCESSORS(UArenaAttributeSet, Damage);
	ARENA_ATTRIBUTE_ACCESSORS(UArenaAttributeSet, Healing);

protected:
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Arena|Attributes")
	FGameplayAttributeData Health;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Arena|Attributes")
	FGameplayAttributeData MaxHealth;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Shield, Category = "Arena|Attributes")
	FGameplayAttributeData Shield;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Energy, Category = "Arena|Attributes")
	FGameplayAttributeData Energy;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxEnergy, Category = "Arena|Attributes")
	FGameplayAttributeData MaxEnergy;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AttackPower, Category = "Arena|Attributes")
	FGameplayAttributeData AttackPower;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Defense, Category = "Arena|Attributes")
	FGameplayAttributeData Defense;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MoveSpeed, Category = "Arena|Attributes")
	FGameplayAttributeData MoveSpeed;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CritChance, Category = "Arena|Attributes")
	FGameplayAttributeData CritChance;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CritDamage, Category = "Arena|Attributes")
	FGameplayAttributeData CritDamage;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Attributes|Meta")
	FGameplayAttributeData Damage;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Attributes|Meta")
	FGameplayAttributeData Healing;

	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Shield(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Energy(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxEnergy(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_AttackPower(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Defense(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MoveSpeed(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_CritChance(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_CritDamage(const FGameplayAttributeData& OldValue);

private:
	// 统一属性边界规则，避免多个执行路径各自 clamp。
	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;
	// 由服务端根据 Health 同步 State.Dead，死亡表现监听该标签。
	void UpdateDeadTag() const;
	// 根据权威 Shield 数值添加或移除持续 Cue，重复补盾不会重复 Add。
	void RefreshShieldGameplayCue();
	// 将同次伤害的元素命中与普通/暴击数字 Cue 合并成一个权威 Multicast。
	void ExecuteDamageFeedbackGameplayCues(const FGameplayEffectModCallbackData& Data, float AppliedDamage) const;

	bool bShieldGameplayCueActive = false;
};
