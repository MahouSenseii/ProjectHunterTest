// Character/Component/InteractionManager.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interactable/Component/InteractableManager.h"
#include "Item/Library/ItemEnums.h"
#include "InteractionManager.generated.h"

class AALSPlayerCameraManager;
class UInteractableManager;
class IInteractable;
class UGroundItemSubsystem;
class UInventoryManager;
class UEquipmentManager;
class UCameraComponent;
class UALSDebugComponent;
class APlayerController;
class UInteractableWidget;
class UUserWidget;
class UItemInstance;

// Delegate for notifying when current interactable changes
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCurrentInteractableChanged, TScriptInterface<IInteractable>, NewInteractable);

/**
 * Handles interaction detection and pickup for PLAYER characters only
 * Add this component in Blueprint to player character
 * AI characters don't need this
 * 
 * MULTIPLAYER READY:
 * - Clients detect interactables locally (no network traffic)
 * - Server validates and executes interactions (authority)
 * - Automatic lag compensation for fair interaction
 * 
 * GROUND ITEMS SUPPORT:
 * - TAP: Pick up to inventory
 * - HOLD: Equip directly to hand/slot (with progress bar)
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECTHUNTERTEST_API UInteractionManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UInteractionManager();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ═══════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════
	
	/** Max interaction distance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float InteractionDistance = 300.0f;

	/** Sphere trace radius for catching overlapping items */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float InteractionSphereRadius = 50.0f;

	/** How often to check (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float InteractionCheckFrequency = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool ChangeInteractionOffset = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction",  meta = (EditCondition = "ChangeInteractionOffset"))
	float InteractionOffsetX = 0.0f; // Forward

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction",  meta = (EditCondition = "ChangeInteractionOffset"))
	float InteractionOffsetY = 0.0f;  // Right (SHOULDER!)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction",  meta = (EditCondition = "ChangeInteractionOffset"))
	float InteractionOffsetZ = 75.0f;   // Up

	/** Trace channel for interaction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	TEnumAsByte<ECollisionChannel> InteractionTraceChannel = ECC_GameTraceChannel1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|ALS")
	bool bUseALSCamera = true;

	/** Max distance camera can be from character before pulling trace start closer */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|ALS", meta = (EditCondition = "bUseALSCamera"))
	float MaxCameraDistanceForTrace = 500.0f;

	/** Pickup radius for "pickup all nearby" */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float PickupAllRadius = 200.0f;

	/** Weight for dot product in scoring (how centered in view) - Higher = prefer what you're looking at */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Scoring", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DotProductWeight = 0.7f;

	/** Weight for distance in scoring (how close) - Higher = prefer closer objects */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Scoring", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DistanceWeight = 0.3f;

	/** Server-side validation distance buffer (allows for lag compensation) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Multiplayer", meta = (ClampMin = "0.0"))
	float ServerValidationBuffer = 100.0f;

	// ═══════════════════════════════════════════════
	// WIDGET CONFIGURATION
	// ═══════════════════════════════════════════════
	
	/** Widget class for interaction prompt */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Widget")
	TSubclassOf<UUserWidget> InteractionWidgetClass;

	/** Enable widget display? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Widget")
	bool bShowWidget = true;

	/** Use screen space widget for ground items? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Widget")
	bool bUseScreenSpaceWidget = true;

	// ═══════════════════════════════════════════════
	// GROUND ITEM CONFIGURATION
	// ═══════════════════════════════════════════════
	
	/** How long to hold for direct equip (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Ground Items", meta = (ClampMin = "0.1"))
	float GroundItemHoldDuration = 1.0f;

	/** Enable direct equip on hold? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Ground Items")
	bool bEnableDirectEquip = true;

	/** Show "Hold to Equip" hint? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Ground Items")
	bool bShowEquipHint = true;

	// ═══════════════════════════════════════════════
	// CURRENT STATE
	// ═══════════════════════════════════════════════
	
	/** Current interactable being looked at (LOCAL ONLY - not replicated) */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TScriptInterface<IInteractable> CurrentInteractable;

	/** Called when CurrentInteractable changes (for widget updates, etc.) */
	UPROPERTY(BlueprintAssignable, Category = "Interaction|Events")
	FOnCurrentInteractableChanged OnCurrentInteractableChanged;

	// ═══════════════════════════════════════════════
	// GETTERS
	// ═══════════════════════════════════════════════

	/**
	 * Get current interactable as UInteractableManager
	 * @return Current interactable cast to UInteractableManager, or nullptr
	 */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	UInteractableManager* GetCurrentInteractable() const 
	{ 
		if (CurrentInteractable.GetInterface())
		{
			return Cast<UInteractableManager>(CurrentInteractable.GetObject());
		}
		return nullptr;
	}

	/**
	 * Get current interactable as interface (if you need the interface directly)
	 * @return Current interactable as TScriptInterface
	 */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	TScriptInterface<IInteractable> GetCurrentInteractableInterface() const 
	{ 
		return CurrentInteractable;
	}

	// ═══════════════════════════════════════════════
	// INTERACTION METHODS (Client calls these)
	// ═══════════════════════════════════════════════
	
	/** Called when interact button is pressed (handles both actors and ground items) */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void OnInteractPressed();

	/** Called when interact button is released (determines tap vs hold) */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void OnInteractReleased();

	/** Pickup all items in radius (server validated) */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void PickupAllNearbyItems();

