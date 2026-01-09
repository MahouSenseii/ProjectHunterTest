// Loot/Subsystem/LootSubsystem.h
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Loot/Library/LootStruct.h"
#include "Loot/Generation/LootGenerator.h"
#include "LootSubsystem.generated.h"

// Forward declarations
class UGroundItemSubsystem;
class UItemInstance;
class UDataTable;

DECLARE_LOG_CATEGORY_EXTERN(LogLootSubsystem, Log, All);

// ═══════════════════════════════════════════════════════════════════════
// DELEGATES
// ═══════════════════════════════════════════════════════════════════════

/** Called when loot is generated */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLootGeneratedDelegate, const FLootResultBatch&, Results, FName, SourceID);

/** Called when item is spawned on ground */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnLootSpawnedDelegate, UItemInstance*, Item, FVector, Location, int32, GroundItemID);

/** Called when a loot table is loaded */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLootTableLoadedDelegate, FName, SourceID, bool, bSuccess);

/**
 * ULootSubsystem - Central loot generation and registry management
 * 
 * SINGLE RESPONSIBILITY: 
 * - Manage loot source registry (Source ID → Loot Table mapping)
 * - Generate loot from registered sources
 * - Coordinate with GroundItemSubsystem for spawning
 * 
 * DESIGN:
 * - World Subsystem (single instance per world)
 * - Lazy loading of loot tables (TSoftObjectPtr)
 * - Caching of loaded tables
 * - Server-authoritative loot generation
 * - Deterministic with seed support
 * 
 * USAGE:
 *   // Get subsystem
 *   ULootSubsystem* LootSub = GetWorld()->GetSubsystem<ULootSubsystem>();
 *   
 *   // Generate loot by source ID
 *   FLootRequest Request("Goblin_Basic");
 *   Request.PlayerLuck = PlayerStats->GetLuck();
 *   FLootResultBatch Results = LootSub->GenerateLoot(Request);
 *   
 *   // Spawn the generated loot
 *   LootSub->SpawnLootResults(Results, SpawnSettings);
 */
