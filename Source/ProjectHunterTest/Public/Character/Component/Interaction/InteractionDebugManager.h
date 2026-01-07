// Character/Component/InteractionDebugManager.h
#pragma once

#include "CoreMinimal.h"
#include "Character/Component/Library/InteractionDebugEnumLibrary.h"

// Forward declarations
class UInteractableManager;
class AActor;
class UWorld;
class UALSDebugComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogInteractionDebugManager, Log, All);

/**
 * A manager class for handling interaction debugging functionality.
 * Manages the display and control of debug data to aid in development and testing.
 */
class PROJECTHUNTERTEST_API FInteractionDebugManager
{
public:
	FInteractionDebugManager();
	~FInteractionDebugManager() = default;

	// ═══════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════

	EInteractionDebugMode DebugMode;
	bool bDrawTraceLines;
	bool bDrawHitPoints;
	bool bDrawInteractionRange;
	bool bDrawGroundItems;
	bool bShowDebugText;

	FColor TraceHitColor;
	FColor TraceMissColor;
	FColor InteractableColor;
	FColor GroundItemColor;

	float DrawDuration; // 0 = single frame
	float DrawThickness;

	// ═══════════════════════════════════════════════
	// INITIALIZATION
	// ═══════════════════════════════════════════════

	void Initialize(AActor* Owner, UWorld* World);

	// ═══════════════════════════════════════════════
	// DEBUG DRAWING
	// ═══════════════════════════════════════════════

	void DrawTraceLine(FVector Start, FVector End, bool bHit);
	void DrawHitPoint(FVector HitLocation, FVector HitNormal);
	void DrawInteractionRange(FVector Center, float Radius);
	void DrawGroundItem(FVector ItemLocation, int32 ItemID);
	void DrawInteractableInfo(UInteractableManager* Interactable, float Distance);

	// ═══════════════════════════════════════════════
	// DEBUG TEXT
	// ═══════════════════════════════════════════════

	void DisplayInteractionState(UInteractableManager* Interactable, float Distance, int32 GroundItemID);
	void DisplayPerformanceMetrics(float TraceTime, float ValidationTime);

	// ═══════════════════════════════════════════════
	// LOGGING
	// ═══════════════════════════════════════════════

	void LogInteraction(UInteractableManager* Interactable, bool bSuccess, const FString& Reason = "");
	void LogGroundItemPickup(int32 ItemID, bool bToInventory, bool bSuccess);
	void LogValidationFailure(const FString& ValidationReason, float Distance, float MaxDistance);

	// ═══════════════════════════════════════════════
	// STATS
	// ═══════════════════════════════════════════════

	void PrintDebugStats();

	// ═══════════════════════════════════════════════
	// ALS DEBUG INTEGRATION
	// ═══════════════════════════════════════════════

	/**
	 * Check if debug traces should be shown
	 * Integrates with ALS Debug component if available
	 */
	bool ShouldShowDebugTraces() const;

private:
	AActor* OwnerActor;
	UWorld* WorldContext;
	UALSDebugComponent* CachedALSDebugComponent;

	// Debug statistics
	int32 TotalInteractions;
	int32 SuccessfulInteractions;
	int32 FailedInteractions;
	int32 TotalGroundItemsPickedUp;
	float AverageTraceTime;
	float AverageValidationTime;
};