private:
	// ═══════════════════════════════════════════════
	// SERVER RPCs (Called by client, executed on server)
	// ═══════════════════════════════════════════════
	
	/** Server: Validate and execute interaction */
	UFUNCTION(Server, Reliable)
	void ServerInteractWithActor(AActor* TargetActor, FVector ClientLocation);

	/** Server: Validate and pickup ground item to inventory */
	UFUNCTION(Server, Reliable)
	void ServerPickupGroundItem(int32 ItemID, FVector ClientLocation);

	/** Server: Validate and equip ground item directly */
	UFUNCTION(Server, Reliable)
	void ServerPickupAndEquipGroundItem(int32 ItemID, FVector ClientLocation);

	/** Server: Validate and pickup nearby items */
	UFUNCTION(Server, Reliable)
	void ServerPickupNearbyItems(FVector ClientLocation);

	// ═══════════════════════════════════════════════
	// VALIDATION (Server-side)
	// ═══════════════════════════════════════════════
	
	/** Validate interaction is legitimate (distance, line of sight, etc.) */
	bool ValidateInteraction(AActor* TargetActor, FVector ClientLocation) const;

	/** Validate ground item distance */
	bool ValidateGroundItemDistance(int32 ItemID, FVector ClientLocation) const;

	/** Check if actor implements IInteractable and can be interacted with */
	bool IsValidInteractable(AActor* Actor, AActor* Interactor) const;

	// ═══════════════════════════════════════════════
	// LOCAL DETECTION (Runs on owning client only)
	// ═══════════════════════════════════════════════
	
	/** Timer handle for interaction checks */
	FTimerHandle InteractionCheckTimer;

	/** Check for interactables (CLIENT ONLY) */
	void CheckForInteractables();

	// Main detection function
	TScriptInterface<IInteractable> TraceForInteractable();
	
	// Stage 1: Line trace
	bool PerformLineTrace(FVector CameraLocation, FVector TraceEnd, FHitResult& OutHit);
	
	// Stage 2: Sphere trace
	TArray<FHitResult> PerformSphereTrace(FVector CameraLocation, FVector TraceEnd);
	
	// Stage 3: Scoring
	struct FInteractableCandidate
	{
		TScriptInterface<IInteractable> Interactable;
		AActor* Actor;
		FVector ImpactPoint;
		float DistanceSq;
		float DotProduct;
		float Score;
		
		FInteractableCandidate() 
			: Actor(nullptr), ImpactPoint(FVector::ZeroVector), 
			  DistanceSq(0.0f), DotProduct(0.0f), Score(0.0f) {}
	};
	
	TArray<FInteractableCandidate> GatherCandidates(const TArray<FHitResult>& Hits, FVector CameraLocation, FVector CameraForward);
	FInteractableCandidate CreateCandidate(const FHitResult& Hit, FVector CameraLocation, FVector CameraForward);
	float CalculateInteractionScore(float DistanceSq, float DotProduct) const;
	
	// Filtering
	bool IsInFrontOfCamera(float DotProduct) const;
	bool IsInFrontOfPlayer(AActor* TargetActor, FVector TargetLocation) const;
	
	// Selection
	TScriptInterface<IInteractable> SelectBestCandidate(const TArray<FInteractableCandidate>& Candidates);
	
	// Debug
	void DebugDrawLineTrace(FVector Start, FVector End, bool bHit) const;
	void DebugDrawSphereTrace(FVector Start, FVector End) const;
	void DebugDrawCandidate(const FInteractableCandidate& Candidate) const;
	void DebugDrawWinner(const TScriptInterface<IInteractable>& Winner, FVector CameraLocation, int32 CandidateCount) const;

	/** Helper: Create interactable interface from actor */
	TScriptInterface<IInteractable> CreateInteractableFromActor(
		AActor* Actor, 
		UInteractableManager* Component, 
		bool bActorImplements
	);

	// ═══════════════════════════════════════════════
	// GROUND ITEM MANAGEMENT
	// ═══════════════════════════════════════════════
	
	/** Current ground item ID being looked at */
	int32 CurrentGroundItemID = -1;

	/** Is holding for ground item equip? */
	bool bIsHoldingForGroundItem = false;

	/** Hold progress [0.0 - 1.0] */
	float GroundItemHoldProgress = 0.0f;

	/** Ground item widget (screen space) */
	UPROPERTY()
	UInteractableWidget* GroundItemWidget = nullptr;

	/** Update ground item hold progress */
	void UpdateGroundItemHoldProgress(float DeltaTime);

	/** Update ground item widget display */
	void UpdateGroundItemWidget(UItemInstance* Item);

	/** Update widget text for normal display */
	void UpdateGroundItemWidgetNormal(UItemInstance* Item);

	/** Update widget text for hold state */
	void UpdateGroundItemWidgetForHold(UItemInstance* Item);

	/** Cancel ground item hold */
	void CancelGroundItemHold();

	/** Determine equipment slot for item */
	EEquipmentSlot DetermineEquipmentSlot(UItemInstance* Item) const;

	// ═══════════════════════════════════════════════
	// CACHED REFERENCES
	// ═══════════════════════════════════════════════
	
	UPROPERTY()
	UALSDebugComponent* ALSDebugComponent;

	UPROPERTY()
	UGroundItemSubsystem* GroundItemSubsystem;

	UPROPERTY()
	UInventoryManager* InventoryComponent;

	UPROPERTY()
	UEquipmentManager* EquipmentComponent;

	UPROPERTY()
	APlayerController* PlayerController;

	UPROPERTY()
	AALSPlayerCameraManager* ALSCameraManager; 

	// ═══════════════════════════════════════════════
	// HELPER FUNCTIONS
	// ═══════════════════════════════════════════════
	
	
	void CacheComponents();
	bool GetCameraViewPoint(FVector& OutLocation, FRotator& OutRotation) const;
	FVector GetTraceStartLocation(const FVector& CameraLocation, const FRotator& CameraRotation) const;
	bool ShouldShowDebugTraces() const;
	bool IsLocallyControlled() const;
};