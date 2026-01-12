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
class UInteractableWidget;
class UItemInstance;

DECLARE_LOG_CATEGORY_EXTERN(LogInteractionManager, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCurrentInteractableChanged, UInteractableManager*, NewInteractable);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGroundItemFocusChanged, int32, GroundItemID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHoldProgressChanged, float, Progress);

/**
 * Interaction Manager Component
 * 
 * SINGLE RESPONSIBILITY: Coordinate interaction systems
 * - Manages sub-managers for tracing, validation, pickup, debug
 * - Routes input to appropriate handlers
 * - Maintains focus state
 * - Manages interaction widget display
 * 
 * WIDGET INTEGRATION:
 * - Automatically creates and manages UInteractableWidget
 * - Updates widget state based on interaction state
 * - Handles hold/mash progress visualization
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
	// WIDGET CONFIGURATION
	// ═══════════════════════════════════════════════

	/** Widget class to spawn for interaction prompts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Widget")
	TSubclassOf<UInteractableWidget> InteractionWidgetClass;

	/** Z-Order for the widget (higher = on top) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Widget")
	int32 WidgetZOrder = 10;

	/** Default action name for ground items */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Widget")
	UInputAction* GroundItemActionInput;

	/** Default text for ground item pickup */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Widget")
	FText GroundItemDefaultText = FText::FromString(TEXT("Pick Up"));

	/** Text format for ground items with name (use {0} for item name) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Widget")
	FText GroundItemNameFormat = FText::FromString(TEXT("Pick Up {0}"));

	// ═══════════════════════════════════════════════
	// HOLD INTERACTION CONFIG
	// ═══════════════════════════════════════════════

	/** Threshold for tap vs hold (0.0-1.0 progress) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Hold", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TapThreshold = 0.3f;

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

	// ═══════════════════════════════════════════════
	// EVENTS
	// ═══════════════════════════════════════════════

	/** Called when CurrentInteractable changes */
	UPROPERTY(BlueprintAssignable, Category = "Interaction|Events")
	FOnCurrentInteractableChanged OnCurrentInteractableChanged;

	/** Called when focused ground item changes */
	UPROPERTY(BlueprintAssignable, Category = "Interaction|Events")
	FOnGroundItemFocusChanged OnGroundItemFocusChanged;

	/** Called when hold progress changes */
	UPROPERTY(BlueprintAssignable, Category = "Interaction|Events")
	FOnHoldProgressChanged OnHoldProgressChanged;

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
	// WIDGET ACCESS
	// ═══════════════════════════════════════════════

	/** Get the interaction widget (creates if needed) */
	UFUNCTION(BlueprintPure, Category = "Interaction|Widget")
	UInteractableWidget* GetInteractionWidget() const { return InteractionWidget; }

	/** Force show/hide widget */
	UFUNCTION(BlueprintCallable, Category = "Interaction|Widget")
	void SetWidgetVisible(bool bVisible);

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

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsHoldingInteraction() const { return bIsHolding; }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	float GetCurrentHoldProgress() const;

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
	void InitializeWidget();
	void ApplyQuickSettings();

	// ═══════════════════════════════════════════════
	// INTERNAL LOGIC
	// ═══════════════════════════════════════════════

	void InteractWithActor(AActor* TargetActor);
	void PickupGroundItemToInventory(int32 ItemID);
	void PickupGroundItemAndEquip(int32 ItemID);
	void UpdateFocusState(TScriptInterface<IInteractable> NewInteractable);
	void UpdateGroundItemFocus(int32 NewGroundItemID);
	void UpdateHoldProgress();

	// ═══════════════════════════════════════════════
	// WIDGET MANAGEMENT
	// ═══════════════════════════════════════════════

	/** Update widget for actor interactable focus */
	void UpdateWidgetForActorInteractable(UInteractableManager* Interactable);

	/** Update widget for ground item focus */
	void UpdateWidgetForGroundItem(int32 GroundItemID);

	/** Hide widget (no focus) */
	void HideWidget();

	/** Set widget to holding state with progress */
	void SetWidgetHoldingState(float Progress);

	/** Set widget to completed state */
	void SetWidgetCompletedState();

	/** Set widget to cancelled state */
	void SetWidgetCancelledState();

	/** Get item instance for ground item ID */
	UItemInstance* GetGroundItemInstance(int32 GroundItemID) const;

private:
	// ═══════════════════════════════════════════════
	// WIDGET INSTANCE
	// ═══════════════════════════════════════════════

	/** The interaction widget instance */
	UPROPERTY()
	TObjectPtr<UInteractableWidget> InteractionWidget;

	// ═══════════════════════════════════════════════
	// STATE FLAGS
	// ═══════════════════════════════════════════════

	/** Guard flag to prevent double initialization */
	bool bSystemInitialized = false;

	/** Is currently in hold interaction? */
	bool bIsHolding = false;

	/** Last progress value (to avoid redundant updates) */
	float LastHoldProgress = -1.0f;

	// ═══════════════════════════════════════════════
	// TIMERS
	// ═══════════════════════════════════════════════

	FTimerHandle InteractionCheckTimer;
	FTimerHandle HoldProgressTimer;
	FTimerHandle PossessionCheckTimer;
};