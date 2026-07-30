#include "Item/ArenaInventoryComponent.h"

#include "AbilitySystemComponent.h"
#include "Core/ArenaGameState.h"
#include "Core/ArenaPlayerState.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GAS/ArenaAttributeSet.h"
#include "GAS/ArenaGameplayTags.h"
#include "GameplayEffect.h"
#include "Item/ArenaInventoryPickupActor.h"
#include "Item/ArenaItemDataAsset.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogArenaInventory, Log, All);

// 创建无 Tick 的服务器权威背包组件，并把 FastArray 回调指回当前 Model。
UArenaInventoryComponent::UArenaInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	InventoryList.OwnerComponent = this;
}

// 仅向所属客户端复制 FastArray，其他玩家不接收私有背包内容。
void UArenaInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UArenaInventoryComponent, InventoryList, COND_OwnerOnly);
}

// 按连续 DisplayOrder 排序返回副本，View 不能持有可写 FastArray 引用。
TArray<FArenaInventoryEntry> UArenaInventoryComponent::GetInventorySnapshot() const
{
	TArray<FArenaInventoryEntry> Snapshot = InventoryList.Entries;
	Snapshot.Sort([](const FArenaInventoryEntry& Left, const FArenaInventoryEntry& Right)
	{
		return Left.DisplayOrder < Right.DisplayOrder;
	});
	return Snapshot;
}

// 查询最新堆栈数量，供 Drop 数量面板和服务器重验证使用。
int32 UArenaInventoryComponent::GetStackQuantity(FGuid StackId) const
{
	const FArenaInventoryEntry* Entry = InventoryList.Entries.FindByPredicate(
		[StackId](const FArenaInventoryEntry& Candidate)
		{
			return Candidate.StackId == StackId;
		});
	return Entry ? FMath::Max(Entry->Quantity, 0) : 0;
}

// Authority 重验阶段与角色状态，再以不可重入事务补满同类堆栈并标记变化。
bool UArenaInventoryComponent::TryAddItem(UArenaItemDataAsset* ItemData, int32 Quantity)
{
	const AArenaPlayerState* ArenaPlayerState = Cast<AArenaPlayerState>(GetOwner());
	if (!ArenaPlayerState || !ArenaPlayerState->HasAuthority()
		|| !ItemData || Quantity <= 0 || bInventoryMutationInProgress)
	{
		return false;
	}
	if (!CanPerformInventoryAction())
	{
		return false;
	}
	TGuardValue<bool> MutationGuard(bInventoryMutationInProgress, true);

	FText RuntimeDefinitionError;
	if (!ItemData->IsRuntimeDefinitionValid(&RuntimeDefinitionError))
	{
		UE_LOG(
			LogArenaInventory,
			Error,
			TEXT("Rejected invalid inventory definition %s: %s"),
			*GetNameSafe(ItemData),
			*RuntimeDefinitionError.ToString());
		return false;
	}

	for (const FArenaInventoryEntry& Entry : InventoryList.Entries)
	{
		if (Entry.ItemData && Entry.ItemData != ItemData && Entry.ItemData->ItemTag == ItemData->ItemTag)
		{
			UE_LOG(
				LogArenaInventory,
				Error,
				TEXT("Rejected conflicting inventory definitions %s and %s for ItemTag %s."),
				*GetNameSafe(Entry.ItemData),
				*GetNameSafe(ItemData),
				*ItemData->ItemTag.ToString());
			return false;
		}
	}

	const int32 MaxStackSize = ItemData->MaxStackSize;
	TArray<int32> MatchingEntryIndices;
	TArray<int32> ResolvedStackQuantities;
	for (int32 EntryIndex = 0; EntryIndex < InventoryList.Entries.Num(); ++EntryIndex)
	{
		const FArenaInventoryEntry& Entry = InventoryList.Entries[EntryIndex];
		if (!Entry.ItemData || Entry.ItemData->ItemTag != ItemData->ItemTag
			|| Entry.Quantity <= 0)
		{
			continue;
		}

		MatchingEntryIndices.Add(EntryIndex);
		ResolvedStackQuantities.Add(Entry.Quantity);
	}
	if (!ArenaInventory::ApplyStackAddition(ResolvedStackQuantities, MaxStackSize, Quantity))
	{
		UE_LOG(
			LogArenaInventory,
			Error,
			TEXT("Rejected invalid stack state while adding %d of %s."),
			Quantity,
			*ItemData->ItemTag.ToString());
		return false;
	}

	for (int32 MatchingIndex = 0; MatchingIndex < MatchingEntryIndices.Num(); ++MatchingIndex)
	{
		FArenaInventoryEntry& Entry = InventoryList.Entries[MatchingEntryIndices[MatchingIndex]];
		const int32 ResolvedQuantity = ResolvedStackQuantities[MatchingIndex];
		if (Entry.Quantity != ResolvedQuantity)
		{
			Entry.Quantity = ResolvedQuantity;
			InventoryList.MarkItemDirty(Entry);
		}
	}

	for (int32 NewStackIndex = MatchingEntryIndices.Num();
		NewStackIndex < ResolvedStackQuantities.Num();
		++NewStackIndex)
	{
		FArenaInventoryEntry& NewEntry = InventoryList.Entries.AddDefaulted_GetRef();
		NewEntry.StackId = FGuid::NewGuid();
		NewEntry.ItemData = ItemData;
		NewEntry.Quantity = ResolvedStackQuantities[NewStackIndex];
		NewEntry.DisplayOrder = InventoryList.Entries.Num() - 1;
		InventoryList.MarkItemDirty(NewEntry);
	}

	CompactEntries();
	NotifyInventoryChanged();
	return true;
}

