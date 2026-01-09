// Loot/Generation/LootGenerator.h
#pragma once

#include "CoreMinimal.h"
#include "Loot/Library/LootStruct.h"
#include "LootGenerator.generated.h"

// Forward declarations
class UItemInstance;
class UDataTable;
class UObject;

DECLARE_LOG_CATEGORY_EXTERN(LogLootGenerator, Log, All);

/**
 * FLootGenerator - Pure loot generation logic
 * 
 * SINGLE RESPONSIBILITY: Roll loot from tables and create items
 * 
 * DESIGN:
 * - Lightweight struct (no virtual dispatch)
 * - Stateless operations (thread-safe)
 * - Deterministic with seed support
 * - Integrates with existing ItemInstance/AffixGenerator system
 * 
 * USAGE:
 *   FLootGenerator Generator;
 *   FLootResultBatch Results = Generator.GenerateLoot(LootTable, Settings, Seed, Outer);
 */
USTRUCT(BlueprintType)
struct PROJECTHUNTERTEST_API FLootGenerator
{
	GENERATED_BODY()

public:
	// ═══════════════════════════════════════════════
	// MAIN GENERATION FUNCTIONS
	// ═══════════════════════════════════════════════

	/**
	 * Generate loot from a loot table
	 * @param LootTable - Source loot table
	 * @param Settings - Generation settings
	 * @param Seed - Random seed (0 = generate new seed)
	 * @param Outer - Outer object for created ItemInstances
	 * @return Batch of generated loot results
	 */
	FLootResultBatch GenerateLoot(
		const FLootTable& LootTable,
		const FLootDropSettings& Settings,
		int32 Seed,
		UObject* Outer) const;

	/**
	 * Generate loot from DataTable row handle
	 * @param TableHandle - Handle to loot table row
	 * @param Settings - Generation settings
	 * @param Seed - Random seed
	 * @param Outer - Outer object for created ItemInstances
	 * @return Batch of generated loot results
	 */
	FLootResultBatch GenerateLootFromHandle(
		const FDataTableRowHandle& TableHandle,
		const FLootDropSettings& Settings,
		int32 Seed,
		UObject* Outer) const;

	/**
	 * Generate loot with source type tracking
	 * @param LootTable - Source loot table
	 * @param Settings - Generation settings
	 * @param SourceType - What generated this loot
	 * @param Seed - Random seed
	 * @param Outer - Outer object
	 * @return Batch with source type set
	 */
	FLootResultBatch GenerateLootWithSource(
		const FLootTable& LootTable,
		const FLootDropSettings& Settings,
		ELootSourceType SourceType,
		int32 Seed,
		UObject* Outer) const;

	// ═══════════════════════════════════════════════
	// FILTERED GENERATION
	// ═══════════════════════════════════════════════

	/**
	 * Generate only corrupted items from table
	 * @param LootTable - Source table
	 * @param Settings - Generation settings
	 * @param Seed - Random seed
	 * @param Outer - Outer object
	 * @return Only corrupted item results
	 */
	FLootResultBatch GenerateCorruptedLoot(
		const FLootTable& LootTable,
		const FLootDropSettings& Settings,
		int32 Seed,
		UObject* Outer) const;

	/**
	 * Generate loot with corruption type filter
	 * @param LootTable - Source table
	 * @param Settings - Generation settings
	 * @param CorruptionFilter - Only include this corruption type
	 * @param Seed - Random seed
	 * @param Outer - Outer object
	 * @return Filtered results
	 */
	FLootResultBatch GenerateLootByCorruptionType(
		const FLootTable& LootTable,
		const FLootDropSettings& Settings,
		ECorruptionType CorruptionFilter,
		int32 Seed,
		UObject* Outer) const;

	// ═══════════════════════════════════════════════
	// SINGLE ITEM GENERATION
	// ═══════════════════════════════════════════════

	/**
	 * Create single item from loot entry
	 * @param Entry - Loot entry definition
	 * @param Settings - Generation settings
	 * @param RandStream - Random stream
	 * @param Outer - Outer object
	 * @return Created item result
	 */
	FLootResult CreateItemFromEntry(
		const FLootEntry& Entry,
		const FLootDropSettings& Settings,
		FRandomStream& RandStream,
		UObject* Outer) const;

	// ═══════════════════════════════════════════════
	// UTILITY FUNCTIONS
	// ═══════════════════════════════════════════════

	/**
	 * Roll quantity for an entry
	 * @param Entry - Source entry
	 * @param Settings - Generation settings
	 * @param RandStream - Random stream
	 * @return Final quantity
	 */
	int32 RollQuantity(
		const FLootEntry& Entry,
		const FLootDropSettings& Settings,
		FRandomStream& RandStream) const;