UCLASS()
class PROJECTHUNTERTEST_API ULootSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// ═══════════════════════════════════════════════
	// SUBSYSTEM LIFECYCLE
	// ═══════════════════════════════════════════════

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

	// ═══════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════

	/** Path to loot source registry DataTable */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot|Config")
	TSoftObjectPtr<UDataTable> LootSourceRegistryPath;

	/** Global drop chance multiplier (for events, difficulty, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot|Config", meta = (ClampMin = "0.0"))
	float GlobalDropChanceMultiplier = 1.0f;

	/** Global quantity multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot|Config", meta = (ClampMin = "0.0"))
	float GlobalQuantityMultiplier = 1.0f;

	/** Global corruption chance modifier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GlobalCorruptionChanceModifier = 0.0f;

	// ═══════════════════════════════════════════════
	// EVENTS
	// ═══════════════════════════════════════════════

	/** Broadcast when loot is generated */
	UPROPERTY(BlueprintAssignable, Category = "Loot|Events")
	FOnLootGeneratedDelegate OnLootGenerated;

	/** Broadcast when item is spawned */
	UPROPERTY(BlueprintAssignable, Category = "Loot|Events")
	FOnLootSpawnedDelegate OnLootSpawned;

	/** Broadcast when loot table is loaded */
	UPROPERTY(BlueprintAssignable, Category = "Loot|Events")
	FOnLootTableLoadedDelegate OnLootTableLoaded;

	// ═══════════════════════════════════════════════
	// PRIMARY GENERATION API
	// ═══════════════════════════════════════════════

	/**
	 * Generate loot from a registered source
	 * @param Request - Loot generation request
	 * @return Generated loot batch
	 */
	UFUNCTION(BlueprintCallable, Category = "Loot|Generation")
	FLootResultBatch GenerateLoot(const FLootRequest& Request);

	/**
	 * Generate loot by source ID with player stats
	 * @param SourceID - Row name in loot source registry
	 * @param PlayerLuck - Player's luck stat
	 * @param PlayerMagicFind - Player's magic find stat
	 * @param Seed - Random seed (0 = random)
	 * @return Generated loot batch
	 */
	UFUNCTION(BlueprintCallable, Category = "Loot|Generation")
	FLootResultBatch GenerateLootByID(
		FName SourceID,
		float PlayerLuck = 0.0f,
		float PlayerMagicFind = 0.0f,
		int32 Seed = 0);

	/**
	 * Generate and spawn loot in one call
	 * @param Request - Loot generation request
	 * @param SpawnSettings - How to spawn the loot
	 * @return Generated loot batch
	 */
	UFUNCTION(BlueprintCallable, Category = "Loot|Generation")
	FLootResultBatch GenerateAndSpawnLoot(
		const FLootRequest& Request,
		const FLootSpawnSettings& SpawnSettings);

	// ═══════════════════════════════════════════════
	// SPAWNING API
	// ═══════════════════════════════════════════════

	/**
	 * Spawn generated loot results in world
	 * @param Results - Already generated results
	 * @param SpawnSettings - Spawn configuration
	 */
	UFUNCTION(BlueprintCallable, Category = "Loot|Spawn")
	void SpawnLootResults(
		const FLootResultBatch& Results,
		const FLootSpawnSettings& SpawnSettings);

	/**
	 * Spawn single item at location
	 * @param Item - Item to spawn
	 * @param Location - World location
	 * @return Ground item ID (-1 on failure)
	 */
	UFUNCTION(BlueprintCallable, Category = "Loot|Spawn")
	int32 SpawnItemAtLocation(UItemInstance* Item, FVector Location);

	/**
	 * Calculate scatter positions for items
	 * @param Settings - Spawn settings
	 * @param NumItems - Number of positions needed
	 * @return Array of world positions
	 */
	UFUNCTION(BlueprintPure, Category = "Loot|Spawn")
	TArray<FVector> CalculateScatterPositions(
		const FLootSpawnSettings& Settings,
		int32 NumItems) const;

	// ═══════════════════════════════════════════════
	// REGISTRY QUERIES
	// ═══════════════════════════════════════════════

	/**
	 * Check if source ID exists in registry
	 * @param SourceID - Row name to check
	 * @return True if registered
	 */
	UFUNCTION(BlueprintPure, Category = "Loot|Registry")
	bool IsSourceRegistered(FName SourceID) const;

	/**
	 * Get source entry from registry
	 * @param SourceID - Row name
	 * @param OutEntry - Output source entry
	 * @return True if found
	 */
	UFUNCTION(BlueprintPure, Category = "Loot|Registry")
	bool GetSourceEntry(FName SourceID, FLootSourceEntry& OutEntry) const;

	/**
	 * Get all registered source IDs
	 * @return Array of source IDs
	 */
	UFUNCTION(BlueprintPure, Category = "Loot|Registry")
	TArray<FName> GetAllSourceIDs() const;

	/**
	 * Get source IDs by category
	 * @param Category - Category to filter
	 * @return Matching source IDs
	 */
	UFUNCTION(BlueprintPure, Category = "Loot|Registry")
	TArray<FName> GetSourceIDsByCategory(ELootSourceType Category) const;

	/**
	 * Get source IDs by tag
	 * @param Tag - Tag to filter
	 * @return Matching source IDs
	 */
	UFUNCTION(BlueprintPure, Category = "Loot|Registry")
	TArray<FName> GetSourceIDsByTag(FName Tag) const;

	// ═══════════════════════════════════════════════
	// LOOT TABLE MANAGEMENT
	// ═══════════════════════════════════════════════

	/**
	 * Preload loot table for source (async-friendly)
	 * @param SourceID - Source to preload
	 */
	UFUNCTION(BlueprintCallable, Category = "Loot|Tables")
	void PreloadLootTable(FName SourceID);

	/**
	 * Preload multiple loot tables
	 * @param SourceIDs - Sources to preload
	 */
	UFUNCTION(BlueprintCallable, Category = "Loot|Tables")
	void PreloadLootTables(const TArray<FName>& SourceIDs);

	/**
	 * Unload cached loot table
	 * @param SourceID - Source to unload
	 */
	UFUNCTION(BlueprintCallable, Category = "Loot|Tables")
	void UnloadLootTable(FName SourceID);

	/**
	 * Clear all cached loot tables
	 */
	UFUNCTION(BlueprintCallable, Category = "Loot|Tables")
	void ClearLootTableCache();

	/**
	 * Check if loot table is loaded
	 * @param SourceID - Source to check
	 * @return True if loaded
	 */
	UFUNCTION(BlueprintPure, Category = "Loot|Tables")
	bool IsLootTableLoaded(FName SourceID) const;

	// ═══════════════════════════════════════════════
	// CORRUPTION QUERIES
	// ═══════════════════════════════════════════════

	/**
	 * Generate only corrupted loot from source
	 * @param Request - Generation request
	 * @return Only corrupted items
	 */
	UFUNCTION(BlueprintCallable, Category = "Loot|Corruption")
	FLootResultBatch GenerateCorruptedLoot(const FLootRequest& Request);

	/**
	 * Generate loot filtered by corruption type
	 * @param Request - Generation request
	 * @param CorruptionType - Filter type
	 * @return Filtered results
	 */
	UFUNCTION(BlueprintCallable, Category = "Loot|Corruption")
	FLootResultBatch GenerateLootByCorruptionType(
		const FLootRequest& Request,
		ECorruptionType CorruptionType);

	// ═══════════════════════════════════════════════
	// UTILITY
	// ═══════════════════════════════════════════════

	/**
	 * Apply player modifiers to drop settings
	 * @param BaseSettings - Base settings
	 * @param Luck - Player luck
	 * @param MagicFind - Player magic find
	 * @return Modified settings
	 */
	UFUNCTION(BlueprintPure, Category = "Loot|Utility")
	FLootDropSettings ApplyPlayerModifiers(
		const FLootDropSettings& BaseSettings,
		float Luck,
		float MagicFind) const;

	/**
	 * Set the registry DataTable path
	 * @param RegistryPath - Path to registry DataTable
	 */
	UFUNCTION(BlueprintCallable, Category = "Loot|Config")
	void SetRegistryPath(TSoftObjectPtr<UDataTable> RegistryPath);

	/**
	 * Reload the registry (after path change)
	 */
	UFUNCTION(BlueprintCallable, Category = "Loot|Config")
	void ReloadRegistry();

	// ═══════════════════════════════════════════════
	// DEBUG
	// ═══════════════════════════════════════════════

