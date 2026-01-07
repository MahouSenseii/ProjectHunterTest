// Character/Component/InteractionValidatorManager.h
#pragma once

#include "CoreMinimal.h"

// Forward declarations
class IInteractable;
class UInteractableManager;
class UGroundItemSubsystem;
class AActor;
class UWorld;
DECLARE_LOG_CATEGORY_EXTERN(LogInteractionValidatorManager, Log, All);
/**
 * Manages the lifecycle and validation of interaction-related objects.
 * Responsible for ensuring interactions adhere to specific rules
 * and constraints across different contexts.
 */
class PROJECTHUNTERTEST_API FInteractionValidatorManager
{
public:
	FInteractionValidatorManager();
	~FInteractionValidatorManager() = default;

	// ═══════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════

	/** Additional distance buffer for network latency/lag compensation */
	float LatencyBuffer;

	/** Use dynamic latency buffer based on player ping */
	bool bUseDynamicLatencyBuffer;

	/** Minimum latency buffer (ms to units) */
	float MinLatencyBuffer;

	/** Maximum latency buffer (ms to units) */
	float MaxLatencyBuffer;

	/** Require line of sight for actor interactions */
	bool bRequireLineOfSight;

	/** Log validation failures for anti-cheat monitoring */
	bool bLogValidationFailures;

	// ═══════════════════════════════════════════════
	// INITIALIZATION
	// ═══════════════════════════════════════════════

	/**
	 * Initialize validator manager
	 * @param Owner - Owner actor (character)
	 * @param World - World context
	 */
	void Initialize(AActor* Owner, UWorld* World);

	// ═══════════════════════════════════════════════
	// VALIDATION FUNCTIONS (Server-side)
	// ═══════════════════════════════════════════════

	/**
	 * Validate actor interaction request
	 */
	bool ValidateActorInteraction(AActor* TargetActor, FVector ClientLocation, float MaxDistance);

	/**
	 * Validate ground item pickup request
	 */
	bool ValidateGroundItemPickup(int32 ItemID, FVector ClientLocation, float MaxDistance);

	/**
	 * Check if actor can be interacted with
	 */
	bool IsValidInteractable(AActor* Actor, AActor* Interactor) const;

	/**
	 * Validate distance between two points
	 */
	bool ValidateDistance(FVector LocationA, FVector LocationB, float MaxDistance, bool bUseLatencyBuffer = true);

	/**
	 * Check line of sight between two points
	 */
	bool HasLineOfSight(FVector Start, FVector End, AActor* IgnoreActor = nullptr);

	/**
	 * Calculate dynamic latency buffer based on player ping
	 */
	float GetDynamicLatencyBuffer() const;

	/**
	 * Get player ping in milliseconds
	 */
	float GetPlayerPing() const;

	/**
	 * Check if this has server authority
	 */
	bool HasAuthority() const;

	/**
	 * Log validation failure for monitoring
	 */
	void LogValidationFailure(const FString& Reason, FVector ClientLocation, FVector TargetLocation);

private:
	// ═══════════════════════════════════════════════
	// CACHED REFERENCES
	// ═══════════════════════════════════════════════

	AActor* OwnerActor;
	UWorld* WorldContext;
	UGroundItemSubsystem* CachedGroundItemSubsystem;

	// Anti-cheat tracking
	int32 ValidationFailureCount;
	float LastValidationFailureTime;

	// ═══════════════════════════════════════════════
	// INTERNAL HELPERS
	// ═══════════════════════════════════════════════

	void CacheComponents();
};