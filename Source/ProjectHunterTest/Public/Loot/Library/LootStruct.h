// Loot/Library/LootStruct.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Item/ItemInstance.h"
#include "Loot/Library/LootEnum.h"
#include "Item/Library/ItemEnums.h"
#include "LootStruct.generated.h"

// Forward declarations
class UItemInstance;


// ═══════════════════════════════════════════════════════════════════════
// LOOT DROP SETTINGS - Generation parameters
// ═══════════════════════════════════════════════════════════════════════

/**
 * FLootDropSettings - Controls HOW loot is generated
 * 
 * SINGLE RESPONSIBILITY: Generation parameters only
 */
USTRUCT(BlueprintType)
struct FLootDropSettings
{
	GENERATED_BODY()

	// ═══════════════════════════════════════════════
	// QUANTITY MODIFIERS
	// ═══════════════════════════════════════════════

	/** Minimum guaranteed drops */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quantity", meta = (ClampMin = "0"))
	int32 MinDrops = 1;

	/** Maximum possible drops */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quantity", meta = (ClampMin = "1"))
	int32 MaxDrops = 3;

	/** Multiplier for all drop chances */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quantity", meta = (ClampMin = "0.0"))
	float DropChanceMultiplier = 1.0f;

	/** Multiplier for all quantities */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quantity", meta = (ClampMin = "0.0"))
	float QuantityMultiplier = 1.0f;

	// ═══════════════════════════════════════════════
	// RARITY MODIFIERS
	// ═══════════════════════════════════════════════

	/** Base rarity of this loot source */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rarity")
	EDropRarity SourceRarity = EDropRarity::DR_Common;

	/** Bonus rarity chance (increases rare drop odds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rarity", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RarityBonusChance = 0.0f;

	/** Force minimum rarity (IR_None = no minimum) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rarity")
	EItemRarity MinimumItemRarity = EItemRarity::IR_None;

	// ═══════════════════════════════════════════════
	// ITEM LEVEL
	// ═══════════════════════════════════════════════

	/** Source level (affects item level generation) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level", meta = (ClampMin = "1", ClampMax = "100"))
	int32 SourceLevel = 1;

	/** Item level variance (+/- from source level) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level", meta = (ClampMin = "0"))
	int32 LevelVariance = 2;

	// ═══════════════════════════════════════════════
	// CORRUPTION
	// ═══════════════════════════════════════════════

	/** Chance for dropped items to become corrupted */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Corruption", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CorruptionChance = 0.0f;

	/** Only drop corrupted items from this source? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Corruption")
	bool bOnlyCorruptedDrops = false;

	/** Exclude corrupted entries from pool? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Corruption")
	bool bExcludeCorruptedEntries = false;

	// ═══════════════════════════════════════════════
	// PLAYER MODIFIERS (Runtime bonuses)
	// ═══════════════════════════════════════════════

	/** Player luck bonus (affects rare drops) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
	float PlayerLuckBonus = 0.0f;

	/** Player magic find bonus (affects quantity) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
	float PlayerMagicFindBonus = 0.0f;

	FLootDropSettings()
		: MinDrops(1)
		, MaxDrops(3)
		, DropChanceMultiplier(1.0f)
		, QuantityMultiplier(1.0f)
		, SourceRarity(EDropRarity::DR_Common)
		, RarityBonusChance(0.0f)
		, MinimumItemRarity(EItemRarity::IR_None)
		, SourceLevel(1)
		, LevelVariance(2)
		, CorruptionChance(0.0f)
		, bOnlyCorruptedDrops(false)
		, bExcludeCorruptedEntries(false)
		, PlayerLuckBonus(0.0f)
		, PlayerMagicFindBonus(0.0f)
	{}
};

// ═══════════════════════════════════════════════════════════════════════
// LOOT SOURCE REGISTRY - Maps source IDs to loot tables
// ═══════════════════════════════════════════════════════════════════════

/**
 * FLootSourceEntry - DataTable row mapping a drop source to its loot table
 * 
 * SINGLE RESPONSIBILITY: Define the mapping from source → loot data
 * 
 * USAGE:
 *   Create DT_LootSourceRegistry with rows like:
 *   - "Goblin_Basic" → Category: NPC, Table: DT_GoblinLoot
 *   - "Chest_Common" → Category: Chest, Table: DT_CommonChestLoot
 *   - "Barrel_Dungeon" → Category: Breakable, Table: DT_BarrelLoot
 */
USTRUCT(BlueprintType)
struct FLootSourceEntry : public FTableRowBase
{
	GENERATED_BODY()

