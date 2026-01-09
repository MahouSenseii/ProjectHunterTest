// Character/Component/InteractionManager.cpp

#include "Character/Component/Interaction/InteractionManager.h"
#include "Interactable/Interface/Interactable.h"
#include "Interactable/Component/InteractableManager.h"
#include "Tower/Subsystem/GroundItemSubsystem.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY(LogInteractionManager);

// ═══════════════════════════════════════════════════════════════════════
// LIFECYCLE
// ═══════════════════════════════════════════════════════════════════════

UInteractionManager::UInteractionManager()
{
	PrimaryComponentTick.bCanEverTick = false; 
	SetIsReplicatedByDefault(false);
}

void UInteractionManager::BeginPlay()
{
	Super::BeginPlay();

	// ═══════════════════════════════════════════════
	// EARLY VALIDATION
	// ═══════════════════════════════════════════════

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		UE_LOG(LogInteractionManager, Warning, TEXT("InteractionManager: Owner is not a Pawn"));
		return;
	}

	// ═══════════════════════════════════════════════
	// CHECK LOCAL CONTROL (Direct check, not delegated)
	// ═══════════════════════════════════════════════
	
	// FIXED: Check locally controlled directly here instead of delegating
	// to uninitialized TraceManager. We can't call IsLocallyControlled()
	// because TraceManager.OwnerActor is still nullptr at this point.
	if (!OwnerPawn->IsLocallyControlled())
	{
		UE_LOG(LogInteractionManager, Log, TEXT("InteractionManager: Not locally controlled - skipping initialization"));
		return;
	}

	// ═══════════════════════════════════════════════
	// INITIALIZE SYSTEM
	// ═══════════════════════════════════════════════
	
	InitializeInteractionSystem();
	
	// ═══════════════════════════════════════════════
	// POSSESSION CHECK (Fallback for MP)
	// ═══════════════════════════════════════════════
	
	// This handles edge cases in multiplayer where possession
	// might not be fully established yet
	UE_LOG(LogInteractionManager, Log, TEXT("InteractionManager: Starting possession verification timer..."));
	
	GetWorld()->GetTimerManager().SetTimer(
		PossessionCheckTimer,
		this,
		&UInteractionManager::CheckPossessionAndInitialize,
		0.1f,  // Check every 0.1 seconds
		true   // Loop
	);
}

void UInteractionManager::Initialize()
{
	// Public initialization function (for manual setup if needed)
	if (bInteractionEnabled)
	{
		GetWorld()->GetTimerManager().SetTimer(
			InteractionCheckTimer,
			this,
			&UInteractionManager::CheckForInteractables,
			TraceManager.CheckFrequency,
			true
		);

		UE_LOG(LogInteractionManager, Log, TEXT("InteractionManager: Manually initialized on %s (Frequency: %.2fs)"), 
			*GetOwner()->GetName(), TraceManager.CheckFrequency);
	}
}

void UInteractionManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UInteractionManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clear all timers
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(InteractionCheckTimer);
		GetWorld()->GetTimerManager().ClearTimer(HoldProgressTimer);
		GetWorld()->GetTimerManager().ClearTimer(PossessionCheckTimer);
	}

	// End focus on current interactable
	if (CurrentInteractable.GetInterface())
	{
		IInteractable::Execute_OnEndFocus(CurrentInteractable.GetObject(), GetOwner());
	}

	Super::EndPlay(EndPlayReason);
}

// ═══════════════════════════════════════════════════════════════════════
// PRIMARY INTERFACE
// ═══════════════════════════════════════════════════════════════════════

void UInteractionManager::OnInteractPressed()
{
	if (!bInteractionEnabled)
	{
		return;
	}

	// PRIORITY 1: Actor-based interactables
	if (CurrentInteractable.GetInterface())
	{
		UInteractableManager* InteractableComp = Cast<UInteractableManager>(CurrentInteractable.GetObject());
		if (InteractableComp)
		{
			AActor* TargetActor = InteractableComp->GetOwner();
			InteractWithActor(TargetActor);
			return;
		}
	}

	// PRIORITY 2: Ground items (start hold interaction)
	if (CurrentGroundItemID != -1)
	{
		// Start hold interaction
		PickupManager.StartHoldInteraction(CurrentGroundItemID);
		
		// Start timer for hold progress updates (60 FPS)
		GetWorld()->GetTimerManager().SetTimer(
			HoldProgressTimer,
			this,
			&UInteractionManager::UpdateHoldProgress,
			0.016f, // ~60 FPS
			true    // Loop
		);
	}
}

void UInteractionManager::OnInteractReleased()
{
	if (!bInteractionEnabled)
	{
		return;
	}

	// If holding for ground item, check if it was a tap (quick release)
	if (PickupManager.IsHoldingForGroundItem())
	{
		float HoldProgress = PickupManager.GetHoldProgress();
		
		// Stop the hold progress timer
		GetWorld()->GetTimerManager().ClearTimer(HoldProgressTimer);
		
		// If progress < threshold, treat as tap (pickup to inventory)
		if (HoldProgress < 0.3f) // Tap threshold
		{
			int32 ItemID = PickupManager.GetCurrentHoldItemID();
			PickupManager.CancelHoldInteraction();
			PickupGroundItemToInventory(ItemID);
		}
		// Otherwise, hold will complete naturally (equip)
	}
}