// Authority 用不可重入事务应用可回滚冷却与恢复 GE；只有属性增加后才消费条目。
bool UArenaInventoryComponent::TryUseItem(FGuid StackId)
{
	if (bInventoryMutationInProgress)
	{
		return false;
	}
	TGuardValue<bool> MutationGuard(bInventoryMutationInProgress, true);

	FArenaInventoryEntry* Entry = FindMutableEntry(StackId);
	if (!Entry || Entry->Quantity <= 0 || !Entry->ItemData || !CanPerformInventoryAction())
	{
		return false;
	}

	AArenaPlayerState* ArenaPlayerState = Cast<AArenaPlayerState>(GetOwner());
	UAbilitySystemComponent* AbilitySystemComponent = ArenaPlayerState
		? ArenaPlayerState->GetAbilitySystemComponent()
		: nullptr;
	UArenaItemDataAsset* ItemData = Entry->ItemData;
	if (!AbilitySystemComponent)
	{
		return false;
	}

	FText UseConfigurationError;
	if (!ItemData->IsUseConfigurationValid(&UseConfigurationError))
	{
		UE_LOG(
			LogArenaInventory,
			Error,
			TEXT("Rejected use of invalid item asset %s: %s"),
			*GetNameSafe(ItemData),
			*UseConfigurationError.ToString());
		return false;
	}
	if (AbilitySystemComponent->HasMatchingGameplayTag(ArenaGameplayTags::Cooldown_Item_Consumable))
	{
		return false;
	}

	float ResourceBefore = 0.0f;
	float ResourceMaximum = 0.0f;
	if (!GetTrackedResourceValues(ItemData, ResourceBefore, ResourceMaximum)
		|| ResourceMaximum <= KINDA_SMALL_NUMBER
		|| ResourceBefore >= ResourceMaximum - KINDA_SMALL_NUMBER)
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(ItemData);
	FGameplayEffectSpecHandle UseSpecHandle = AbilitySystemComponent->MakeOutgoingSpec(
		ItemData->UseGameplayEffectClass,
		1.0f,
		EffectContext);
	if (!UseSpecHandle.IsValid())
	{
		return false;
	}
	FGameplayEffectSpecHandle CooldownSpecHandle = AbilitySystemComponent->MakeOutgoingSpec(
		ItemData->CooldownGameplayEffectClass,
		1.0f,
		EffectContext);
	if (!CooldownSpecHandle.IsValid())
	{
		return false;
	}

	const FActiveGameplayEffectHandle CooldownHandle =
		AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*CooldownSpecHandle.Data.Get());
	if (!CooldownHandle.IsValid())
	{
		return false;
	}

	UseSpecHandle.Data->SetSetByCallerMagnitude(ItemData->SetByCallerMagnitudeTag, ItemData->UseMagnitude);
	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*UseSpecHandle.Data.Get());

	float ResourceAfter = 0.0f;
	if (!GetTrackedResourceValues(ItemData, ResourceAfter, ResourceMaximum)
		|| ResourceAfter <= ResourceBefore + KINDA_SMALL_NUMBER)
	{
		AbilitySystemComponent->RemoveActiveGameplayEffect(CooldownHandle);
		return false;
	}

	--Entry->Quantity;
	if (Entry->Quantity > 0)
	{
		InventoryList.MarkItemDirty(*Entry);
	}
	CompactEntries();
	NotifyInventoryChanged();
	return true;
}