	/**
	 * Roll item level for an entry
	 * @param Entry - Source entry
	 * @param Settings - Generation settings
	 * @param RandStream - Random stream
	 * @return Final item level
	 */
	int32 RollItemLevel(
		const FLootEntry& Entry,
		const FLootDropSettings& Settings,
		FRandomStream& RandStream) const;

	/**
	 * Determine final rarity for item
	 * @param Entry - Source entry
	 * @param Settings - Generation settings
	 * @param RandStream - Random stream
	 * @return Final item rarity
	 */
	EItemRarity DetermineRarity(
		const FLootEntry& Entry,
		const FLootDropSettings& Settings,
		FRandomStream& RandStream) const;

	/**
	 * Check if item should be corrupted
	 * @param Entry - Source entry
	 * @param Settings - Generation settings
	 * @param RandStream - Random stream
	 * @return True if should corrupt
	 */
	bool ShouldCorruptItem(
		const FLootEntry& Entry,
		const FLootDropSettings& Settings,
		FRandomStream& RandStream) const;

	/**
	 * Get loot table from DataTable handle
	 * @param Handle - Row handle
	 * @return Pointer to loot table or nullptr
	 */
	static const FLootTable* GetLootTableFromHandle(const FDataTableRowHandle& Handle);

private:
	// ═══════════════════════════════════════════════
	// INTERNAL SELECTION METHODS
	// ═══════════════════════════════════════════════

	/**
	 * Select entries using weighted random
	 * @param Entries - Available entries
	 * @param NumToSelect - How many to select
	 * @param bAllowDuplicates - Allow same entry multiple times
	 * @param RandStream - Random stream
	 * @return Selected entry indices
	 */
	TArray<int32> SelectWeighted(
		const TArray<FLootEntry>& Entries,
		int32 NumToSelect,
		bool bAllowDuplicates,
		FRandomStream& RandStream) const;

	/**
	 * Select entries sequentially (each rolls independently)
	 * @param Entries - Available entries
	 * @param Settings - Generation settings
	 * @param RandStream - Random stream
	 * @return Selected entry indices
	 */
	TArray<int32> SelectSequential(
		const TArray<FLootEntry>& Entries,
		const FLootDropSettings& Settings,
		FRandomStream& RandStream) const;

	/**
	 * Select at least one entry (guaranteed drop)
	 * @param Entries - Available entries
	 * @param RandStream - Random stream
	 * @return Selected entry indices
	 */
	TArray<int32> SelectGuaranteedOne(
		const TArray<FLootEntry>& Entries,
		FRandomStream& RandStream) const;

	/**
	 * Select all entries that pass chance roll
	 * @param Entries - Available entries
	 * @param Settings - Generation settings
	 * @param RandStream - Random stream
	 * @return Selected entry indices
	 */
	TArray<int32> SelectAll(
		const TArray<FLootEntry>& Entries,
		const FLootDropSettings& Settings,
		FRandomStream& RandStream) const;

	/**
	 * Filter entries based on settings
	 * @param Entries - Input entries
	 * @param Settings - Filter settings
	 * @return Filtered entries
	 */
	TArray<FLootEntry> FilterEntries(
		const TArray<FLootEntry>& Entries,
		const FLootDropSettings& Settings) const;

	/**
	 * Calculate number of items to generate
	 * @param Table - Source table
	 * @param Settings - Generation settings
	 * @param RandStream - Random stream
	 * @return Number of items to generate
	 */
	int32 CalculateDropCount(
		const FLootTable& Table,
		const FLootDropSettings& Settings,
		FRandomStream& RandStream) const;

	/**
	 * Create ItemInstance from entry data
	 * @param Entry - Source entry
	 * @param ItemLevel - Rolled item level
	 * @param Rarity - Determined rarity
	 * @param bCorrupted - Should corrupt
	 * @param Seed - Item seed
	 * @param Outer - Outer object
	 * @return Created item instance
	 */
	UItemInstance* CreateItemInstance(
		const FLootEntry& Entry,
		int32 ItemLevel,
		EItemRarity Rarity,
		bool bCorrupted,
		int32 Seed,
		UObject* Outer) const;
};

// ═══════════════════════════════════════════════════════════════════════
// INLINE IMPLEMENTATIONS
// ═══════════════════════════════════════════════════════════════════════

FORCEINLINE const FLootTable* FLootGenerator::GetLootTableFromHandle(const FDataTableRowHandle& Handle)
{
	if (!Handle.DataTable || Handle.RowName.IsNone())
	{
		return nullptr;
	}
	
	return Handle.DataTable->FindRow<FLootTable>(Handle.RowName, TEXT("FLootGenerator::GetLootTableFromHandle"));
}