void UInteractionManager::PickupAllNearbyItems()
{
	// Get camera location
	FVector CameraLocation;
	FRotator CameraRotation;
	if (!TraceManager.GetCameraViewPoint(CameraLocation, CameraRotation))
	{
		return;
	}

	// Pickup all in radius
	int32 PickedUpCount = PickupManager.PickupAllNearby(CameraLocation);

	UE_LOG(LogTemp, Log, TEXT("InteractionManager: Picked up %d items from area"), PickedUpCount);
}

void UInteractionManager::CheckForInteractables()
{
	// FIXED: Now TraceManager is properly initialized, so we can safely
	// delegate to TraceManager.IsLocallyControlled()
	if (!bInteractionEnabled || !IsLocallyControlled())
	{
		return;
	}

	// Get camera location for debug visualization
	FVector CameraLocation;
	FRotator CameraRotation;
	TraceManager.GetCameraViewPoint(CameraLocation, CameraRotation);

	// ═══════════════════════════════════════════════
	// DEBUG: Draw interaction range sphere
	// ═══════════════════════════════════════════════
	if (bDebugEnabled)
	{
		DebugManager.DrawInteractionRange(CameraLocation, TraceManager.InteractionDistance);
	}

	// PRIORITY 1: Check for actor-based interactables
	TScriptInterface<IInteractable> NewInteractable = TraceManager.TraceForActorInteractable();

	// PRIORITY 2: Check for ground items (if no actor found)
	int32 NewGroundItemID = -1;
	if (!NewInteractable.GetInterface())
	{
		TraceManager.FindNearestGroundItem(NewGroundItemID);
	}

	// Update actor interactable state
	if (NewInteractable != CurrentInteractable)
	{
		UpdateFocusState(NewInteractable);
	}

	// Update ground item state
	if (NewGroundItemID != CurrentGroundItemID)
	{
		CurrentGroundItemID = NewGroundItemID;
	}

	// ═══════════════════════════════════════════════
	// DEBUG VISUALIZATION (NOW COMPLETE!)
	// ═══════════════════════════════════════════════
	if (bDebugEnabled)
	{
		UInteractableManager* InteractableComp = GetCurrentInteractable();
		float Distance = 0.0f;
		
		// Calculate distance if we have an interactable
		if (InteractableComp)
		{
			Distance = FVector::Distance(CameraLocation, InteractableComp->GetOwner()->GetActorLocation());
			
			// ✅ NOW ACTUALLY CALLED: Draw detailed interactable info
			DebugManager.DrawInteractableInfo(InteractableComp, Distance);
		}
		
		// ✅ NOW ACTUALLY CALLED: Draw ground item visualization
		if (CurrentGroundItemID != -1)
		{
			// Get ground item location from subsystem
			if (UGroundItemSubsystem* Subsystem = GetWorld()->GetSubsystem<UGroundItemSubsystem>())
			{
				const TMap<int32, FVector>& Locations = Subsystem->GetInstanceLocations();
				if (const FVector* LocationPtr = Locations.Find(CurrentGroundItemID))
				{
					DebugManager.DrawGroundItem(*LocationPtr, CurrentGroundItemID);
				}
			}
		}

		// Display on-screen debug text
		DebugManager.DisplayInteractionState(InteractableComp, Distance, CurrentGroundItemID);
	}
}

// ═══════════════════════════════════════════════════════════════════════
// GETTERS
// ═══════════════════════════════════════════════════════════════════════

UInteractableManager* UInteractionManager::GetCurrentInteractable() const
{
	if (CurrentInteractable.GetInterface())
	{
		return Cast<UInteractableManager>(CurrentInteractable.GetObject());
	}
	return nullptr;
}

// ═══════════════════════════════════════════════════════════════════════
// INTERNAL LOGIC
// ═══════════════════════════════════════════════════════════════════════

void UInteractionManager::InitializeInteractionSystem()
{
	UE_LOG(LogInteractionManager, Log, TEXT("═══════════════════════════════════════════"));
	UE_LOG(LogInteractionManager, Log, TEXT("  INTERACTION MANAGER - Initializing"));
	UE_LOG(LogInteractionManager, Log, TEXT("═══════════════════════════════════════════"));

	// Initialize all sub-managers (lightweight!)
	InitializeSubManagers();

	// Apply quick settings (debug mode)
	ApplyQuickSettings();

	// Start interaction check timer
	if (bInteractionEnabled)
	{
		GetWorld()->GetTimerManager().SetTimer(
			InteractionCheckTimer,
			this,
			&UInteractionManager::CheckForInteractables,
			TraceManager.CheckFrequency,
			true
		);

		UE_LOG(LogInteractionManager, Log, TEXT("InteractionManager: ✓ Initialized on %s (Frequency: %.2fs)"), 
			*GetOwner()->GetName(), TraceManager.CheckFrequency);
	}
	
	UE_LOG(LogInteractionManager, Log, TEXT("═══════════════════════════════════════════"));
}

