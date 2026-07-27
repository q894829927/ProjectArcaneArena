#include "Item/ArenaInventoryTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GAS/ArenaGameplayTags.h"
#include "Item/ArenaItemDataAsset.h"
#include "Misc/AutomationTest.h"

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

#endif