	// ═══════════════════════════════════════════════
	// IDENTIFICATION
	// ═══════════════════════════════════════════════

	/** Display name for this loot source */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Info")
	FText DisplayName;

	/** Category for organization (NPC, Chest, Breakable, Boss, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Info")
	ELootSourceType Category = ELootSourceType::LST_None;

	/** Tags for filtering (e.g., "Dungeon", "Overworld", "Event") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Info")
	TArray<FName> Tags;

	// ═══════════════════════════════════════════════
	// LOOT TABLE REFERENCE
	// ═══════════════════════════════════════════════

	/** Soft reference to loot DataTable (lazy loaded) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	TSoftObjectPtr<UDataTable> LootTable;

	/** Row name within the loot table (empty = use all rows) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	FName LootTableRowName;

	// ═══════════════════════════════════════════════
	// DEFAULT SETTINGS
	// ═══════════════════════════════════════════════

	/** Default drop settings for this source */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FLootDropSettings DefaultSettings;

	/** Override rarity (DR_Common = use settings) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	EDropRarity SourceRarity = EDropRarity::DR_Common;

	/** Base level for item generation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ClampMin = "1", ClampMax = "100"))
	int32 BaseLevel = 1;

	// ═══════════════════════════════════════════════
	// CURRENCY & REWARDS
	// ═══════════════════════════════════════════════

	/** Minimum currency drop */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Currency", meta = (ClampMin = "0"))
	int32 MinCurrency = 0;

	/** Maximum currency drop */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Currency", meta = (ClampMin = "0"))
	int32 MaxCurrency = 0;

	/** Experience reward (for NPCs) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rewards", meta = (ClampMin = "0"))
	int32 ExperienceReward = 0;

	// ═══════════════════════════════════════════════
	// FLAGS
	// ═══════════════════════════════════════════════

	/** Is this source enabled? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flags")
	bool bEnabled = true;

	/** Is this a boss source? (guaranteed drops) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flags")
	bool bIsBoss = false;

	/** Scale drops with player count? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flags")
	bool bScaleWithPlayerCount = false;

	// ═══════════════════════════════════════════════
	// VALIDATION
	// ═══════════════════════════════════════════════

	bool IsValid() const
	{
		return bEnabled && !LootTable.IsNull();
	}

	bool HasTag(FName Tag) const
	{
		return Tags.Contains(Tag);
	}

	FLootSourceEntry()
		: Category(ELootSourceType::LST_None)
		, SourceRarity(EDropRarity::DR_Common)
		, BaseLevel(1)
		, MinCurrency(0)
		, MaxCurrency(0)
		, ExperienceReward(0)
		, bEnabled(true)
		, bIsBoss(false)
		, bScaleWithPlayerCount(false)
	{}
};

// ═══════════════════════════════════════════════════════════════════════
// LOOT ENTRY - Single possible drop definition
// ═══════════════════════════════════════════════════════════════════════

/**
 * FLootEntry - Defines a single possible item drop
 * 
 * SINGLE RESPONSIBILITY: Define what CAN drop and probability
 * NOTE: Does NOT hold affixes - ItemInstance::Initialize() handles that
 */
USTRUCT(BlueprintType)
struct FLootEntry
{
	GENERATED_BODY()

	// ═══════════════════════════════════════════════
	// ITEM REFERENCE
	// ═══════════════════════════════════════════════