// Authority 验证丢弃 Pawn 属于背包 PlayerState，再以不可重入事务确认 Pickup 落地后扣除堆栈。
bool UArenaInventoryComponent::TryDropItem(FGuid StackId, int32 Quantity, APawn* SourcePawn)
{
	if (bInventoryMutationInProgress)
	{
		return false;
	}
	TGuardValue<bool> MutationGuard(bInventoryMutationInProgress, true);

	FArenaInventoryEntry* Entry = FindMutableEntry(StackId);
	AArenaPlayerState* ArenaPlayerState = Cast<AArenaPlayerState>(GetOwner());
	if (!Entry
		|| !Entry->ItemData
		|| !SourcePawn
		|| !ArenaPlayerState
		|| SourcePawn->GetPlayerState() != ArenaPlayerState
		|| !CanPerformInventoryAction())
	{
		return false;
	}

	if (!ArenaInventory::IsValidDropQuantity(Quantity, Entry->Quantity) || !GetWorld())
	{
		return false;
	}

	FTransform SpawnTransform;
	if (!FindSafeDropTransform(SourcePawn, SpawnTransform))
	{
		return false;
	}

	TSubclassOf<AArenaInventoryPickupActor> PickupClass = Entry->ItemData->WorldPickupClass;
	if (!PickupClass)
	{
		PickupClass = AArenaInventoryPickupActor::StaticClass();
	}

	AArenaInventoryPickupActor* PickupActor = GetWorld()->SpawnActorDeferred<AArenaInventoryPickupActor>(
		PickupClass,
		SpawnTransform,
		SourcePawn,
		SourcePawn,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding);
	if (!PickupActor)
	{
		UE_LOG(LogArenaInventory, Warning, TEXT("Failed to create deferred inventory pickup for %s."), *GetNameSafe(Entry->ItemData));
		return false;
	}

	if (!PickupActor->InitializePickup(Entry->ItemData, Quantity, ArenaPlayerState, 0.5f))
	{
		PickupActor->Destroy();
		return false;
	}
	AActor* FinishedActor = UGameplayStatics::FinishSpawningActor(PickupActor, SpawnTransform);
	if (!IsValid(FinishedActor))
	{
		if (IsValid(PickupActor))
		{
			PickupActor->Destroy();
		}
		return false;
	}

	Entry->Quantity -= Quantity;
	if (Entry->Quantity > 0)
	{
		InventoryList.MarkItemDirty(*Entry);
	}
	CompactEntries();
	NotifyInventoryChanged();
	return true;
}

// FastArray 的 PostReplicatedReceive 已保证批次完成，因此直接广播最终 Model 快照。
void UArenaInventoryComponent::HandleReplicatedInventoryChanged()
{
	OnInventoryChanged.Broadcast();
}

// 服务器按稳定 StackId 查找可写条目。
FArenaInventoryEntry* UArenaInventoryComponent::FindMutableEntry(FGuid StackId)
{
	AArenaPlayerState* ArenaPlayerState = Cast<AArenaPlayerState>(GetOwner());
	if (!ArenaPlayerState || !ArenaPlayerState->HasAuthority() || !StackId.IsValid())
	{
		return nullptr;
	}

	return InventoryList.Entries.FindByPredicate(
		[StackId](const FArenaInventoryEntry& Candidate)
		{
			return Candidate.StackId == StackId;
		});
}

// 删除空堆栈并重建连续顺序，只让真实移除和改序进入 FastArray 增量复制。
void UArenaInventoryComponent::CompactEntries()
{
	bool bRemovedEntry = false;
	for (int32 Index = InventoryList.Entries.Num() - 1; Index >= 0; --Index)
	{
		const FArenaInventoryEntry& Entry = InventoryList.Entries[Index];
		if (!Entry.ItemData || Entry.Quantity <= 0 || !Entry.StackId.IsValid())
		{
			InventoryList.Entries.RemoveAt(Index);
			bRemovedEntry = true;
		}
	}
	if (bRemovedEntry)
	{
		InventoryList.MarkArrayDirty();
	}

	for (int32 Index = 0; Index < InventoryList.Entries.Num(); ++Index)
	{
		FArenaInventoryEntry& Entry = InventoryList.Entries[Index];
		if (Entry.DisplayOrder != Index)
		{
			Entry.DisplayOrder = Index;
			InventoryList.MarkItemDirty(Entry);
		}
	}
}

