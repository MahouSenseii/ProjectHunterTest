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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLootGeneratedDelegate, const FLootResultBatch&, Results, FName, SourceID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnLootSpawnedDelegate, UItemInstance*, Item, FVector, Location, int32, GroundItemID);
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
 * API CLEANUP:
 * - Removed redundant GenerateLootByID() - use GenerateLoot() instead
 * - Primary entry point is GenerateLoot(FLootRequest)
 * - Helper functions for common patterns
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

	UPROPERTY(BlueprintAssignable, Category = "Loot|Events")
	FOnLootGeneratedDelegate OnLootGenerated;

	UPROPERTY(BlueprintAssignable, Category = "Loot|Events")
	FOnLootSpawnedDelegate OnLootSpawned;

	UPROPERTY(BlueprintAssignable, Category = "Loot|Events")
	FOnLootTableLoadedDelegate OnLootTableLoaded;

	// ═══════════════════════════════════════════════
	// PRIMARY API - GENERATION
	// ═══════════════════════════════════════════════

	/**
	 * PRIMARY ENTRY POINT: Generate loot from a registered source
	 * @param Request - Loot generation request with all parameters
	 * @return Generated loot batch
	 */
	UFUNCTION(BlueprintCallable, Category = "Loot|Generation")
	FLootResultBatch GenerateLoot(const FLootRequest& Request);

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
	// PRIMARY API - SPAWNING
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
	 */
	UFUNCTION(BlueprintPure, Category = "Loot|Registry")
	bool IsSourceRegistered(FName SourceID) const;

	/**
	 * Get source entry from registry
	 */
	UFUNCTION(BlueprintPure, Category = "Loot|Registry")
	bool GetSourceEntry(FName SourceID, FLootSourceEntry& OutEntry) const;

	/**
	 * Get all registered source IDs
	 */
	UFUNCTION(BlueprintPure, Category = "Loot|Registry")
	TArray<FName> GetAllSourceIDs() const;

	/**
	 * Get source IDs by category
	 */
	UFUNCTION(BlueprintPure, Category = "Loot|Registry")
	TArray<FName> GetSourceIDsByCategory(ELootSourceType Category) const;

	// ═══════════════════════════════════════════════
	// CACHE MANAGEMENT
	// ═══════════════════════════════════════════════

	/**
	 * Preload loot tables for sources (call during loading screen)
	 */
	UFUNCTION(BlueprintCallable, Category = "Loot|Cache")
	void PreloadLootTables(const TArray<FName>& SourceIDs);

	/**
	 * Clear all cached loot tables
	 */
	UFUNCTION(BlueprintCallable, Category = "Loot|Cache")
	void ClearLootTableCache();

	/**
	 * Get number of cached loot tables
	 */
	UFUNCTION(BlueprintPure, Category = "Loot|Cache")
	int32 GetCachedTableCount() const { return LootTableCache.Num(); }

protected:
	// ═══════════════════════════════════════════════
	// INTERNAL - REGISTRY
	// ═══════════════════════════════════════════════

	void LoadRegistry();
	
	// ═══════════════════════════════════════════════
	// INTERNAL - LOOT TABLE LOADING
	// ═══════════════════════════════════════════════

	const FLootTable* GetLootTableFromSource(const FLootSourceEntry& Source, FName RowName);
	bool LoadLootTableAsync(const FLootSourceEntry& Source);

	// ═══════════════════════════════════════════════
	// INTERNAL - SETTINGS BUILDING
	// ═══════════════════════════════════════════════

	FLootDropSettings BuildFinalSettings(const FLootSourceEntry& Source, const FLootRequest& Request) const;
	FLootDropSettings ApplyGlobalModifiers(const FLootDropSettings& Settings) const;
	FLootDropSettings ApplyPlayerModifiers(const FLootDropSettings& Settings, float Luck, float MagicFind) const;

	// ═══════════════════════════════════════════════
	// INTERNAL - SUBSYSTEM CACHING
	// ═══════════════════════════════════════════════

	bool EnsureGroundItemSubsystem();

	// ═══════════════════════════════════════════════
	// CACHED DATA
	// ═══════════════════════════════════════════════

	/** Cached registry DataTable */
	UPROPERTY()
	UDataTable* CachedRegistry;

	/** Cached loot tables (SourceID → DataTable) */
	UPROPERTY()
	TMap<FName, UDataTable*> LootTableCache;

	/** Cached GroundItemSubsystem reference */
	UPROPERTY()
	UGroundItemSubsystem* CachedGroundItemSubsystem;

	/** Cached world reference for subsystem lookups */
	UPROPERTY()
	UWorld* CachedWorld;

	/** Loot generator instance */
	FLootGenerator LootGenerator;
};

// ═══════════════════════════════════════════════════════════════════════
// INLINE HELPER - Quick loot generation
// ═══════════════════════════════════════════════════════════════════════

/**
 * Helper function for quick loot generation
 * Use when you don't need full FLootRequest control
 * 
 * Example:
 *   FLootResultBatch Loot = QuickGenerateLoot(GetWorld(), "Goblin_Basic", PlayerStats->GetLuck());
 */
FORCEINLINE FLootResultBatch QuickGenerateLoot(
	UWorld* World,
	FName SourceID,
	float PlayerLuck = 0.0f,
	float PlayerMagicFind = 0.0f,
	int32 Seed = 0)
{
	if (!World)
	{
		return FLootResultBatch();
	}

	ULootSubsystem* LootSub = World->GetSubsystem<ULootSubsystem>();
	if (!LootSub)
	{
		return FLootResultBatch();
	}

	FLootRequest Request(SourceID);
	Request.PlayerLuck = PlayerLuck;
	Request.PlayerMagicFind = PlayerMagicFind;
	Request.Seed = Seed;

	return LootSub->GenerateLoot(Request);
}