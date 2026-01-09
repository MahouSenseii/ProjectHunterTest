// Character/Component/GroundItemPickupManager.h
#pragma once

#include "CoreMinimal.h"
#include "Item/Library/ItemEnums.h"
#include "GroundItemPickupManager.generated.h"

class UGroundItemSubsystem;
class UItemInstance;
class UInventoryManager;
class UEquipmentManager;
class AActor;
class UWorld;

DECLARE_LOG_CATEGORY_EXTERN(LogGroundItemPickupManager, Log, All);

/**
 * Manages the logic for picking up ground items in the game.
 */
USTRUCT(BlueprintType)
struct PROJECTHUNTERTEST_API FGroundItemPickupManager
{
	GENERATED_BODY()

public:
	FGroundItemPickupManager();

	// ═══════════════════════════════════════════════
	// CONFIGURATION (Blueprint-editable)
	// ═══════════════════════════════════════════════

	/** Maximum radius to pickup items */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	float PickupRadius = 500.0f;

	/** How long to hold button to equip instead of pickup */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	float HoldToEquipDuration = 0.5f;

	/** Show hint that holding will equip */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	bool bShowEquipHint = true;

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
	// CACHED REFERENCES (Not Blueprint-exposed)
	// ═══════════════════════════════════════════════

	AActor* OwnerActor = nullptr;
	UWorld* WorldContext = nullptr;
	UInventoryManager* CachedInventoryManager = nullptr;
	UEquipmentManager* CachedEquipmentManager = nullptr;
	UGroundItemSubsystem* CachedGroundItemSubsystem = nullptr;

	// ═══════════════════════════════════════════════
	// HOLD STATE (Not Blueprint-exposed)
	// ═══════════════════════════════════════════════

	bool bIsHoldingForGroundItem = false;
	int32 CurrentHoldItemID = -1;
	float HoldElapsedTime = 0.0f;
	float HoldProgress = 0.0f;

	// ═══════════════════════════════════════════════
	// INTERNAL LOGIC
	// ═══════════════════════════════════════════════

	void CacheComponents();
	bool PickupToInventoryInternal(int32 ItemID, FVector ClientLocation);
	bool PickupAndEquipInternal(int32 ItemID, FVector ClientLocation);
	EEquipmentSlot DetermineEquipmentSlot(UItemInstance* Item) const;
};