	/** Use direct class reference instead of DataTable row? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	bool bUseDirectClass = false;

	/** Row handle to item DataTable (primary reference method) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item", meta = (EditCondition = "!bUseDirectClass"))
	FDataTableRowHandle ItemRowHandle;

	/** Direct item class reference */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item", meta = (EditCondition = "bUseDirectClass"))
	TSubclassOf<UItemInstance> ItemClass;
	
	// ═══════════════════════════════════════════════
	// DROP PROBABILITY
	// ═══════════════════════════════════════════════

	/** Drop chance [0.0 - 1.0] (1.0 = 100% guaranteed) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Probability", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DropChance = 1.0f;

	/** Weight for weighted random selection (higher = more likely) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Probability", meta = (ClampMin = "0.1"))
	float Weight = 1.0f;

	// ═══════════════════════════════════════════════
	// QUANTITY
	// ═══════════════════════════════════════════════

	/** Minimum quantity per drop */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quantity", meta = (ClampMin = "1"))
	int32 MinQuantity = 1;

	/** Maximum quantity per drop */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quantity", meta = (ClampMin = "1"))
	int32 MaxQuantity = 1;

	// ═══════════════════════════════════════════════
	// ITEM GENERATION SETTINGS
	// ═══════════════════════════════════════════════

	/** Override base rarity (IR_None = use item's default) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	EItemRarity OverrideRarity = EItemRarity::IR_None;

	/** Generate affixes for this drop? (equipment only) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	bool bGenerateAffixes = true;

	/** Use item level for affix generation? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	bool bUseItemLevel = true;

	/** Minimum item level (if bUseItemLevel) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (EditCondition = "bUseItemLevel", ClampMin = "1", ClampMax = "100"))
	int32 MinItemLevel = 1;

	/** Maximum item level (if bUseItemLevel) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (EditCondition = "bUseItemLevel", ClampMin = "1", ClampMax = "100"))
	int32 MaxItemLevel = 100;

	// ═══════════════════════════════════════════════
	// CORRUPTION FLAGS
	// ═══════════════════════════════════════════════

	/** Is this a corrupted item entry? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Corruption")
	bool bIsCorrupted = false;

	/** Corruption type (if corrupted) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Corruption", meta = (EditCondition = "bIsCorrupted"))
	ECorruptionType CorruptionType = ECorruptionType::CT_None;

	/** Can this entry produce corrupted variants? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Corruption")
	bool bCanBeCorrupted = true;

	// ═══════════════════════════════════════════════
	// VALIDATION
	// ═══════════════════════════════════════════════

	/** Check if entry has valid item reference */
	FORCEINLINE bool IsValid() const
	{
		return ItemRowHandle.DataTable != nullptr || ItemClass != nullptr;
	}

	/** Get effective weight (accounts for drop chance) */
	FORCEINLINE float GetEffectiveWeight() const
	{
		return Weight * DropChance;
	}

	FLootEntry()
		: ItemClass(nullptr)
		, DropChance(1.0f)
		, Weight(1.0f)
		, MinQuantity(1)
		, MaxQuantity(1)
		, OverrideRarity(EItemRarity::IR_None)
		, bGenerateAffixes(true)
		, bUseItemLevel(true)
		, MinItemLevel(1)
		, MaxItemLevel(100)
		, bIsCorrupted(false)
		, CorruptionType(ECorruptionType::CT_None)
		, bCanBeCorrupted(true)
	{}
};

// ═══════════════════════════════════════════════════════════════════════
// LOOT TABLE - Collection of entries (DataTable row)
// ═══════════════════════════════════════════════════════════════════════

/**
 * FLootTable - DataTable row containing loot entries
 * 
 * SINGLE RESPONSIBILITY: Define a collection of possible drops
 */
USTRUCT(BlueprintType)
struct FLootTable : public FTableRowBase
{
	GENERATED_BODY()

	/** Display name for this loot table */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Info")
	FText DisplayName;

	/** All possible loot entries in this table */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	TArray<FLootEntry> Entries;

	/** Selection method for this table */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	ELootSelectionMethod SelectionMethod = ELootSelectionMethod::LSM_Weighted;

