// Tower/Subsystem/GroundItemSubsystem.h
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GroundItemSubsystem.generated.h"

// Forward declarations
class UItemInstance;
class AISMContainerActor;
class UInstancedStaticMeshComponent;
class UStaticMesh;

/**
 * Struct to track ISM instance data
 * FIX: Stores all necessary info to handle index invalidation
 */
USTRUCT()
struct FGroundItemISMData
{
	GENERATED_BODY()

	/** The ISM component containing this instance */
	UPROPERTY()
	UInstancedStaticMeshComponent* ISMComponent = nullptr;

	/** Current instance index within the ISM (can change on removal!) */
	int32 InstanceIndex = INDEX_NONE;

	/** The mesh used (for lookup) */
	UPROPERTY()
	UStaticMesh* Mesh = nullptr;

	FGroundItemISMData() = default;

	FGroundItemISMData(UInstancedStaticMeshComponent* InISM, int32 InIndex, UStaticMesh* InMesh)
		: ISMComponent(InISM)
		, InstanceIndex(InIndex)
		, Mesh(InMesh)
	{}

	bool IsValid() const { return ISMComponent != nullptr && InstanceIndex != INDEX_NONE; }
};

/**
 * UGroundItemSubsystem - Manages items on the ground using Instanced Static Meshes
 * 
 * SINGLE RESPONSIBILITY: Ground item instance management and rendering
 * 
 * DESIGN:
 * - World Subsystem (single instance per world)
 * - Uses ISM for efficient rendering of many ground items
 * - Lazy container creation (not in Initialize)
 * - Server-authoritative item placement
 * 
 * FIX: ISM Index Invalidation
 * - When an ISM instance is removed, UE re-indexes remaining instances
 * - We now track and update indices after each removal
 * - Uses FGroundItemISMData struct instead of separate maps
 * 
 * USAGE:
 *   UGroundItemSubsystem* Sub = GetWorld()->GetSubsystem<UGroundItemSubsystem>();
 *   int32 ID = Sub->AddItemToGround(Item, Location);
 *   UItemInstance* Item = Sub->RemoveItemFromGround(ID);
 */
