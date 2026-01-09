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
 * Represents a manager component for handling interaction systems, including
 * tracing, validation, pickup, debugging, and state tracking for nearby interactables.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECTHUNTERTEST_API UInteractionManager : public UActorComponent
{
	GENERATED_BODY()

public:
	// ═══════════════════════════════════════════════
	// LIFECYCLE
	// ═══════════════════════════════════════════════

	UInteractionManager();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "Interaction|Setup")
	void Initialize();
	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ═══════════════════════════════════════════════
	// CONFIGURATION - Sub-Managers (Blueprint-editable!)
	// ═══════════════════════════════════════════════

	/** Enable interaction system */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Setup")
	bool bInteractionEnabled = true;

	/** Trace Manager - Expand to configure trace settings */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Managers", meta = (ShowOnlyInnerProperties))
	FInteractionTraceManager TraceManager;

	/** Validator Manager - Expand to configure validation settings */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Managers", meta = (ShowOnlyInnerProperties))
	FInteractionValidatorManager ValidatorManager;

	/** Pickup Manager - Expand to configure pickup settings */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Managers", meta = (ShowOnlyInnerProperties))
	FGroundItemPickupManager PickupManager;

	/** Debug Manager - Expand to configure debug settings */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Managers", meta = (ShowOnlyInnerProperties))
	FInteractionDebugManager DebugManager;

	// ═══════════════════════════════════════════════
	// QUICK ACCESS SETTINGS (Optional convenience)
	// ═══════════════════════════════════════════════

	/** Quick toggle for debug visualization (also in DebugManager) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Quick Settings")
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

	/**
	 * Check if locally controlled (safe to call after initialization)
	 * 
	 * NOTE: This delegates to TraceManager, which must be initialized first.
	 *       In BeginPlay(), we check locally controlled status directly
	 *       on the Pawn before sub-managers are initialized.
	 */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsLocallyControlled() const { return TraceManager.IsLocallyControlled(); }

	// ═══════════════════════════════════════════════
	// DEBUG COMMANDS (Blueprint callable)
	// ═══════════════════════════════════════════════

	UFUNCTION(BlueprintCallable, Category = "Interaction|Debug")
	void PrintDebugStats() { DebugManager.PrintDebugStats(); }

protected:
	// ═══════════════════════════════════════════════
	// INITIALIZATION
	// ═══════════════════════════════════════════════

	/**
	 * Initialize the interaction system (called after possession check)
	 */
	void InitializeInteractionSystem();

	/**
	 * Check if possessed and confirm initialization
	 * (Fallback for multiplayer edge cases)
	 */
	void CheckPossessionAndInitialize();

	// ═══════════════════════════════════════════════
	// INTERNAL LOGIC
	// ═══════════════════════════════════════════════

	void InitializeSubManagers();
	void ApplyQuickSettings();
	void InteractWithActor(AActor* TargetActor);
	void PickupGroundItemToInventory(int32 ItemID);
	void PickupGroundItemAndEquip(int32 ItemID);
	void UpdateFocusState(TScriptInterface<IInteractable> NewInteractable);

	/** Timer callback for hold progress updates */
	void UpdateHoldProgress();

private:
	// ═══════════════════════════════════════════════
	// TIMERS
	// ═══════════════════════════════════════════════

	FTimerHandle InteractionCheckTimer;
	FTimerHandle HoldProgressTimer;
	FTimerHandle PossessionCheckTimer;
};