	/** Allow duplicate items from same entry? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bAllowDuplicates = true;

	/** Minimum entries to select (0 = use SelectionMethod logic) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ClampMin = "0"))
	int32 MinSelections = 0;

	/** Maximum entries to select (0 = unlimited) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ClampMin = "0"))
	int32 MaxSelections = 0;

	// ═══════════════════════════════════════════════
	// UTILITY FUNCTIONS
	// ═══════════════════════════════════════════════

	/** Get total weight of all entries */
	float GetTotalWeight() const
	{
		float Total = 0.0f;
		for (const FLootEntry& Entry : Entries)
		{
			if (Entry.IsValid())
			{
				Total += Entry.GetEffectiveWeight();
			}
		}
		return Total;
	}

	/** Get only corrupted entries */
	TArray<FLootEntry> GetCorruptedEntries() const
	{
		TArray<FLootEntry> Result;
		for (const FLootEntry& Entry : Entries)
		{
			if (Entry.bIsCorrupted && Entry.IsValid())
			{
				Result.Add(Entry);
			}
		}
		return Result;
	}

	/** Get non-corrupted entries */
	TArray<FLootEntry> GetNormalEntries() const
	{
		TArray<FLootEntry> Result;
		for (const FLootEntry& Entry : Entries)
		{
			if (!Entry.bIsCorrupted && Entry.IsValid())
			{
				Result.Add(Entry);
			}
		}
		return Result;
	}

	/** Filter entries by corruption type */
	TArray<FLootEntry> GetEntriesByCorruptionType(ECorruptionType Type) const
	{
		TArray<FLootEntry> Result;
		for (const FLootEntry& Entry : Entries)
		{
			if (Entry.CorruptionType == Type && Entry.IsValid())
			{
				Result.Add(Entry);
			}
		}
		return Result;
	}

	FLootTable()
		: SelectionMethod(ELootSelectionMethod::LSM_Weighted)
		, bAllowDuplicates(true)
		, MinSelections(0)
		, MaxSelections(0)
	{}
};



// ═══════════════════════════════════════════════════════════════════════
// LOOT RESULT - Output from loot generation
// ═══════════════════════════════════════════════════════════════════════

/**
 * FLootResult - Single generated item result
 * 
 * SINGLE RESPONSIBILITY: Hold generation result data
 */
USTRUCT(BlueprintType)
struct FLootResult
{
	GENERATED_BODY()

	/** Generated item instance */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	UItemInstance* Item = nullptr;

	/** Quantity of this item */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 Quantity = 1;

	/** Source entry index (for debugging) */
	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	int32 SourceEntryIndex = -1;

	/** Was this item corrupted during generation? */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	bool bWasCorrupted = false;

	FLootResult()
		: Item(nullptr)
		, Quantity(1)
		, SourceEntryIndex(-1)
		, bWasCorrupted(false)
	{}

	FLootResult(UItemInstance* InItem, int32 InQuantity, int32 InSourceIndex = -1, bool bCorrupted = false)
		: Item(InItem)
		, Quantity(InQuantity)
		, SourceEntryIndex(InSourceIndex)
		, bWasCorrupted(bCorrupted)
	{}

	bool IsValid() const { return Item != nullptr && Quantity > 0; }
};

/**
 * FLootResultBatch - Collection of generated results
 */
USTRUCT(BlueprintType)
struct FLootResultBatch
{
	GENERATED_BODY()

	/** All generated items */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	TArray<FLootResult> Results;

	/** Total items generated */
	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 TotalItemCount = 0;

	/** Currency dropped */
	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 CurrencyDropped = 0;

	/** Experience reward */
	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 ExperienceReward = 0;

	/** Source type that generated this loot */
	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	ELootSourceType SourceType = ELootSourceType::LST_None;

	/** Source ID (row name from registry) */
	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	FName SourceID;

	/** Seed used for generation (for replication) */
	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 GenerationSeed = 0;

