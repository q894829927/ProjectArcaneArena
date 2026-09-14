#include "Core/ArenaPlayerState.h"

#include "Core/ArenaBalanceTelemetryComponent.h"
#include "Core/ArenaGameState.h"
#include "Core/ArenaUpgradeDataAsset.h"
#include "Engine/World.h"
#include "GAS/ArenaAbilitySystemComponent.h"
#include "GAS/ArenaAttributeSet.h"
#include "Item/ArenaInventoryComponent.h"
#include "Net/UnrealNetwork.h"

// 构造玩家状态，创建跨角色生命周期的 ASC、AttributeSet 和背包 Model。
AArenaPlayerState::AArenaPlayerState()
{
	SetNetUpdateFrequency(100.0f);
	SetMinNetUpdateFrequency(33.0f);

	// 玩家 ASC 放在 PlayerState 上，后续死亡重生时可以保留长期 GAS 状态。
	AbilitySystemComponent = CreateDefaultSubobject<UArenaAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UArenaAttributeSet>(TEXT("AttributeSet"));
	// 显式注册 AttributeSet 子对象，确保 ASC 能发现并复制属性。
	AbilitySystemComponent->AddAttributeSetSubobject(AttributeSet.Get());

	InventoryComponent = CreateDefaultSubobject<UArenaInventoryComponent>(TEXT("InventoryComponent"));
}

// 复制 OwnerOnly 升级数据、公共选择完成状态与 Victory Ready，UI 只观察这些数据。
void AArenaPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AArenaPlayerState, UpgradeCandidates, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AArenaPlayerState, OwnedUpgrades, COND_OwnerOnly);
	DOREPLIFETIME(AArenaPlayerState, bHasSelectedUpgrade);
	DOREPLIFETIME(AArenaPlayerState, bVictoryRestartReady);
}

// 服务器更新该玩家的 Victory 重开确认状态，并立即同步 Listen Server UI。
void AArenaPlayerState::SetVictoryRestartReady(bool bNewReady)
{
	if (!HasAuthority() || bVictoryRestartReady == bNewReady)
	{
		return;
	}

	bVictoryRestartReady = bNewReady;
	OnVictoryRestartReadyChanged.Broadcast(bVictoryRestartReady);
	ForceNetUpdate();
}

// 返回标准 GAS 接口需要的 AbilitySystemComponent。
UAbilitySystemComponent* AArenaPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

// 返回项目自定义 ASC，供输入路由和项目扩展调用。
UArenaAbilitySystemComponent* AArenaPlayerState::GetArenaAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

// 返回玩家 AttributeSet，供 HUD 和初始化流程读取属性。
UArenaAttributeSet* AArenaPlayerState::GetArenaAttributeSet() const
{
	return AttributeSet;
}

// 返回 PlayerState 持有的背包组件，重生只更换 Avatar 时仍保留本局物品。
UArenaInventoryComponent* AArenaPlayerState::GetInventoryComponent() const
{
	return InventoryComponent;
}

// 记录启动技能是否已经授予，避免 Possess/复制路径重复授予。
void AArenaPlayerState::SetGrantedStartupAbilities(bool bNewGrantedStartupAbilities)
{
	bGrantedStartupAbilities = bNewGrantedStartupAbilities;
}

// 记录默认属性是否已经应用，避免重生或重复初始化时叠加属性。
void AArenaPlayerState::SetAppliedDefaultAttributes(bool bNewAppliedDefaultAttributes)
{
	bAppliedDefaultAttributes = bNewAppliedDefaultAttributes;
}

// 返回拥有者当前可选的 Upgrade DataAsset 快照，避免外部直接修改复制数组。
TArray<UArenaUpgradeDataAsset*> AArenaPlayerState::GetUpgradeCandidates() const
{
	TArray<UArenaUpgradeDataAsset*> Result;
	Result.Reserve(UpgradeCandidates.Num());
	for (UArenaUpgradeDataAsset* Candidate : UpgradeCandidates)
	{
		if (Candidate)
		{
			Result.Add(Candidate);
		}
	}
	return Result;
}

// 查询指定升级的永久堆叠层数，供服务器资格校验和 UI 展示。
int32 AArenaPlayerState::GetUpgradeStackCount(FName UpgradeID) const
{
	for (const FArenaOwnedUpgrade& OwnedUpgrade : OwnedUpgrades)
	{
		if (OwnedUpgrade.UpgradeID == UpgradeID)
		{
			return OwnedUpgrade.StackCount;
		}
	}
	return 0;
}

