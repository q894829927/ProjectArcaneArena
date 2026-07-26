#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTagContainer.h"
#include "ArenaPlayerState.generated.h"

class UArenaAbilitySystemComponent;
class UArenaAttributeSet;
class UArenaUpgradeDataAsset;
class UAbilitySystemComponent;

USTRUCT(BlueprintType)
struct FArenaOwnedUpgrade
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Upgrade")
	FName UpgradeID;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Upgrade")
	TObjectPtr<UArenaUpgradeDataAsset> UpgradeData;

	UPROPERTY(BlueprintReadOnly, Category = "Arena|Upgrade")
	int32 StackCount = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FArenaUpgradeStateChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FArenaVictoryRestartReadyChangedSignature, bool, bIsReady);

UCLASS()
class PROJECTARCANEARENA_API AArenaPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AArenaPlayerState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// PlayerState 拥有玩家 ASC，方便未来死亡重生时保留长期 GAS 状态。
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "Arena|GAS")
	UArenaAbilitySystemComponent* GetArenaAbilitySystemComponent() const;

	UFUNCTION(BlueprintPure, Category = "Arena|GAS")
	UArenaAttributeSet* GetArenaAttributeSet() const;

	// 防止重复授予启动技能，后续重生流程会复用该状态。
	bool HasGrantedStartupAbilities() const { return bGrantedStartupAbilities; }
	void SetGrantedStartupAbilities(bool bNewGrantedStartupAbilities);

	// 防止重复应用默认属性，后续可替换为 Init GameplayEffect 流程。
	bool HasAppliedDefaultAttributes() const { return bAppliedDefaultAttributes; }
	void SetAppliedDefaultAttributes(bool bNewAppliedDefaultAttributes);

	UFUNCTION(BlueprintPure, Category = "Arena|Upgrade")
	TArray<UArenaUpgradeDataAsset*> GetUpgradeCandidates() const;

	// 返回拥有者已获得的升级 ID 与层数快照，供 UI 展示构筑摘要。
	UFUNCTION(BlueprintPure, Category = "Arena|Upgrade")
	TArray<FArenaOwnedUpgrade> GetOwnedUpgrades() const { return OwnedUpgrades; }

	// 查询指定升级当前层数，服务器用它执行堆叠上限验证。
	UFUNCTION(BlueprintPure, Category = "Arena|Upgrade")
	int32 GetUpgradeStackCount(FName UpgradeID) const;

	// 按可选 Ability/伤害类型与必填升级标签汇总数值，无效路由标签表示跳过对应筛选。
	UFUNCTION(BlueprintPure, Category = "Arena|Upgrade")
	float GetOwnedUpgradeNumericTotal(
		FGameplayTag TargetAbilityTag,
		FGameplayTag DamageTypeTag,
		FGameplayTag UpgradeTag) const;

	// 返回本轮是否已经完成选择，GameMode 据此等待全部参与玩家。
	UFUNCTION(BlueprintPure, Category = "Arena|Upgrade")
	bool HasSelectedUpgrade() const { return bHasSelectedUpgrade; }

	// 返回该玩家是否已确认 Victory 重开。
	UFUNCTION(BlueprintPure, Category = "Arena|Victory")
	bool IsVictoryRestartReady() const { return bVictoryRestartReady; }

	// 仅由服务器规则层更新该玩家的 Victory 重开确认状态。
	void SetVictoryRestartReady(bool bNewReady);

	// 以下写接口仅供服务器 GameMode 管理每轮候选、选择状态和永久堆叠。
	void BeginUpgradeSelection(const TArray<UArenaUpgradeDataAsset*>& InCandidates);
	void CompleteUpgradeSelection(UArenaUpgradeDataAsset* Upgrade);
	// 无候选时不授予升级但完成本轮选择，资源恢复仍由 GameMode 负责。
	void CompleteUpgradeSelectionWithoutReward();

	UPROPERTY(BlueprintAssignable, Category = "Arena|Upgrade")
	FArenaUpgradeStateChangedSignature OnUpgradeStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Arena|Victory")
	FArenaVictoryRestartReadyChangedSignature OnVictoryRestartReadyChanged;

private:
	UFUNCTION()
	void OnRep_UpgradeCandidates();

	UFUNCTION()
	void OnRep_OwnedUpgrades();

	UFUNCTION()
	void OnRep_HasSelectedUpgrade();

	UFUNCTION()
	void OnRep_VictoryRestartReady();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UArenaAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UArenaAttributeSet> AttributeSet;

	bool bGrantedStartupAbilities = false;
	bool bAppliedDefaultAttributes = false;

	UPROPERTY(ReplicatedUsing = OnRep_UpgradeCandidates)
	TArray<TObjectPtr<UArenaUpgradeDataAsset>> UpgradeCandidates;

	UPROPERTY(ReplicatedUsing = OnRep_OwnedUpgrades)
	TArray<FArenaOwnedUpgrade> OwnedUpgrades;

	UPROPERTY(ReplicatedUsing = OnRep_HasSelectedUpgrade)
	bool bHasSelectedUpgrade = true;

	UPROPERTY(ReplicatedUsing = OnRep_VictoryRestartReady)
	bool bVictoryRestartReady = false;
};