void UInteractionManager::CheckPossessionAndInitialize()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		GetWorld()->GetTimerManager().ClearTimer(PossessionCheckTimer);
		return;
	}

	// Check if possessed now (direct check again, not delegated)
	if (OwnerPawn->IsLocallyControlled())
	{
		UE_LOG(LogInteractionManager, Log, TEXT("InteractionManager: Possession confirmed!"));
		
		// Stop checking
		GetWorld()->GetTimerManager().ClearTimer(PossessionCheckTimer);
		
		// Already initialized in BeginPlay, so we just confirm here
		UE_LOG(LogInteractionManager, Log, TEXT("InteractionManager: System already active"));
	}
}

void UInteractionManager::InitializeSubManagers()
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();

	if (!Owner || !World)
	{
		UE_LOG(LogInteractionManager, Error, TEXT("InteractionManager: Invalid owner or world"));
		return;
	}

	// Initialize all managers (lightweight, just passing references!)
	TraceManager.Initialize(Owner, World);
	ValidatorManager.Initialize(Owner, World);
	PickupManager.Initialize(Owner, World);
	DebugManager.Initialize(Owner, World);

	// ✅ Connect debug manager to trace manager for trace visualization
	TraceManager.SetDebugManager(&DebugManager);

	UE_LOG(LogTemp, Log, TEXT("InteractionManager: All sub-managers initialized"));
}

void UInteractionManager::ApplyQuickSettings()
{
	// Apply quick-access debug setting to DebugManager
	if (bDebugEnabled)
	{
		DebugManager.DebugMode = EInteractionDebugMode::Full;
	}
	else
	{
		DebugManager.DebugMode = EInteractionDebugMode::None;
	}
}

void UInteractionManager::InteractWithActor(AActor* TargetActor)
{
	if (!TargetActor)
	{
		return;
	}

	// Get client location for validation
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	FVector ClientLocation = Owner->GetActorLocation();

	// Validate interaction (server-side)
	if (ValidatorManager.HasAuthority())
	{
		if (!ValidatorManager.ValidateActorInteraction(TargetActor, ClientLocation, TraceManager.InteractionDistance))
		{
			UE_LOG(LogInteractionManager, Warning, TEXT("InteractionManager: Actor interaction failed validation"));
			
			if (bDebugEnabled)
			{
				DebugManager.LogInteraction(GetCurrentInteractable(), false, "Validation failed");
			}
			return;
		}
	}

	// Execute interaction
	UInteractableManager* InteractableComp = TargetActor->FindComponentByClass<UInteractableManager>();
	if (InteractableComp)
	{
		IInteractable::Execute_OnInteract(InteractableComp, Owner);
		
		if (bDebugEnabled)
		{
			DebugManager.LogInteraction(InteractableComp, true);
		}
	}
	else if (TargetActor->GetClass()->ImplementsInterface(UInteractable::StaticClass()))
	{
		IInteractable::Execute_OnInteract(TargetActor, Owner);
	}
}

void UInteractionManager::PickupGroundItemToInventory(int32 ItemID)
{
	bool bSuccess = PickupManager.PickupToInventory(ItemID);
	
	if (bDebugEnabled)
	{
		DebugManager.LogGroundItemPickup(ItemID, true, bSuccess);
	}
}

void UInteractionManager::PickupGroundItemAndEquip(int32 ItemID)
{
	bool bSuccess = PickupManager.PickupAndEquip(ItemID);
	
	if (bDebugEnabled)
	{
		DebugManager.LogGroundItemPickup(ItemID, false, bSuccess);
	}
}

void UInteractionManager::UpdateFocusState(TScriptInterface<IInteractable> NewInteractable)
{
	// End focus on old interactable
	if (CurrentInteractable.GetInterface())
	{
		IInteractable::Execute_OnEndFocus(CurrentInteractable.GetObject(), GetOwner());
	}

	// Update current interactable
	CurrentInteractable = NewInteractable;

	// Start focus on new interactable
	if (CurrentInteractable.GetInterface())
	{
		IInteractable::Execute_OnBeginFocus(CurrentInteractable.GetObject(), GetOwner());
	}

	// Broadcast change event
	OnCurrentInteractableChanged.Broadcast(GetCurrentInteractable());
}

void UInteractionManager::UpdateHoldProgress()
{
	// Update hold progress via timer (called at 60 FPS)
	bool bCompleted = PickupManager.UpdateHoldProgress(0.016f);
	
	if (bCompleted)
	{
		// Hold completed, stop timer
		GetWorld()->GetTimerManager().ClearTimer(HoldProgressTimer);
		
		UE_LOG(LogInteractionManager, Verbose, TEXT("InteractionManager: Hold interaction completed via timer"));
	}
}