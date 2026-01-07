// World/Subsystem/GroundItemSubsystem.h
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GroundItemSubsystem.generated.h"

class UItemInstance;
class AISMContainerActor;

UCLASS()
class PROJECTHUNTERTEST_API UGroundItemSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ═══════════════════════════════════════════════
	// PUBLIC ACCESSORS
	// ═══════════════════════════════════════════════
	
	/** Get mapping of ItemID -> ISM instance index */
	const TMap<int32, int32>& GetItemToInstanceIndex() const { return ItemToInstanceIndex; }
	
	/** Get mapping of ItemID -> ISM component */
	const TMap<int32, UInstancedStaticMeshComponent*>& GetInstanceToComponent() const { return InstanceToComponent; }
	
	/** Get mapping of ItemID -> ItemInstance */
	const TMap<int32, UItemInstance*>& GetGroundItems() const { return GroundItems; }
	
	/** Get mapping of ItemID -> World location (IMPORTANT: Used by InteractionManager for validation) */
	const TMap<int32, FVector>& GetInstanceLocations() const { return InstanceLocations; }

	// ═══════════════════════════════════════════════
	// ITEM MANAGEMENT
	// ═══════════════════════════════════════════════

	/** Add item to ground, returns unique ItemID or -1 on failure */
	UFUNCTION(BlueprintCallable, Category = "World|Items")
	int32 AddItemToGround(UItemInstance* Item, FVector Location, FRotator Rotation = FRotator::ZeroRotator);

	/** Remove item from ground, returns the ItemInstance or nullptr */
	UFUNCTION(BlueprintCallable, Category = "World|Items")
	UItemInstance* RemoveItemFromGround(int32 ItemID);

	/** Get nearest item to location within MaxDistance, returns item and sets OutItemID */
	UFUNCTION(BlueprintCallable, Category = "World|Items")
	UItemInstance* GetNearestItem(FVector Location, float MaxDistance, int32& OutItemID);

	/** Get all items within radius */
	UFUNCTION(BlueprintCallable, Category = "World|Items")
	TArray<UItemInstance*> GetItemsInRadius(FVector Location, float Radius);

	/** Get item ID */
	UFUNCTION(BlueprintPure, Category = "Ground Items")
	int32 GetInstanceID(UItemInstance* Item) const;

	/** Update visual location of item (doesn't affect item data) */
	UFUNCTION(BlueprintCallable, Category = "World|Items")
	void UpdateItemLocation(int32 ItemID, FVector NewLocation);

	/** Remove all items from ground */
	UFUNCTION(BlueprintCallable, Category = "World|Items")
	void ClearAllItems();

	// ═══════════════════════════════════════════════
	// DEBUG
	// ═══════════════════════════════════════════════

	/** Get total number of items on ground */
	UFUNCTION(BlueprintPure, Category = "World|Items|Debug")
	int32 GetTotalItemCount() const { return GroundItems.Num(); }

	/** Draw debug visualization of all ground items */
	UFUNCTION(BlueprintCallable, Category = "World|Items|Debug")
	void DebugDrawAllItems(float Duration = 5.0f);

private:
	// ═══════════════════════════════════════════════
	// INTERNAL DATA
	// ═══════════════════════════════════════════════
	
	/** Mesh -> ISM component mapping (one ISM per mesh type for batching) */
	UPROPERTY()
	TMap<UStaticMesh*, UInstancedStaticMeshComponent*> MeshComponents;

	/** ItemID -> ItemInstance mapping */
	UPROPERTY()
	TMap<int32, UItemInstance*> GroundItems;

	/** ItemID -> ISM component mapping */
	UPROPERTY()
	TMap<int32, UInstancedStaticMeshComponent*> InstanceToComponent;

	/** ItemID -> World location mapping */
	UPROPERTY()
	TMap<int32, FVector> InstanceLocations;

	/** ItemID -> ISM instance index mapping */
	UPROPERTY()
	TMap<int32, int32> ItemToInstanceIndex;

	/** Next available ItemID (auto-increments) */
	int32 NextItemID = 0;

	/** Container actor that owns all ISM components */
	UPROPERTY()
	AISMContainerActor* ISMContainerActor = nullptr;

	// ═══════════════════════════════════════════════
	// INTERNAL HELPERS
	// ═══════════════════════════════════════════════

	/** Lazy-create container actor on first use */
	void EnsureISMContainerExists();

	/** Get or create ISM component for mesh type */
	UInstancedStaticMeshComponent* GetOrCreateISMComponent(UStaticMesh* Mesh);
};