UCLASS()
class PROJECTHUNTERTEST_API UGroundItemSubsystem : public UWorldSubsystem
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
	// PRIMARY API
	// ═══════════════════════════════════════════════

	/**
	 * Add item to ground
	 * @param Item - Item instance to add
	 * @param Location - World location
	 * @param Rotation - World rotation
	 * @return Unique ground item ID (-1 on failure)
	 */
	UFUNCTION(BlueprintCallable, Category = "Ground Items")
	int32 AddItemToGround(UItemInstance* Item, FVector Location, FRotator Rotation = FRotator::ZeroRotator);

	/**
	 * Remove item from ground
	 * @param ItemID - Ground item ID
	 * @return Removed item instance (nullptr if not found)
	 */
	UFUNCTION(BlueprintCallable, Category = "Ground Items")
	UItemInstance* RemoveItemFromGround(int32 ItemID);

	/**
	 * Get item by ID
	 * @param ItemID - Ground item ID
	 * @return Item instance (nullptr if not found)
	 */
	UFUNCTION(BlueprintPure, Category = "Ground Items")
	UItemInstance* GetItemByID(int32 ItemID) const;

	/**
	 * Get nearest item to location
	 * @param Location - Search center
	 * @param MaxDistance - Maximum search radius
	 * @param OutItemID - Output item ID
	 * @return Nearest item (nullptr if none in range)
	 */
	UFUNCTION(BlueprintPure, Category = "Ground Items")
	UItemInstance* GetNearestItem(FVector Location, float MaxDistance, int32& OutItemID);

	/**
	 * Get all items in radius
	 * @param Location - Search center
	 * @param Radius - Search radius
	 * @param OutItemIDs - Output item IDs
	 * @return Number of items found
	 */
	UFUNCTION(BlueprintCallable, Category = "Ground Items")
	int32 GetItemsInRadius(FVector Location, float Radius, TArray<int32>& OutItemIDs);

	/**
	 * Get all item instances in radius
	 * @param Location - Search center
	 * @param Radius - Search radius
	 * @return Array of item instances
	 */
	UFUNCTION(BlueprintCallable, Category = "Ground Items")
	TArray<UItemInstance*> GetItemInstancesInRadius(FVector Location, float Radius);

	/**
	 * Clear all ground items
	 */
	UFUNCTION(BlueprintCallable, Category = "Ground Items")
	void ClearAllItems();

	// ═══════════════════════════════════════════════
	// QUERIES
	// ═══════════════════════════════════════════════

	UFUNCTION(BlueprintPure, Category = "Ground Items")
	int32 GetTotalItemCount() const { return GroundItems.Num(); }

	UFUNCTION(BlueprintPure, Category = "Ground Items")
	bool IsValidItemID(int32 ItemID) const { return GroundItems.Contains(ItemID); }

	/**
	 * Get item ID from item instance (reverse lookup)
	 * @param Item - Item to find
	 * @return Item ID or -1 if not found
	 */
	UFUNCTION(BlueprintPure, Category = "Ground Items")
	int32 GetInstanceID(UItemInstance* Item) const;

	/**
	 * Update visual location of item
	 * @param ItemID - Ground item ID
	 * @param NewLocation - New world location
	 */
	UFUNCTION(BlueprintCallable, Category = "Ground Items")
	void UpdateItemLocation(int32 ItemID, FVector NewLocation);

	/**
	 * Get all instance locations (for debug/visualization)
	 */
	const TMap<int32, FVector>& GetInstanceLocations() const { return InstanceLocations; }

	/**
	 * Get all ground items map
	 */
	const TMap<int32, UItemInstance*>& GetGroundItems() const { return GroundItems; }

	// ═══════════════════════════════════════════════
	// DEBUG
	// ═══════════════════════════════════════════════

#if WITH_EDITOR
	/** Draw debug visualization of all ground items */
	UFUNCTION(BlueprintCallable, Category = "Ground Items|Debug")
	void DebugDrawAllItems(float Duration = 5.0f);
#endif

protected:
	// ═══════════════════════════════════════════════
	// ISM MANAGEMENT
	// ═══════════════════════════════════════════════

	/** Ensure ISM container actor exists */
	void EnsureISMContainerExists();

	/** Get or create ISM component for a mesh type */
	UInstancedStaticMeshComponent* GetOrCreateISMComponent(UStaticMesh* Mesh);

	/**
	 * FIX: Re-index ISM instances after removal
	 * When RemoveInstance is called, UE shifts all higher indices down by 1
	 * This function updates our tracking to match
	 * 
	 * @param ISMComponent - The ISM that had an instance removed
	 * @param RemovedIndex - The index that was removed
	 */
	void ReindexAfterRemoval(UInstancedStaticMeshComponent* ISMComponent, int32 RemovedIndex);

	// ═══════════════════════════════════════════════
	// DATA
	// ═══════════════════════════════════════════════

	/** ISM container actor */
	UPROPERTY()
	AISMContainerActor* ISMContainerActor;

	/** Map: ItemID → ItemInstance */
	UPROPERTY()
	TMap<int32, UItemInstance*> GroundItems;

	/** Map: ItemID → Location */
	UPROPERTY()
	TMap<int32, FVector> InstanceLocations;

	/** 
	 * FIX: Map: ItemID → ISM tracking data
	 * Replaces separate InstanceToComponent and ItemToInstanceIndex maps
	 * Single source of truth for ISM instance tracking
	 */
	UPROPERTY()
	TMap<int32, FGroundItemISMData> ItemISMData;

	/** Map: Mesh → ISM Component (for fast lookup) */
	UPROPERTY()
	TMap<UStaticMesh*, UInstancedStaticMeshComponent*> MeshToISM;

	/** Next item ID to assign */
	int32 NextItemID = 0;
};