#if WITH_EDITOR
	/** Debug: Print all registered sources */
	UFUNCTION(BlueprintCallable, Category = "Loot|Debug")
	void DebugPrintRegistry() const;

	/** Debug: Validate all loot table references */
	UFUNCTION(BlueprintCallable, Category = "Loot|Debug")
	void DebugValidateTables() const;
#endif

protected:
	// ═══════════════════════════════════════════════
	// INTERNAL
	// ═══════════════════════════════════════════════

	/** Loot generator (lightweight struct) */
	FLootGenerator LootGenerator;

	/** Cached registry DataTable */
	UPROPERTY()
	UDataTable* CachedRegistry = nullptr;

	/** Cached loot tables (SourceID → Loaded DataTable) */
	UPROPERTY()
	TMap<FName, UDataTable*> CachedLootTables;

	/** Cached GroundItemSubsystem */
	UPROPERTY()
	UGroundItemSubsystem* CachedGroundItemSubsystem = nullptr;

	// ═══════════════════════════════════════════════
	// INTERNAL HELPERS
	// ═══════════════════════════════════════════════

	/** Load and cache registry */
	void LoadRegistry();

	/** Get or load loot table for source */
	UDataTable* GetOrLoadLootTable(FName SourceID);

	/** Get loot table from source entry */
	const FLootTable* GetLootTableFromSource(const FLootSourceEntry& Source, FName RowName = NAME_None);

	/** Build final settings from source + request */
	FLootDropSettings BuildFinalSettings(
		const FLootSourceEntry& Source,
		const FLootRequest& Request) const;

	/** Apply global modifiers */
	FLootDropSettings ApplyGlobalModifiers(const FLootDropSettings& Settings) const;

	/** Ensure GroundItemSubsystem is cached */
	bool EnsureGroundItemSubsystem();
};