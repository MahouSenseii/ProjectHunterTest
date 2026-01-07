// Character/Component/GroundItemPickupManager.h
#pragma once

#include "CoreMinimal.h"
#include "Item/Library/ItemEnums.h"

// Forward declarations
class UItemInstance;
class UInventoryManager;
class UEquipmentManager;
class UGroundItemSubsystem;
class AActor;
class UWorld;
DECLARE_LOG_CATEGORY_EXTERN(LogGroundItemPickupManager, Log, All);
/**
 * Manages the logic for picking up ground items in the game.
 */
class PROJECTHUNTERTEST_API FGroundItemPickupManager
{
public:
	FGroundItemPickupManager();
	~FGroundItemPickupManager() = default;

	// ═══════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════

	float PickupRadius;
	float HoldToEquipDuration;
	bool bShowEquipHint;

	// ═══════════════════════════════════════════════
	// INITIALIZATION
	// ═══════════════════════════════════════════════

	void Initialize(AActor* Owner, UWorld* World);

	// ═══════════════════════════════════════════════
	// PRIMARY FUNCTIONS
	// ═══════════════════════════════════════════════

	/**
	 * Pickup ground item to inventory (tap action)
	 */
	bool PickupToInventory(int32 ItemID);

	/**
	 * Pickup and equip ground item directly (hold action)
	 */
	bool PickupAndEquip(int32 ItemID);

	/**
	 * Pickup all ground items in radius
	 */
	int32 PickupAllNearby(FVector Location);

	/**
	 * Start hold interaction for ground item
	 */
	void StartHoldInteraction(int32 ItemID);

	/**
	 * Update hold progress (called by InteractionManager)
	 * @param DeltaTime - Time since last update
	 * @return True if hold completed, false if still in progress
	 */
	bool UpdateHoldProgress(float DeltaTime);

	/**
	 * Cancel hold interaction
	 */
	void CancelHoldInteraction();

	/**
	 * Check if currently holding for ground item
	 */
	bool IsHoldingForGroundItem() const { return bIsHoldingForGroundItem; }

	/**
	 * Get current hold progress [0-1]
	 */
	float GetHoldProgress() const { return HoldProgress; }

	/**
	 * Get current hold item ID
	 */
	int32 GetCurrentHoldItemID() const { return CurrentHoldItemID; }

private:
	// ═══════════════════════════════════════════════
	// CACHED REFERENCES
	// ═══════════════════════════════════════════════

	AActor* OwnerActor;
	UWorld* WorldContext;
	UInventoryManager* CachedInventoryManager;
	UEquipmentManager* CachedEquipmentManager;
	UGroundItemSubsystem* CachedGroundItemSubsystem;

	// ═══════════════════════════════════════════════
	// HOLD STATE
	// ═══════════════════════════════════════════════

	bool bIsHoldingForGroundItem;
	int32 CurrentHoldItemID;
	float HoldElapsedTime;
	float HoldProgress;

	// ═══════════════════════════════════════════════
	// INTERNAL LOGIC
	// ═══════════════════════════════════════════════

	void CacheComponents();
	bool PickupToInventoryInternal(int32 ItemID, FVector ClientLocation);
	bool PickupAndEquipInternal(int32 ItemID, FVector ClientLocation);
	EEquipmentSlot DetermineEquipmentSlot(UItemInstance* Item) const;
};