#include "Item/ArenaInventoryTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/ArenaGameState.h"
#include "Core/ArenaPlayerState.h"
#include "Engine/StaticMesh.h"
#include "GAS/ArenaGameplayEffect_ConsumableCooldown.h"
#include "GAS/ArenaGameplayEffect_HealthRestore.h"
#include "GAS/ArenaGameplayEffect_Stunned.h"
#include "GAS/ArenaGameplayTags.h"
#include "Item/ArenaInventoryComponent.h"
#include "Item/ArenaInventoryPickupActor.h"
#include "Item/ArenaItemDataAsset.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FArenaInventoryPaginationTest,
	"ProjectArcaneArena.Inventory.Pagination",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 验证空背包保留一页，并覆盖计划规定的二十槽分页边界。
bool FArenaInventoryPaginationTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	TestEqual(TEXT("Negative stack count remains one page"), ArenaInventory::CalculatePageCount(-1), 1);
	TestEqual(TEXT("0 stacks use one page"), ArenaInventory::CalculatePageCount(0), 1);
	TestEqual(TEXT("19 stacks use one page"), ArenaInventory::CalculatePageCount(19), 1);
	TestEqual(TEXT("20 stacks use one page"), ArenaInventory::CalculatePageCount(20), 1);
	TestEqual(TEXT("21 stacks use two pages"), ArenaInventory::CalculatePageCount(21), 2);
	TestEqual(TEXT("40 stacks use two pages"), ArenaInventory::CalculatePageCount(40), 2);
	TestEqual(TEXT("41 stacks use three pages"), ArenaInventory::CalculatePageCount(41), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FArenaInventoryStackingTest,
	"ProjectArcaneArena.Inventory.Stacking",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 验证补栈、溢出拆栈及丢弃数量边界均走 Model 与 View 共用的纯规则函数。
bool FArenaInventoryStackingTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);

	TArray<int32> EmptyInventory;
	TestTrue(
		TEXT("Eleven potions split into ten and one"),
		ArenaInventory::ApplyStackAddition(EmptyInventory, 10, 11));
	TestEqual(TEXT("Eleven potions create two stacks"), EmptyInventory.Num(), 2);
	TestEqual(TEXT("First new stack is full"), EmptyInventory[0], 10);
	TestEqual(TEXT("Second new stack keeps overflow"), EmptyInventory[1], 1);

	TArray<int32> ExistingStacks = { 7, 10 };
	TestTrue(
		TEXT("Addition fills the first partial stack before appending"),
		ArenaInventory::ApplyStackAddition(ExistingStacks, 10, 8));
	TestEqual(TEXT("Partial stack becomes full"), ExistingStacks[0], 10);
	TestEqual(TEXT("Existing full stack remains unchanged"), ExistingStacks[1], 10);
	TestEqual(TEXT("Remaining quantity creates a third stack"), ExistingStacks[2], 5);

	TArray<int32> MultiStackOverflow = { 9 };
	TestTrue(
		TEXT("Large additions deterministically create multiple stacks"),
		ArenaInventory::ApplyStackAddition(MultiStackOverflow, 10, 25));
	const TArray<int32> ExpectedOverflow = { 10, 10, 10, 4 };
	TestTrue(TEXT("Large addition stack distribution"), MultiStackOverflow == ExpectedOverflow);

	TArray<int32> InvalidExistingStack = { 11 };
	TestFalse(
		TEXT("Existing quantities above MaxStackSize are rejected"),
		ArenaInventory::ApplyStackAddition(InvalidExistingStack, 10, 1));
	TestFalse(
		TEXT("Zero added quantity is rejected"),
		ArenaInventory::ApplyStackAddition(EmptyInventory, 10, 0));
	TestFalse(
		TEXT("Zero MaxStackSize is rejected"),
		ArenaInventory::ApplyStackAddition(EmptyInventory, 0, 1));
	TestTrue(
		TEXT("A positive quantity up to the current stack can be dropped"),
		ArenaInventory::IsValidDropQuantity(4, 10));
	TestTrue(
		TEXT("The entire current stack can be dropped"),
		ArenaInventory::IsValidDropQuantity(10, 10));
	TestFalse(
		TEXT("Zero drop quantity is rejected instead of becoming one"),
		ArenaInventory::IsValidDropQuantity(0, 10));
	TestFalse(
		TEXT("Negative drop quantity is rejected"),
		ArenaInventory::IsValidDropQuantity(-1, 10));
	TestFalse(
		TEXT("Drop quantity above the current stack is rejected"),
		ArenaInventory::IsValidDropQuantity(11, 10));
	TestFalse(
		TEXT("Drop requests against an empty stack are rejected"),
		ArenaInventory::IsValidDropQuantity(1, 0));

	TArray<int32> TransactionLimitStacks = { 5 };
	const TArray<int32> TransactionLimitSnapshot = TransactionLimitStacks;
	const int32 ExcessiveAddition =
		ArenaInventory::MaxNewStacksPerAddition * 10 + 6;
	TestFalse(
		TEXT("One malformed pickup cannot allocate more than the per-transaction stack limit"),
		ArenaInventory::ApplyStackAddition(TransactionLimitStacks, 10, ExcessiveAddition));
	TestTrue(
		TEXT("Rejected oversized additions leave every existing stack unchanged"),
		TransactionLimitStacks == TransactionLimitSnapshot);

	TArray<int32> MaximumAcceptedAddition;
	TestTrue(
		TEXT("Exactly the per-transaction stack limit remains valid"),
		ArenaInventory::ApplyStackAddition(
			MaximumAcceptedAddition,
			10,
			ArenaInventory::MaxNewStacksPerAddition * 10));
	TestEqual(
		TEXT("The accepted boundary creates the configured maximum number of stacks"),
		MaximumAcceptedAddition.Num(),
		ArenaInventory::MaxNewStacksPerAddition);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FArenaInventoryTabGestureTest,
	"ProjectArcaneArena.Inventory.TabGesture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 验证轻点切换、长按临时查看和迟到松开均使用 Controller 共用的纯判定规则。
bool FArenaInventoryTabGestureTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	TestFalse(
		TEXT("A short press from closed keeps the newly opened inventory visible"),
		ArenaInventory::ShouldCloseOnTabRelease(false, true, 0.1, 0.25));
	TestTrue(
		TEXT("A long press from closed closes its temporary inventory on release"),
		ArenaInventory::ShouldCloseOnTabRelease(false, true, 0.25, 0.25));
	TestTrue(
		TEXT("Pressing Tab while already open closes on release regardless of duration"),
		ArenaInventory::ShouldCloseOnTabRelease(true, true, 0.01, 0.25));
	TestFalse(
		TEXT("A delayed release cannot close an inventory already closed by another path"),
		ArenaInventory::ShouldCloseOnTabRelease(true, false, 1.0, 0.25));
	TestFalse(
		TEXT("Negative durations are clamped before evaluating the hold threshold"),
		ArenaInventory::ShouldCloseOnTabRelease(false, true, -1.0, 0.25));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FArenaInventoryPhaseAccessTest,
	"ProjectArcaneArena.Inventory.PhaseAccess",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 验证背包查看、完整操作和 Waiting 测试授权使用同一份确定性阶段矩阵。
bool FArenaInventoryPhaseAccessTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	TestTrue(
		TEXT("Waiting allows inventory view"),
		AArenaGameState::IsInventoryViewAllowedForPhase(EArenaGamePhase::Waiting));
	TestFalse(
		TEXT("Waiting is read-only by default"),
		AArenaGameState::AreInventoryOperationsAllowedForPhase(EArenaGamePhase::Waiting, false));
	TestTrue(
		TEXT("Waiting test override allows operations"),
		AArenaGameState::AreInventoryOperationsAllowedForPhase(EArenaGamePhase::Waiting, true));
	TestTrue(
		TEXT("Combat allows operations"),
		AArenaGameState::AreInventoryOperationsAllowedForPhase(EArenaGamePhase::Combat, false));
	TestTrue(
		TEXT("Upgrade allows inventory view"),
		AArenaGameState::IsInventoryViewAllowedForPhase(EArenaGamePhase::Upgrade));
	TestFalse(
		TEXT("Upgrade remains read-only"),
		AArenaGameState::AreInventoryOperationsAllowedForPhase(EArenaGamePhase::Upgrade, true));
	TestFalse(
		TEXT("BossIntro blocks inventory view"),
		AArenaGameState::IsInventoryViewAllowedForPhase(EArenaGamePhase::BossIntro));
	TestFalse(
		TEXT("BossOutro blocks inventory view"),
		AArenaGameState::IsInventoryViewAllowedForPhase(EArenaGamePhase::BossOutro));
	TestTrue(
		TEXT("Victory allows full inventory operations"),
		AArenaGameState::AreInventoryOperationsAllowedForPhase(EArenaGamePhase::Victory, false));
	TestTrue(
		TEXT("Defeat allows inventory view"),
		AArenaGameState::IsInventoryViewAllowedForPhase(EArenaGamePhase::Defeat));
	TestFalse(
		TEXT("Defeat remains read-only"),
		AArenaGameState::AreInventoryOperationsAllowedForPhase(EArenaGamePhase::Defeat, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FArenaInventoryReplicationContractTest,
	"ProjectArcaneArena.Inventory.ReplicationContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 验证背包 Model 随 PlayerState 存在，并且私有 FastArray 只向所属客户端复制。
bool FArenaInventoryReplicationContractTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);

	const AArenaPlayerState* PlayerStateCDO = GetDefault<AArenaPlayerState>();
	TestNotNull(TEXT("PlayerState CDO exists"), PlayerStateCDO);
	if (!PlayerStateCDO)
	{
		return false;
	}

	const UArenaInventoryComponent* InventoryComponent = PlayerStateCDO->GetInventoryComponent();
	TestNotNull(TEXT("PlayerState owns an InventoryComponent default subobject"), InventoryComponent);
	if (!InventoryComponent)
	{
		return false;
	}

	TestTrue(
		TEXT("InventoryComponent is a default subobject"),
		InventoryComponent->HasAnyFlags(RF_DefaultSubObject));
	TestTrue(
		TEXT("InventoryComponent replicates by default"),
		InventoryComponent->GetIsReplicated());

	const FProperty* InventoryListProperty = FindFProperty<FProperty>(
		UArenaInventoryComponent::StaticClass(),
		FName(TEXT("InventoryList")));
	TestNotNull(TEXT("InventoryList remains a reflected property"), InventoryListProperty);
	if (!InventoryListProperty)
	{
		return false;
	}

	TestTrue(
		TEXT("InventoryList remains marked for replication"),
		InventoryListProperty->HasAnyPropertyFlags(CPF_Net));

	TArray<FLifetimeProperty> LifetimeProperties;
	InventoryComponent->GetLifetimeReplicatedProps(LifetimeProperties);
	const FLifetimeProperty* InventoryLifetimeProperty = LifetimeProperties.FindByPredicate(
		[InventoryListProperty](const FLifetimeProperty& Candidate)
		{
			return Candidate.RepIndex == InventoryListProperty->RepIndex;
		});
	TestNotNull(
		TEXT("InventoryList is registered in lifetime replication"),
		InventoryLifetimeProperty);
	if (InventoryLifetimeProperty)
	{
		TestEqual(
			TEXT("InventoryList uses OwnerOnly replication"),
			InventoryLifetimeProperty->Condition,
			COND_OwnerOnly);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FArenaInventoryFilterTest,
	"ProjectArcaneArena.Inventory.Filters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 验证无筛选、多个筛选 OR、父标签层级与唯一 ItemTag 均使用同一匹配规则。
bool FArenaInventoryFilterTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	UArenaItemDataAsset* HealthPotion = NewObject<UArenaItemDataAsset>(GetTransientPackage());
	HealthPotion->ItemTag = ArenaGameplayTags::Item_Consumable_HealthPotion;
	HealthPotion->ItemTags.AddTag(ArenaGameplayTags::Item_Type_Consumable);
	HealthPotion->ItemTags.AddTag(ArenaGameplayTags::Item_Effect_Restore_Health);

	const FGameplayTag ItemTypeParent =
		FGameplayTag::RequestGameplayTag(FName(TEXT("Item.Type")), false);
	const FGameplayTag RestoreParent =
		FGameplayTag::RequestGameplayTag(FName(TEXT("Item.Effect.Restore")), false);
	const FGameplayTag ConsumableParent =
		FGameplayTag::RequestGameplayTag(FName(TEXT("Item.Consumable")), false);
	TestTrue(TEXT("Generated Item.Type parent tag exists"), ItemTypeParent.IsValid());
	TestTrue(TEXT("Generated restore parent tag exists"), RestoreParent.IsValid());
	TestTrue(TEXT("Generated consumable parent tag exists"), ConsumableParent.IsValid());

	FGameplayTagContainer Filters;
	TestTrue(
		TEXT("No active filters include a valid item"),
		ArenaInventory::MatchesFilters(HealthPotion, Filters));

	Filters.AddTag(ArenaGameplayTags::Item_Effect_Restore_Energy);
	Filters.AddTag(ArenaGameplayTags::Item_Effect_Restore_Health);
	TestTrue(
		TEXT("Multiple filters use OR semantics"),
		ArenaInventory::MatchesFilters(HealthPotion, Filters));

	Filters.Reset();
	Filters.AddTag(RestoreParent);
	TestTrue(
		TEXT("Parent filter matches a child item tag"),
		ArenaInventory::MatchesFilters(HealthPotion, Filters));

	Filters.Reset();
	Filters.AddTag(ItemTypeParent);
	TestTrue(
		TEXT("Item type parent filter matches category tag"),
		ArenaInventory::MatchesFilters(HealthPotion, Filters));

	Filters.Reset();
	Filters.AddTag(ArenaGameplayTags::Build_Fire);
	TestFalse(
		TEXT("Unrelated filter excludes the item"),
		ArenaInventory::MatchesFilters(HealthPotion, Filters));

	UArenaItemDataAsset* ItemTagOnlyPotion = NewObject<UArenaItemDataAsset>(GetTransientPackage());
	ItemTagOnlyPotion->ItemTag = ArenaGameplayTags::Item_Consumable_HealthPotion;
	Filters.Reset();
	Filters.AddTag(ConsumableParent);
	TestTrue(
		TEXT("Unique ItemTag participates in hierarchical filtering"),
		ArenaInventory::MatchesFilters(ItemTagOnlyPotion, Filters));
	TestFalse(
		TEXT("Null item is never included"),
		ArenaInventory::MatchesFilters(nullptr, Filters));
	return true;
}

#if WITH_EDITOR

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FArenaInventoryItemValidationTest,
	"ProjectArcaneArena.Inventory.ItemValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 验证物品资产在进入 PIE 前拒绝缺失恢复事务或错误 GE 生命周期的配置。
bool FArenaInventoryItemValidationTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);

	UArenaItemDataAsset* ValidPotion = NewObject<UArenaItemDataAsset>(GetTransientPackage());
	ValidPotion->ItemTag = ArenaGameplayTags::Item_Consumable_HealthPotion;
	ValidPotion->DisplayName = FText::FromString(TEXT("Health Potion"));
	ValidPotion->MaxStackSize = 10;
	ValidPotion->UseGameplayEffectClass = UArenaGameplayEffect_HealthRestore::StaticClass();
	ValidPotion->SetByCallerMagnitudeTag = ArenaGameplayTags::SetByCaller_Recovery_Health;
	ValidPotion->UseMagnitude = 50.0f;
	ValidPotion->CooldownGameplayEffectClass = UArenaGameplayEffect_ConsumableCooldown::StaticClass();
	ValidPotion->WorldPickupClass = AArenaInventoryPickupActor::StaticClass();

	FDataValidationContext ValidContext;
	TestEqual(
		TEXT("Complete Health Potion configuration is valid"),
		ValidPotion->IsDataValid(ValidContext),
		EDataValidationResult::Valid);
	TestEqual(TEXT("Missing optional icon produces one warning"), ValidContext.GetNumWarnings(), 1u);
	TestEqual(TEXT("Valid potion produces no validation errors"), ValidContext.GetNumErrors(), 0u);

	UArenaItemDataAsset* InvalidPotion = NewObject<UArenaItemDataAsset>(GetTransientPackage());
	InvalidPotion->ItemTag = ArenaGameplayTags::Item_Consumable_HealthPotion;
	InvalidPotion->DisplayName = FText::FromString(TEXT("Broken Potion"));
	InvalidPotion->MaxStackSize = 10;
	InvalidPotion->UseGameplayEffectClass = UArenaGameplayEffect_ConsumableCooldown::StaticClass();
	InvalidPotion->SetByCallerMagnitudeTag = ArenaGameplayTags::Build_Fire;
	InvalidPotion->UseMagnitude = 50.0f;
	InvalidPotion->CooldownGameplayEffectClass = UArenaGameplayEffect_HealthRestore::StaticClass();
	InvalidPotion->WorldPickupClass = AArenaInventoryPickupActor::StaticClass();

	FText UseConfigurationError;
	TestFalse(
		TEXT("Unsupported SetByCaller recovery route is rejected"),
		InvalidPotion->IsUseConfigurationValid(&UseConfigurationError));
	TestFalse(TEXT("Unsupported recovery route reports an error"), UseConfigurationError.IsEmpty());

	InvalidPotion->SetByCallerMagnitudeTag = ArenaGameplayTags::SetByCaller_Recovery_Health;
	TestFalse(
		TEXT("Duration GameplayEffect cannot be used as atomic recovery"),
		InvalidPotion->IsUseConfigurationValid(&UseConfigurationError));

	InvalidPotion->UseGameplayEffectClass = UArenaGameplayEffect_HealthRestore::StaticClass();
	TestFalse(
		TEXT("Instant GameplayEffect cannot be used as consumable cooldown"),
		InvalidPotion->IsUseConfigurationValid(&UseConfigurationError));

	FDataValidationContext InvalidContext;
	TestEqual(
		TEXT("Misconfigured cooldown makes the item asset invalid"),
		InvalidPotion->IsDataValid(InvalidContext),
		EDataValidationResult::Invalid);
	TestTrue(
		TEXT("Invalid potion reports an actionable configuration error"),
		InvalidContext.GetNumErrors() >= 1u);

	ValidPotion->MaxStackSize = 0;
	FText RuntimeDefinitionError;
	TestFalse(
		TEXT("Runtime definition rejects zero MaxStackSize"),
		ValidPotion->IsRuntimeDefinitionValid(&RuntimeDefinitionError));
	TestFalse(
		TEXT("Invalid runtime stack definition reports an error"),
		RuntimeDefinitionError.IsEmpty());

	ValidPotion->MaxStackSize = 10;
	ValidPotion->ItemTag = FGameplayTag();
	TestFalse(
		TEXT("Runtime definition rejects a missing unique ItemTag"),
		ValidPotion->IsRuntimeDefinitionValid(&RuntimeDefinitionError));

	ValidPotion->ItemTag = ArenaGameplayTags::Item_Consumable_HealthPotion;
	ValidPotion->UseMagnitude = 0.0f;
	TestFalse(
		TEXT("Runtime definition rejects a zero recovery magnitude"),
		ValidPotion->IsRuntimeDefinitionValid(&RuntimeDefinitionError));

	ValidPotion->UseMagnitude = 50.0f;
	ValidPotion->CooldownGameplayEffectClass = UArenaGameplayEffect_Stunned::StaticClass();
	TestFalse(
		TEXT("Duration effects without Cooldown.Item.Consumable cannot be used as the shared cooldown"),
		ValidPotion->IsRuntimeDefinitionValid(&RuntimeDefinitionError));

	ValidPotion->CooldownGameplayEffectClass = UArenaGameplayEffect_ConsumableCooldown::StaticClass();
	ValidPotion->WorldMesh = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	ValidPotion->WorldMeshRelativeScale = FVector(0.25f, 0.0f, 0.25f);
	FDataValidationContext InvalidWorldVisualContext;
	TestEqual(
		TEXT("Configured world meshes reject invisible scale components"),
		ValidPotion->IsDataValid(InvalidWorldVisualContext),
		EDataValidationResult::Invalid);
	return true;
}

#endif

#endif
