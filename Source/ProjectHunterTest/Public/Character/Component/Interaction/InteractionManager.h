// Character/Component/Interaction/InteractionManager.h
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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCurrentInteractableChanged, UInteractableManager*, NewInteractable);

/**
 * Interaction Manager Component
 * 
 * SINGLE RESPONSIBILITY: Coordinate interaction systems
 * - Manages sub-managers for tracing, validation, pickup, debug
 * - Routes input to appropriate handlers
 * - Maintains focus state
 * 
 * FIXES APPLIED:
 * - Added bSystemInitialized guard to prevent double initialization
 * - Uses correct interface function names (OnBeginFocus, OnEndFocus)
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
	// CONFIGURATION
	// ═══════════════════════════════════════════════

	/** Enable interaction system */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Setup")
	bool bInteractionEnabled = true;

	/** Trace Manager - Handles tracing for interactables */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Managers", meta = (ShowOnlyInnerProperties))
	FInteractionTraceManager TraceManager;

	/** Validator Manager - Validates interactions server-side */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Managers", meta = (ShowOnlyInnerProperties))
	FInteractionValidatorManager ValidatorManager;

	/** Pickup Manager - Handles ground item pickup */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Managers", meta = (ShowOnlyInnerProperties))
	FGroundItemPickupManager PickupManager;

	/** Debug Manager - Handles debug visualization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Managers", meta = (ShowOnlyInnerProperties))
	FInteractionDebugManager DebugManager;

	// ═══════════════════════════════════════════════
	// QUICK SETTINGS
	// ═══════════════════════════════════════════════

	/** Quick toggle for debug visualization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Quick Settings")
	bool bDebugEnabled = false;

	// ═══════════════════════════════════════════════
	// STATE
	// ═══════════════════════════════════════════════

	/** Current interactable being looked at */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction|State")
	TScriptInterface<IInteractable> CurrentInteractable;

	/** Current ground item ID (-1 if none) */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction|State")
	int32 CurrentGroundItemID = -1;

	/** Called when CurrentInteractable changes */
	UPROPERTY(BlueprintAssignable, Category = "Interaction|Events")
	FOnCurrentInteractableChanged OnCurrentInteractableChanged;

	// ═══════════════════════════════════════════════
	// PRIMARY INTERFACE
	// ═══════════════════════════════════════════════

	/** Called when interact button is pressed */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void OnInteractPressed();

	/** Called when interact button is released */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void OnInteractReleased();

	/** Pickup all items in radius around player */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void PickupAllNearbyItems();

	/** Check for interactables (called on timer) */
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

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsSystemInitialized() const { return bSystemInitialized; }

	// ═══════════════════════════════════════════════
	// DEBUG
	// ═══════════════════════════════════════════════

	UFUNCTION(BlueprintCallable, Category = "Interaction|Debug")
	void PrintDebugStats() { DebugManager.PrintDebugStats(); }

protected:
	// ═══════════════════════════════════════════════
	// INITIALIZATION
	// ═══════════════════════════════════════════════

	void InitializeInteractionSystem();
	void CheckPossessionAndInitialize();
	void InitializeSubManagers();
	void ApplyQuickSettings();

	// ═══════════════════════════════════════════════
	// INTERNAL LOGIC
	// ═══════════════════════════════════════════════

	void InteractWithActor(AActor* TargetActor);
	void PickupGroundItemToInventory(int32 ItemID);
	void PickupGroundItemAndEquip(int32 ItemID);
	void UpdateFocusState(TScriptInterface<IInteractable> NewInteractable);
	void UpdateHoldProgress();

private:
	// ═══════════════════════════════════════════════
	// STATE FLAGS
	// ═══════════════════════════════════════════════

	/** 
	 * FIX: Guard flag to prevent double initialization
	 */
	bool bSystemInitialized = false;

	// ═══════════════════════════════════════════════
	// TIMERS
	// ═══════════════════════════════════════════════

	FTimerHandle InteractionCheckTimer;
	FTimerHandle HoldProgressTimer;
	FTimerHandle PossessionCheckTimer;
};
