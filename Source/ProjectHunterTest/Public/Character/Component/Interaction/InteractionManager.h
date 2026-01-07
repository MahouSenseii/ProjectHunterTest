// Character/Component/InteractionManager.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Character/Component/Interaction/InteractionTraceManager.h"
#include "Character/Component/Interaction/InteractionValidatorManager.h"
#include "Character/Component/Interaction/GroundItemPickupManager.h"
#include "Character/Component/Interaction/InteractionDebugManager.h"
#include "Character/Component/Library/InteractionDebugEnumLibrary.h"
#include "Interactable/Library/InteractionEnumLibrary.h"
#include "InteractionManager.generated.h"

// Forward declarations
class IInteractable;
class UInteractableManager;
DECLARE_LOG_CATEGORY_EXTERN(LogInteractionManager, Log, All);
/**
 * Delegates
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCurrentInteractableChanged, UInteractableManager*, NewInteractable);

/**
 * Interaction Manager (Lightweight Coordinator)
 * 
 * ONLY UACTORCOMPONENT IN THE SYSTEM!
 * 
 * SINGLE RESPONSIBILITY: Coordinate lightweight interaction managers
 * - Owns all sub-managers (as member variables, NOT components!)
 * - Manages current interactable state
 * - Routes input to appropriate handlers
 * - Provides unified interface
 * 
 * SUB-MANAGERS (Plain C++ classes):
 * - FInteractionTraceManager (~80 bytes)
 * - FInteractionValidatorManager (~64 bytes)
 * - FGroundItemPickupManager (~72 bytes)
 * - FInteractionDebugManager (~96 bytes)
 * 
 * TOTAL OVERHEAD: ~312 bytes for all managers
 *   vs OLD: ~800 bytes for single monolithic component
 *   vs PREVIOUS REFACTOR: ~1200 bytes (5 separate components)
 * 
 * PERFORMANCE: ✅ Best cache locality, minimal indirection
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECTHUNTERTEST_API UInteractionManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UInteractionManager();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "Interaction|Setup")
	void Initialize();
	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ═══════════════════════════════════════════════
	// CONFIGURATION (Exposed to Blueprint)
	// ═══════════════════════════════════════════════

	/** Enable interaction system */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Setup")
	bool bInteractionEnabled = true;

	// Trace settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Trace")
	float InteractionDistance = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Trace")
	float CheckFrequency = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Trace")
	bool bUseALSCameraOrigin = true;

	// Validation settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Validation")
	float LatencyBuffer = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Validation")
	bool bUseDynamicLatencyBuffer = true;

	// Pickup settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Pickup")
	float PickupRadius = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Pickup")
	float HoldToEquipDuration = 0.5f;

	// Debug settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Debug")
	bool bDebugEnabled = false;

	// ═══════════════════════════════════════════════
	// CURRENT STATE
	// ═══════════════════════════════════════════════

	/** Current interactable being looked at (LOCAL ONLY) */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction|State")
	TScriptInterface<IInteractable> CurrentInteractable;

	/** Current ground item ID (-1 if none) */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction|State")
	int32 CurrentGroundItemID;

	/** Called when CurrentInteractable changes */
	UPROPERTY(BlueprintAssignable, Category = "Interaction|Events")
	FOnCurrentInteractableChanged OnCurrentInteractableChanged;

	// ═══════════════════════════════════════════════
	// PRIMARY INTERFACE (Client-side)
	// ═══════════════════════════════════════════════

	/**
	 * Called when interact button is pressed
	 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void OnInteractPressed();

	/**
	 * Called when interact button is released
	 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void OnInteractReleased();

	/**
	 * Pickup all items in radius
	 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void PickupAllNearbyItems();

	/**
	 * Check for interactables (called by timer)
	 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void CheckForInteractables();

	// ═══════════════════════════════════════════════
	// GETTERS
	// ═══════════════════════════════════════════════

	UFUNCTION(BlueprintPure, Category = "Interaction")
	UInteractableManager* GetCurrentInteractable() const;

	UFUNCTION(BlueprintPure, Category = "Interaction")
	TScriptInterface<IInteractable> GetCurrentInteractableInterface() const { return CurrentInteractable; }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	int32 GetCurrentGroundItemID() const { return CurrentGroundItemID; }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsLocallyControlled() const { return TraceManager.IsLocallyControlled(); }

	// ═══════════════════════════════════════════════
	// DEBUG COMMANDS (Blueprint callable)
	// ═══════════════════════════════════════════════

	UFUNCTION(BlueprintCallable, Category = "Interaction|Debug")
	void PrintDebugStats() { DebugManager.PrintDebugStats(); }

protected:
	// ═══════════════════════════════════════════════
	// SUB-MANAGERS (Lightweight, stack-allocated!)
	// ═══════════════════════════════════════════════

	FInteractionTraceManager TraceManager;
	FInteractionValidatorManager ValidatorManager;
	FGroundItemPickupManager PickupManager;
	FInteractionDebugManager DebugManager;
	
	// ═══════════════════════════════════════════════
	// INITIALIZATION
	// ═══════════════════════════════════════════════

	/**
	 * Initialize the interaction system (called after possession)
	 */
	void InitializeInteractionSystem();

	/**
	 * Check if possessed and initialize if ready
	 */
	void CheckPossessionAndInitialize();


	// ═══════════════════════════════════════════════
	// INTERNAL LOGIC
	// ═══════════════════════════════════════════════

	void InitializeSubManagers();
	void ApplyConfigurationToManagers();
	void InteractWithActor(AActor* TargetActor);
	void PickupGroundItemToInventory(int32 ItemID);
	void PickupGroundItemAndEquip(int32 ItemID);
	void UpdateFocusState(TScriptInterface<IInteractable> NewInteractable);

	/** Timer callback for hold progress updates */
	void UpdateHoldProgress();

private:
	FTimerHandle InteractionCheckTimer;
	FTimerHandle HoldProgressTimer;
	FTimerHandle PossessionCheckTimer;
};