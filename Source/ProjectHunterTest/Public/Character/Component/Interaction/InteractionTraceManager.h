// Character/Component/InteractionTraceManager.h
#pragma once

#include "CoreMinimal.h"
#include "Interactable/Library/InteractionEnumLibrary.h"

// Forward declarations
class IInteractable;
class UInteractableManager;
class UGroundItemSubsystem;
class UItemInstance;
class APlayerController;
class AALSPlayerCameraManager;
class UALSDebugComponent;
class AActor;
class UWorld;
DECLARE_LOG_CATEGORY_EXTERN(LogInteractionTraceManager, Log, All);
/**
 * 
 */
class PROJECTHUNTERTEST_API FInteractionTraceManager
{
public:
	FInteractionTraceManager();
	~FInteractionTraceManager() = default;

	// ═══════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════

	/** Maximum interaction distance */
	float InteractionDistance = 300.0f;

	/** How often to check for interactables (per second) */
	float CheckFrequency = 0.1f;

	/** Trace channel for actor detection */
	TEnumAsByte<ECollisionChannel> InteractionTraceChannel = ECC_Visibility;

	/** Use ALS camera target location for trace origin */
	bool bUseALSCameraOrigin = true;

	/** Forward offset from camera pivot (X) */
	float OffsetForward = 0.0f;

	/** Right offset from camera pivot (Y) */
	float OffsetRight = 0.0f;

	/** Up offset from camera pivot (Z) */
	float OffsetUp = 60.0f;

	// ═══════════════════════════════════════════════
	// INITIALIZATION
	// ═══════════════════════════════════════════════

	/**
	 * Initialize trace manager
	 * @param Owner - Owner actor (character)
	 * @param World - World context
	 */
	void Initialize(AActor* Owner, UWorld* World);

	// ═══════════════════════════════════════════════
	// PRIMARY FUNCTIONS
	// ═══════════════════════════════════════════════

	/**
	 * Trace for actor-based interactables (doors, chests, NPCs)
	 * @return Found interactable, or invalid interface if none found
	 */
	TScriptInterface<IInteractable> TraceForActorInteractable();

	/**
	 * Find nearest ground item within range
	 * @param OutItemID - ID of found item (-1 if none)
	 * @return Item instance if found, nullptr otherwise
	 */
	UItemInstance* FindNearestGroundItem(int32& OutItemID);

	/**
	 * Get camera view point (location + rotation)
	 * @param OutLocation - Camera world location
	 * @param OutRotation - Camera rotation
	 * @return True if successful
	 */
	bool GetCameraViewPoint(FVector& OutLocation, FRotator& OutRotation) const;

	/**
	 * Get trace start location (with ALS offsets applied)
	 */
	FVector GetTraceStartLocation(const FVector& CameraLocation, const FRotator& CameraRotation) const;

	/**
	 * Get trace end location
	 */
	FVector GetTraceEndLocation(const FVector& CameraLocation, const FRotator& CameraRotation) const;

	/**
	 * Get last successful trace result (for debug/validation)
	 */
	const FHitResult& GetLastTraceResult() const { return LastTraceResult; }

	/**
	 * Check if owner is locally controlled
	 */
	bool IsLocallyControlled() const;

private:
	// ═══════════════════════════════════════════════
	// CACHED REFERENCES
	// ═══════════════════════════════════════════════

	AActor* OwnerActor = nullptr;
	UWorld* WorldContext = nullptr;
	APlayerController* CachedPlayerController = nullptr;
	AALSPlayerCameraManager* CachedALSCameraManager = nullptr;
	UGroundItemSubsystem* CachedGroundItemSubsystem = nullptr;
	
	FHitResult LastTraceResult;

	// ═══════════════════════════════════════════════
	// INTERNAL HELPERS
	// ═══════════════════════════════════════════════

	void CacheComponents();
	bool PerformLineTrace(const FVector& Start, const FVector& End, FHitResult& OutHit);
	bool IsActorInteractable(AActor* Actor) const;
};