// 调用方完成精确 Dirty 标记后，只广播一次本地变化并推动所属 PlayerState 及时复制。
void UArenaInventoryComponent::NotifyInventoryChanged()
{
	OnInventoryChanged.Broadcast();

	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}
}

// 使用和丢弃遵循 GameState 权限矩阵，同时要求权威 PlayerState 存活且未眩晕。
bool UArenaInventoryComponent::CanPerformInventoryAction() const
{
	const AArenaPlayerState* ArenaPlayerState = Cast<AArenaPlayerState>(GetOwner());
	const AArenaGameState* ArenaGameState = GetWorld() ? GetWorld()->GetGameState<AArenaGameState>() : nullptr;
	const UAbilitySystemComponent* AbilitySystemComponent = ArenaPlayerState
		? ArenaPlayerState->GetAbilitySystemComponent()
		: nullptr;
	return ArenaPlayerState
		&& ArenaPlayerState->HasAuthority()
		&& ArenaGameState
		&& ArenaGameState->CanPerformInventoryOperations()
		&& AbilitySystemComponent
		&& !AbilitySystemComponent->HasMatchingGameplayTag(ArenaGameplayTags::State_Dead)
		&& !AbilitySystemComponent->HasMatchingGameplayTag(ArenaGameplayTags::State_Stunned);
}

// 使用 SetByCaller 路由确定恢复资源，分类 ItemTags 只负责筛选而不决定玩法语义。
bool UArenaInventoryComponent::GetTrackedResourceValues(
	const UArenaItemDataAsset* ItemData,
	float& OutCurrentValue,
	float& OutMaximumValue) const
{
	OutCurrentValue = 0.0f;
	OutMaximumValue = 0.0f;
	const AArenaPlayerState* ArenaPlayerState = Cast<AArenaPlayerState>(GetOwner());
	const UArenaAttributeSet* AttributeSet = ArenaPlayerState ? ArenaPlayerState->GetArenaAttributeSet() : nullptr;
	if (!ItemData || !AttributeSet)
	{
		return false;
	}

	if (ItemData->SetByCallerMagnitudeTag == ArenaGameplayTags::SetByCaller_Recovery_Health)
	{
		OutCurrentValue = AttributeSet->GetHealth();
		OutMaximumValue = AttributeSet->GetMaxHealth();
		return true;
	}
	if (ItemData->SetByCallerMagnitudeTag == ArenaGameplayTags::SetByCaller_Recovery_Energy)
	{
		OutCurrentValue = AttributeSet->GetEnergy();
		OutMaximumValue = AttributeSet->GetMaxEnergy();
		return true;
	}
	return false;
}

// 沿玩家水平前方寻找地面，并用球形占用测试拒绝墙内、Pawn 内或其他动态物体内的落点。
bool UArenaInventoryComponent::FindSafeDropTransform(
	const APawn* SourcePawn,
	FTransform& OutSpawnTransform) const
{
	OutSpawnTransform = FTransform::Identity;
	UWorld* World = GetWorld();
	if (!SourcePawn || !World)
	{
		return false;
	}

	FVector Forward = SourcePawn->GetActorForwardVector();
	Forward.Z = 0.0f;
	if (!Forward.Normalize())
	{
		Forward = FVector::ForwardVector;
	}

	const FVector ProposedLocation = SourcePawn->GetActorLocation() + Forward * 140.0f;
	const FVector TraceStart = ProposedLocation + FVector(0.0f, 0.0f, 150.0f);
	const FVector TraceEnd = ProposedLocation - FVector(0.0f, 0.0f, 500.0f);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ArenaInventoryDropGround), false, SourcePawn);

	FHitResult GroundHit;
	if (!World->LineTraceSingleByChannel(
		GroundHit,
		TraceStart,
		TraceEnd,
		ECC_Visibility,
		QueryParams)
		|| GroundHit.ImpactNormal.Z < 0.5f)
	{
		return false;
	}

	constexpr float PickupClearanceRadius = 45.0f;
	const FVector SpawnLocation =
		GroundHit.ImpactPoint + GroundHit.ImpactNormal * (PickupClearanceRadius + 3.0f);
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	if (World->OverlapAnyTestByObjectType(
		SpawnLocation,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(PickupClearanceRadius),
		QueryParams))
	{
		return false;
	}

	OutSpawnTransform = FTransform(SourcePawn->GetActorRotation(), SpawnLocation);
	return true;
}