// 汇总匹配路由标签的已拥有升级数值；无效 Ability 或伤害标签表示该维度不参与筛选。
float AArenaPlayerState::GetOwnedUpgradeNumericTotal(
	FGameplayTag TargetAbilityTag,
	FGameplayTag DamageTypeTag,
	FGameplayTag UpgradeTag) const
{
	if (!UpgradeTag.IsValid())
	{
		return 0.0f;
	}

	float TotalValue = 0.0f;
	for (const FArenaOwnedUpgrade& OwnedUpgrade : OwnedUpgrades)
	{
		const UArenaUpgradeDataAsset* UpgradeData = OwnedUpgrade.UpgradeData;
		if (!UpgradeData || OwnedUpgrade.StackCount <= 0
			|| (TargetAbilityTag.IsValid() && UpgradeData->TargetAbilityTag != TargetAbilityTag)
			|| (DamageTypeTag.IsValid() && UpgradeData->DamageTypeTag != DamageTypeTag)
			|| !UpgradeData->UpgradeTags.HasTagExact(UpgradeTag))
		{
			continue;
		}

		TotalValue += UpgradeData->NumericValue * static_cast<float>(OwnedUpgrade.StackCount);
	}

	return TotalValue;
}

// 开始新一轮服务器权威选择，并通过 OwnerOnly 候选复制驱动本地界面。
void AArenaPlayerState::BeginUpgradeSelection(const TArray<UArenaUpgradeDataAsset*>& InCandidates)
{
	if (!HasAuthority())
	{
		return;
	}

	UpgradeCandidates.Reset();
	for (UArenaUpgradeDataAsset* Candidate : InCandidates)
	{
		if (Candidate)
		{
			UpgradeCandidates.Add(Candidate);
		}
	}
	bHasSelectedUpgrade = false;
	OnUpgradeStateChanged.Broadcast();
	ForceNetUpdate();
}

// 记录已验证升级和永久堆叠，随后把结果层数上报服务器平衡统计并关闭候选。
void AArenaPlayerState::CompleteUpgradeSelection(UArenaUpgradeDataAsset* Upgrade)
{
	if (!HasAuthority() || !Upgrade || Upgrade->UpgradeID.IsNone())
	{
		return;
	}
	const FName UpgradeID = Upgrade->UpgradeID;

	FArenaOwnedUpgrade* ExistingUpgrade = OwnedUpgrades.FindByPredicate(
		[UpgradeID](const FArenaOwnedUpgrade& Entry)
		{
			return Entry.UpgradeID == UpgradeID;
		});
	if (ExistingUpgrade)
	{
		ExistingUpgrade->UpgradeData = Upgrade;
		++ExistingUpgrade->StackCount;
	}
	else
	{
		FArenaOwnedUpgrade& NewUpgrade = OwnedUpgrades.AddDefaulted_GetRef();
		NewUpgrade.UpgradeID = UpgradeID;
		NewUpgrade.UpgradeData = Upgrade;
		NewUpgrade.StackCount = 1;
	}

	if (const UWorld* World = GetWorld())
	{
		if (const AArenaGameState* GameState = World->GetGameState<AArenaGameState>())
		{
			if (UArenaBalanceTelemetryComponent* Telemetry =
				GameState->GetBalanceTelemetryComponent())
			{
				Telemetry->RecordUpgradeSelected(
					this,
					UpgradeID,
					GetUpgradeStackCount(UpgradeID));
			}
		}
	}

	UpgradeCandidates.Reset();
	bHasSelectedUpgrade = true;
	OnUpgradeStateChanged.Broadcast();
	ForceNetUpdate();
}

// 无候选时由服务器无奖励完成本轮选择并清空候选，避免全员选择门槛永久阻塞。
void AArenaPlayerState::CompleteUpgradeSelectionWithoutReward()
{
	if (!HasAuthority())
	{
		return;
	}

	UpgradeCandidates.Reset();
	bHasSelectedUpgrade = true;
	OnUpgradeStateChanged.Broadcast();
	ForceNetUpdate();
}

// OwnerOnly 候选复制后通知本地 Controller 刷新升级界面。
void AArenaPlayerState::OnRep_UpgradeCandidates()
{
	OnUpgradeStateChanged.Broadcast();
}

// 已拥有升级复制后通知本地展示刷新层数或构筑摘要。
void AArenaPlayerState::OnRep_OwnedUpgrades()
{
	OnUpgradeStateChanged.Broadcast();
}

// 选择完成状态复制后关闭本地界面或继续等待其他玩家。
void AArenaPlayerState::OnRep_HasSelectedUpgrade()
{
	OnUpgradeStateChanged.Broadcast();
}

// Victory Ready 状态复制后刷新本地终局界面。
void AArenaPlayerState::OnRep_VictoryRestartReady()
{
	OnVictoryRestartReadyChanged.Broadcast(bVictoryRestartReady);
}
