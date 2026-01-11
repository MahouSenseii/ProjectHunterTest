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
 * - CORRUPTION: Roll negative affixes (ERankPoints < 0)
 * 
 * CORRUPTION SYSTEM:
 * - Corruption = negative affixes that hurt the player
 * - Uses ERankPoints to distinguish positive vs negative
 * - CorruptionChance determines probability per affix
 * - bForceOneCorrupted guarantees at least one negative affix
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
	 * Generate affixes for an item with corruption support
	 * @param BaseItem - Base item data
	 * @param ItemLevel - Item level (1-100)
	 * @param Rarity - Item rarity (determines affix count)
	 * @param Seed - Random seed for deterministic generation
	 * @param CorruptionChance - Chance for each affix to be corrupted (0.0 - 1.0)
	 * @param bForceOneCorrupted - Force at least one corrupted affix
	 * @return Generated stats with affixes
	 */
	FPHItemStats GenerateAffixes(
		const FItemBase& BaseItem,
		int32 ItemLevel,
		EItemRarity Rarity,
		int32 Seed,
		float CorruptionChance = 0.0f,
		bool bForceOneCorrupted = false) const;

	/**
	 * Generate affixes with custom affix counts (no corruption)
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
	// AFFIX COUNT HELPERS
	// ═══════════════════════════════════════════════

	/**
	 * Get affix counts for rarity (Hunter Manga F-SS grades)
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
	 * Get affix DataTable (lazy loaded with attempt tracking)
	 */
	UDataTable* GetAffixDataTable() const;

private:
	// ═══════════════════════════════════════════════
	// AFFIX ROLLING (Internal)
	// ═══════════════════════════════════════════════

	/**
	 * Roll affixes with corruption consideration
	 * @param AffixType - Prefix or Suffix
	 * @param Count - Number of affixes to roll
	 * @param ItemLevel - Item level for filtering
	 * @param ItemType - Item type for filtering
	 * @param ItemSubType - Item subtype for filtering
	 * @param CorruptionChance - Per-affix corruption chance
	 * @param bMustRollOneCorrupted - Force one corrupted if not yet rolled
	 * @param bOutHasRolledCorrupted - Output: whether a corrupted was rolled
	 * @param RandStream - Random stream
	 */
	TArray<FPHAttributeData> RollAffixesWithCorruption(
		EAffixes AffixType,
		int32 Count,
		int32 ItemLevel,
		EItemType ItemType,
		EItemSubType ItemSubType,
		float CorruptionChance,
		bool bMustRollOneCorrupted,
		bool& bOutHasRolledCorrupted,
		FRandomStream& RandStream) const;

	/**
	 * Build available affix pool filtered by corruption
	 * @param bCorruptedOnly - If true, only return negative affixes
	 */
	TArray<FPHAttributeData*> BuildAffixPoolByCorruption(
		EAffixes AffixType,
		EItemType ItemType,
		EItemSubType ItemSubType,
		int32 ItemLevel,
		bool bCorruptedOnly,
		const TArray<FName>& ExcludeAffixes) const;

	/**
	 * Select random affix from pool (weighted)
	 */
	const FPHAttributeData* SelectRandomAffix(
		const TArray<FPHAttributeData*>& AvailableAffixes,
		FRandomStream& RandStream) const;

	/**
	 * Create rolled affix instance from DataTable row
	 */
	FPHAttributeData CreateRolledAffix(
		const FPHAttributeData& TemplateAffix,
		FRandomStream& RandStream) const;

	// ═══════════════════════════════════════════════
	// CACHED DATA
	// ═══════════════════════════════════════════════

	/** Cached affix DataTable */
	mutable UDataTable* CachedAffixTable = nullptr;
	
	/** Track if we've attempted to load (prevents repeated failures) */
	mutable bool bLoadAttempted = false;
};
