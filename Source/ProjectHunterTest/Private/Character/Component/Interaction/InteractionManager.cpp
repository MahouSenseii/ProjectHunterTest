// Character/Component/Interaction/InteractionManager.cpp

#include "Character/Component/Interaction/InteractionManager.h"
#include "Interactable/Interface/Interactable.h"
#include "Interactable/Component/InteractableManager.h"
#include "Interactable/Widget/InteractableWidget.h"
#include "Tower/Subsystem/GroundItemSubsystem.h"
#include "Item/ItemInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY(LogInteractionManager);

// ═══════════════════════════════════════════════════════════════════════
// LIFECYCLE
// ═══════════════════════════════════════════════════════════════════════

UInteractionManager::UInteractionManager()
{
	PrimaryComponentTick.bCanEverTick = false; 
	SetIsReplicatedByDefault(false);
	
	CurrentGroundItemID = -1;
	bSystemInitialized = false;
	bIsHolding = false;
	LastHoldProgress = -1.0f;
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
	
	// Check locally controlled directly here instead of delegating
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

	// Remove widget
	if (InteractionWidget)
	{
		InteractionWidget->RemoveFromParent();
		InteractionWidget = nullptr;
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
		bIsHolding = true;
		LastHoldProgress = 0.0f;
		
		// Update widget to holding state
		SetWidgetHoldingState(0.0f);
		
		// Start timer for hold progress updates (60 FPS)
		GetWorld()->GetTimerManager().SetTimer(
			HoldProgressTimer,
			this,
			&UInteractionManager::UpdateHoldProgress,
			0.016f, // ~60 FPS
			true    // Loop
		);
		
		UE_LOG(LogInteractionManager, Verbose, TEXT("InteractionManager: Started hold interaction for item %d"), CurrentGroundItemID);
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
		bIsHolding = false;
		
		// If progress < threshold, treat as tap (pickup to inventory)
		if (HoldProgress < TapThreshold)
		{
			int32 ItemID = PickupManager.GetCurrentHoldItemID();
			PickupManager.CancelHoldInteraction();
			PickupGroundItemToInventory(ItemID);
			
			// Widget shows completed briefly, then updates based on new focus
			SetWidgetCompletedState();
			
			UE_LOG(LogInteractionManager, Verbose, TEXT("InteractionManager: Tap pickup for item %d"), ItemID);
		}
		else
		{
			// Released before completing hold - cancel
			PickupManager.CancelHoldInteraction();
			SetWidgetCancelledState();
			
			UE_LOG(LogInteractionManager, Verbose, TEXT("InteractionManager: Hold cancelled at %.1f%%"), HoldProgress * 100.0f);
		}
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

	UE_LOG(LogInteractionManager, Log, TEXT("InteractionManager: Picked up %d items from area"), PickedUpCount);
}

void UInteractionManager::CheckForInteractables()
{
	// Now TraceManager is properly initialized, so we can safely
	// delegate to TraceManager.IsLocallyControlled()
	if (!bInteractionEnabled || !IsLocallyControlled())
	{
		return;
	}

	// Don't update focus while holding (would be confusing)
	if (bIsHolding)
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
		UpdateGroundItemFocus(NewGroundItemID);
	}

	// ═══════════════════════════════════════════════
	// WIDGET UPDATE (Based on current focus)
	// ═══════════════════════════════════════════════
	if (CurrentInteractable.GetInterface())
	{
		UpdateWidgetForActorInteractable(GetCurrentInteractable());
	}
	else if (CurrentGroundItemID != -1)
	{
		UpdateWidgetForGroundItem(CurrentGroundItemID);
	}
	else
	{
		HideWidget();
	}

	// ═══════════════════════════════════════════════
	// DEBUG VISUALIZATION
	// ═══════════════════════════════════════════════
	if (bDebugEnabled)
	{
		UInteractableManager* InteractableComp = GetCurrentInteractable();
		float Distance = 0.0f;
		
		// Calculate distance if we have an interactable
		if (InteractableComp)
		{
			Distance = FVector::Distance(CameraLocation, InteractableComp->GetOwner()->GetActorLocation());
			
			// Draw detailed interactable info
			DebugManager.DrawInteractableInfo(InteractableComp, Distance);
		}
		
		// Draw ground item visualization
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
// WIDGET ACCESS
// ═══════════════════════════════════════════════════════════════════════

void UInteractionManager::SetWidgetVisible(bool bVisible)
{
	if (!InteractionWidget)
	{
		return;
	}

	if (bVisible)
	{
		InteractionWidget->Show();
	}
	else
	{
		InteractionWidget->Hide();
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

float UInteractionManager::GetCurrentHoldProgress() const
{
	if (bIsHolding)
	{
		return PickupManager.GetHoldProgress();
	}
	return 0.0f;
}

// ═══════════════════════════════════════════════════════════════════════
// INITIALIZATION
// ═══════════════════════════════════════════════════════════════════════

void UInteractionManager::InitializeInteractionSystem()
{
	// ═══════════════════════════════════════════════
	// FIX: Guard against double initialization
	// ═══════════════════════════════════════════════
	if (bSystemInitialized)
	{
		UE_LOG(LogInteractionManager, Verbose, TEXT("InteractionManager: Already initialized, skipping"));
		return;
	}

	UE_LOG(LogInteractionManager, Log, TEXT("═══════════════════════════════════════════"));
	UE_LOG(LogInteractionManager, Log, TEXT("  INTERACTION MANAGER - Initializing"));
	UE_LOG(LogInteractionManager, Log, TEXT("═══════════════════════════════════════════"));

	// Initialize all sub-managers (lightweight!)
	InitializeSubManagers();

	// Initialize widget
	InitializeWidget();

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
	
	bSystemInitialized = true;
	
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

	// Connect debug manager to trace manager for trace visualization
	TraceManager.SetDebugManager(&DebugManager);

	UE_LOG(LogInteractionManager, Log, TEXT("InteractionManager: All sub-managers initialized"));
}

void UInteractionManager::InitializeWidget()
{
	// Need a valid widget class
	if (!InteractionWidgetClass)
	{
		UE_LOG(LogInteractionManager, Warning, TEXT("InteractionManager: No InteractionWidgetClass set - widget disabled"));
		return;
	}

	// Get owning player controller
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (!PC)
	{
		UE_LOG(LogInteractionManager, Warning, TEXT("InteractionManager: No PlayerController - widget deferred"));
		return;
	}

	// Create widget
	InteractionWidget = CreateWidget<UInteractableWidget>(PC, InteractionWidgetClass);
	if (!InteractionWidget)
	{
		UE_LOG(LogInteractionManager, Error, TEXT("InteractionManager: Failed to create interaction widget"));
		return;
	}

	// Add to viewport (hidden initially)
	InteractionWidget->AddToViewport(WidgetZOrder);
	InteractionWidget->Hide();

	UE_LOG(LogInteractionManager, Log, TEXT("InteractionManager: Widget initialized (Class: %s)"), 
		*InteractionWidgetClass->GetName());
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

// ═══════════════════════════════════════════════════════════════════════
// INTERNAL LOGIC
// ═══════════════════════════════════════════════════════════════════════

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
	
	// Item was picked up, clear current focus
	if (bSuccess && CurrentGroundItemID == ItemID)
	{
		CurrentGroundItemID = -1;
	}
}

void UInteractionManager::PickupGroundItemAndEquip(int32 ItemID)
{
	bool bSuccess = PickupManager.PickupAndEquip(ItemID);
	
	if (bDebugEnabled)
	{
		DebugManager.LogGroundItemPickup(ItemID, false, bSuccess);
	}
	
	// Item was picked up, clear current focus
	if (bSuccess && CurrentGroundItemID == ItemID)
	{
		CurrentGroundItemID = -1;
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

void UInteractionManager::UpdateGroundItemFocus(int32 NewGroundItemID)
{
	int32 OldGroundItemID = CurrentGroundItemID;
	CurrentGroundItemID = NewGroundItemID;
	
	// Broadcast change event
	if (OldGroundItemID != NewGroundItemID)
	{
		OnGroundItemFocusChanged.Broadcast(NewGroundItemID);
	}
}

void UInteractionManager::UpdateHoldProgress()
{
	// Update hold progress via timer (called at 60 FPS)
	bool bCompleted = PickupManager.UpdateHoldProgress(0.016f);
	
	float CurrentProgress = PickupManager.GetHoldProgress();
	
	// Update widget progress (only if changed significantly)
	if (FMath::Abs(CurrentProgress - LastHoldProgress) > 0.005f)
	{
		LastHoldProgress = CurrentProgress;
		SetWidgetHoldingState(CurrentProgress);
		
		// Broadcast progress change
		OnHoldProgressChanged.Broadcast(CurrentProgress);
	}
	
	if (bCompleted)
	{
		// Hold completed - equip the item
		int32 ItemID = PickupManager.GetCurrentHoldItemID();
		
		// Stop timer
		GetWorld()->GetTimerManager().ClearTimer(HoldProgressTimer);
		bIsHolding = false;
		
		// Execute equip
		PickupGroundItemAndEquip(ItemID);
		
		// Widget shows completed state
		SetWidgetCompletedState();
		
		UE_LOG(LogInteractionManager, Log, TEXT("InteractionManager: Hold interaction completed - equipped item %d"), ItemID);
	}
}

// ═══════════════════════════════════════════════════════════════════════
// WIDGET MANAGEMENT
// ═══════════════════════════════════════════════════════════════════════

void UInteractionManager::UpdateWidgetForActorInteractable(UInteractableManager* Interactable)
{
	if (!InteractionWidget || !Interactable)
	{
		return;
	}

	// Get interaction data from interactable
	FName ActionName = Interactable->Config.ActionName;
	FText Description = Interactable->Config.InteractionText;

	// Update widget
	InteractionWidget->SetInteractionData(ActionName, Description);
	InteractionWidget->SetWidgetState(EInteractionWidgetState::IWS_Idle);
	InteractionWidget->Show();
}

void UInteractionManager::UpdateWidgetForGroundItem(int32 GroundItemID)
{
	if (!InteractionWidget || GroundItemID == -1)
	{
		return;
	}

	// Try to get item name
	FText Description = GroundItemDefaultText;
	
	if (UItemInstance* Item = GetGroundItemInstance(GroundItemID))
	{
		FText ItemName = Item->GetDisplayName();
		if (!ItemName.IsEmpty())
		{
			// Format: "Pick Up {ItemName}"
			Description = FText::Format(GroundItemNameFormat, ItemName);
		}
	}

	// Update widget
	InteractionWidget->SetInteractionData(GroundItemActionName, Description);
	InteractionWidget->SetWidgetState(EInteractionWidgetState::IWS_Idle);
	InteractionWidget->Show();
}

void UInteractionManager::HideWidget()
{
	if (!InteractionWidget)
	{
		return;
	}

	InteractionWidget->Hide();
}

void UInteractionManager::SetWidgetHoldingState(float Progress)
{
	if (!InteractionWidget)
	{
		return;
	}

	InteractionWidget->SetWidgetState(EInteractionWidgetState::IWS_Holding);
	InteractionWidget->SetProgress(Progress);
}

void UInteractionManager::SetWidgetCompletedState()
{
	if (!InteractionWidget)
	{
		return;
	}

	InteractionWidget->SetWidgetState(EInteractionWidgetState::IWS_Completed);
}

void UInteractionManager::SetWidgetCancelledState()
{
	if (!InteractionWidget)
	{
		return;
	}

	InteractionWidget->SetWidgetState(EInteractionWidgetState::IWS_Cancelled);
}

UItemInstance* UInteractionManager::GetGroundItemInstance(int32 GroundItemID) const
{
	if (GroundItemID == -1)
	{
		return nullptr;
	}

	UGroundItemSubsystem* Subsystem = GetWorld()->GetSubsystem<UGroundItemSubsystem>();
	if (!Subsystem)
	{
		return nullptr;
	}

	return Subsystem->GetItemByID(GroundItemID);
}