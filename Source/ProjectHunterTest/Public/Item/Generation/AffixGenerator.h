// Item/Generation/AffixGenerator.h
#pragma once

#include "CoreMinimal.h"
#include "Item/Library/ItemStructs.h"
#include "Item/Library/ItemEnums.h"
#include "Item/Library/AffixEnums.h"
#include "AffixGenerator.generated.h"

/**
 * Affix Generator - Handles all affix generation logic
 * PoE2-Style: Directly uses FPHAttributeData rows from DT_Affixes
 * 
 * SINGLE RESPONSIBILITY: Generate affixes for items
 * - Weighted affix selection from DataTable
 * - Value rolling within min-max range
 * - Item level filtering
 * - Rarity-based affix count
 * 
 * Usage:
 *   FAffixGenerator Generator;
 *   FPHItemStats Stats = Generator.GenerateAffixes(BaseItem, ItemLevel, Rarity, Seed);
 * 
 * SIMPLIFIED DESIGN:
 * - No complex FAffixData/FAffixTier structs
 * - Directly uses FPHAttributeData from DataTable
 * - Each DataTable row = one complete affix
 * - Designer-friendly (no code changes needed)
 */
USTRUCT(BlueprintType)
struct PROJECTHUNTERTEST_API FAffixGenerator
{
	GENERATED_BODY()

public:
	// ═══════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════

	/** Path to affix DataTable (DT_Affixes) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affix Generator")
	FSoftObjectPath AffixDataTablePath = FSoftObjectPath(TEXT("/Game/Data/Items/DT_Affixes"));

	/** Default weight for affixes without explicit weight */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affix Generator")
	int32 DefaultAffixWeight = 100;

	// ═══════════════════════════════════════════════
	// MAIN GENERATION FUNCTIONS
	// ═══════════════════════════════════════════════

	/**
	 * Generate affixes for an item
	 * @param BaseItem - Base item data
	 * @param ItemLevel - Item level (1-100)
	 * @param Rarity - Item rarity (determines affix count)
	 * @param Seed - Random seed for deterministic generation
	 * @return Generated stats with affixes
	 */
	FPHItemStats GenerateAffixes(
		const FItemBase& BaseItem,
		int32 ItemLevel,
		EItemRarity Rarity,
		int32 Seed) const;

	/**
	 * Generate affixes with custom affix counts
	 * @param BaseItem - Base item data
	 * @param ItemLevel - Item level
	 * @param NumPrefixes - Number of prefixes to roll
	 * @param NumSuffixes - Number of suffixes to roll
	 * @param Seed - Random seed
	 * @return Generated stats with affixes
	 */
	FPHItemStats GenerateAffixesCustom(
		const FItemBase& BaseItem,
		int32 ItemLevel,
		int32 NumPrefixes,
		int32 NumSuffixes,
		int32 Seed) const;

	// ═══════════════════════════════════════════════
	// AFFIX ROLLING (Internal)
	// ═══════════════════════════════════════════════

private:
	/**
	 * Roll affixes of specific type
	 * @param AffixType - Prefix or Suffix
	 * @param Count - Number of affixes to roll
	 * @param ItemLevel - Item level (for filtering)
	 * @param ItemType - Equipment type (for pool filtering)
	 * @param ItemSubType - Weapon/armor subtype
	 * @param RandStream - Random stream for rolling
	 * @return Array of rolled affixes
	 */
	TArray<FPHAttributeData> RollAffixes(
		EAffixes AffixType,
		int32 Count,
		int32 ItemLevel,
		EItemType ItemType,
		EItemSubType ItemSubType,
		FRandomStream& RandStream) const;

	/**
	 * Select random affix from pool (weighted)
	 * @param AvailableAffixes - Pool of available affixes
	 * @param RandStream - Random stream
	 * @return Selected affix, or nullptr if pool empty
	 */
	const FPHAttributeData* SelectRandomAffix(
		const TArray<FPHAttributeData*>& AvailableAffixes,
		FRandomStream& RandStream) const;

	/**
	 * Build available affix pool
	 * @param AffixType - Prefix or Suffix
	 * @param ItemType - Equipment type
	 * @param ItemSubType - Weapon/armor subtype
	 * @param ExcludeAffixes - Already rolled affixes to exclude
	 * @return Array of available affixes from DataTable
	 */
	TArray<FPHAttributeData*> BuildAffixPool(
		EAffixes AffixType,
		EItemType ItemType,
		EItemSubType ItemSubType,
		const TArray<FName>& ExcludeAffixes) const;

	/**
	 * Create rolled affix instance from DataTable row
	 * @param TemplateAffix - Template affix from DataTable
	 * @param RandStream - Random stream for value rolling
	 * @return Rolled attribute with generated UID and value
	 */
	FPHAttributeData CreateRolledAffix(
		const FPHAttributeData& TemplateAffix,
		FRandomStream& RandStream) const;

	// ═══════════════════════════════════════════════
	// AFFIX COUNT HELPERS
	// ═══════════════════════════════════════════════

	/**
	 * Get affix counts for rarity (Hunter Manga F-SS grades)
	 * @param Rarity - Item rarity
	 * @param OutMinPrefixes - Minimum prefixes
	 * @param OutMaxPrefixes - Maximum prefixes
	 * @param OutMinSuffixes - Minimum suffixes
	 * @param OutMaxSuffixes - Maximum suffixes
	 */
	static void GetAffixCountByRarity(
		EItemRarity Rarity,
		int32& OutMinPrefixes,
		int32& OutMaxPrefixes,
		int32& OutMinSuffixes,
		int32& OutMaxSuffixes);

	// ═══════════════════════════════════════════════
	// DATATABLE ACCESS
	// ═══════════════════════════════════════════════

	/**
	 * Get affix DataTable (lazy loaded)
	 * @return Pointer to affix DataTable (DT_Affixes)
	 */
	UDataTable* GetAffixDataTable() const;

	/** Cached affix DataTable */
	mutable UDataTable* CachedAffixTable = nullptr;
};