	void AddResult(const FLootResult& Result)
	{
		if (Result.IsValid())
		{
			Results.Add(Result);
			TotalItemCount += Result.Quantity;
		}
	}

	void Clear()
	{
		Results.Empty();
		TotalItemCount = 0;
		CurrencyDropped = 0;
		ExperienceReward = 0;
	}

	FLootResultBatch()
		: TotalItemCount(0)
		, CurrencyDropped(0)
		, ExperienceReward(0)
		, SourceType(ELootSourceType::LST_None)
		, GenerationSeed(0)
	{}
};

// ═══════════════════════════════════════════════════════════════════════
// LOOT REQUEST - Input for loot generation
// ═══════════════════════════════════════════════════════════════════════

/**
 * FLootRequest - All data needed to generate loot
 * 
 * SINGLE RESPONSIBILITY: Encapsulate generation request
 */
USTRUCT(BlueprintType)
struct FLootRequest
{
	GENERATED_BODY()

	/** Source ID (row name in DT_LootSourceRegistry) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Request")
	FName SourceID;

	/** Override settings (optional - uses source defaults if not set) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Request")
	FLootDropSettings OverrideSettings;

	/** Use override settings? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Request")
	bool bUseOverrideSettings = false;

	/** Player luck stat */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Request")
	float PlayerLuck = 0.0f;

	/** Player magic find stat */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Request")
	float PlayerMagicFind = 0.0f;

	/** Override level (0 = use source default) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Request", meta = (ClampMin = "0", ClampMax = "100"))
	int32 OverrideLevel = 0;

	/** Random seed (0 = generate) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Request")
	int32 Seed = 0;

	/** Number of players (for scaling) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Request", meta = (ClampMin = "1"))
	int32 PlayerCount = 1;

	FLootRequest()
		: bUseOverrideSettings(false)
		, PlayerLuck(0.0f)
		, PlayerMagicFind(0.0f)
		, OverrideLevel(0)
		, Seed(0)
		, PlayerCount(1)
	{}

	FLootRequest(FName InSourceID)
		: SourceID(InSourceID)
		, bUseOverrideSettings(false)
		, PlayerLuck(0.0f)
		, PlayerMagicFind(0.0f)
		, OverrideLevel(0)
		, Seed(0)
		, PlayerCount(1)
	{}
};

// ═══════════════════════════════════════════════════════════════════════
// SPAWN SETTINGS - How to spawn loot in world
// ═══════════════════════════════════════════════════════════════════════

/**
 * FLootSpawnSettings - Controls how loot is spawned in world
 */
USTRUCT(BlueprintType)
struct FLootSpawnSettings
{
	GENERATED_BODY()

	/** Base spawn location */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	FVector SpawnLocation = FVector::ZeroVector;

	/** Scatter radius */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "0.0"))
	float ScatterRadius = 100.0f;

	/** Height offset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	float HeightOffset = 50.0f;

	/** Random scatter vs organized circle */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	bool bRandomScatter = true;

	/** Apply physics impulse to items */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	bool bApplyImpulse = false;

	/** Impulse direction (normalized) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (EditCondition = "bApplyImpulse"))
	FVector ImpulseDirection = FVector::UpVector;

	/** Impulse strength */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (EditCondition = "bApplyImpulse", ClampMin = "0.0"))
	float ImpulseStrength = 200.0f;

	FLootSpawnSettings()
		: SpawnLocation(FVector::ZeroVector)
		, ScatterRadius(100.0f)
		, HeightOffset(50.0f)
		, bRandomScatter(true)
		, bApplyImpulse(false)
		, ImpulseDirection(FVector::UpVector)
		, ImpulseStrength(200.0f)
	{}

	FLootSpawnSettings(FVector Location, float Radius = 100.0f)
		: SpawnLocation(Location)
		, ScatterRadius(Radius)
		, HeightOffset(50.0f)
		, bRandomScatter(true)
		, bApplyImpulse(false)
		, ImpulseDirection(FVector::UpVector)
		, ImpulseStrength(200.0f)
	{}
};