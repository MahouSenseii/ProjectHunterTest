// Character/Component/Interaction/InteractionManager.cpp

#include "Character/Component/Interaction/InteractionManager.h"
#include "Interactable/Interface/Interactable.h"
#include "Interactable/Component/InteractableManager.h"
#include "Tower/Subsystem/GroundItemSubsystem.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogInteractionManager);

UInteractionManager::UInteractionManager()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	
	CurrentGroundItemID = -1;
	bSystemInitialized = false;
}

void UInteractionManager::BeginPlay()
{
	Super::BeginPlay();

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		UE_LOG(LogInteractionManager, Warning, TEXT("InteractionManager: Owner is not a Pawn!"));
		return;
	}

	if (OwnerPawn->IsLocallyControlled())
	{
		InitializeInteractionSystem();
	}
	else
	{
		GetWorld()->GetTimerManager().SetTimer(
			PossessionCheckTimer,
			this,
			&UInteractionManager::CheckPossessionAndInitialize,
			0.1f,
			true,
			0.5f
		);
		
		UE_LOG(LogInteractionManager, Log, TEXT("InteractionManager: Waiting for possession..."));
	}
}

void UInteractionManager::Initialize()
{
	InitializeInteractionSystem();
}

void UInteractionManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	if (!bSystemInitialized || !bInteractionEnabled)
	{
		return;
	}
	
	FVector CameraLocation;
	FVector CameraDirection;
	TraceManager.GetTraceOrigin(CameraLocation, CameraDirection);

	if (bDebugEnabled)
	{
		UInteractableManager* InteractableComp = GetCurrentInteractable();
		float Distance = 0.0f;
		
		if (InteractableComp)
		{
			Distance = FVector::Distance(CameraLocation, InteractableComp->GetOwner()->GetActorLocation());
			DebugManager.DrawInteractableInfo(InteractableComp, Distance);
		}
		
		if (CurrentGroundItemID != -1)
		{
			if (UGroundItemSubsystem* Subsystem = GetWorld()->GetSubsystem<UGroundItemSubsystem>())
			{
				const TMap<int32, FVector>& Locations = Subsystem->GetInstanceLocations();
				if (const FVector* LocationPtr = Locations.Find(CurrentGroundItemID))
				{
					DebugManager.DrawGroundItem(*LocationPtr, CurrentGroundItemID);
				}
			}
		}

		DebugManager.DisplayInteractionState(InteractableComp, Distance, CurrentGroundItemID);
	}
}

void UInteractionManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// ═══════════════════════════════════════════════
	// FIX: Safe timer cleanup
	// ═══════════════════════════════════════════════
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(InteractionCheckTimer);
		TimerManager.ClearTimer(HoldProgressTimer);
		TimerManager.ClearTimer(PossessionCheckTimer);
	}

	if (CurrentInteractable.GetInterface())
	{
		IInteractable::Execute_OnEndFocus(CurrentInteractable.GetObject(), GetOwner());
	}

	Super::EndPlay(EndPlayReason);
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
	
	if (!GetOwner())
	{
		UE_LOG(LogInteractionManager, Error, TEXT("InteractionManager: Cannot initialize - no owner!"));
		return;
	}
	
	UE_LOG(LogInteractionManager, Log, TEXT("═══════════════════════════════════════════"));
	UE_LOG(LogInteractionManager, Log, TEXT("  INTERACTION MANAGER - Initializing"));
	UE_LOG(LogInteractionManager, Log, TEXT("═══════════════════════════════════════════"));

	InitializeSubManagers();
	ApplyQuickSettings();

	if (bInteractionEnabled)
	{
		GetWorld()->GetTimerManager().SetTimer(
			InteractionCheckTimer,
			this,
			&UInteractionManager::CheckForInteractables,
			TraceManager.CheckFrequency,
			true
		);
	}
	
	bSystemInitialized = true;
	SetComponentTickEnabled(true);

	UE_LOG(LogInteractionManager, Log, TEXT("InteractionManager: ✓ Initialized on %s (Frequency: %.2fs)"), 
		*GetOwner()->GetName(), TraceManager.CheckFrequency);
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

	if (OwnerPawn->IsLocallyControlled())
	{
		UE_LOG(LogInteractionManager, Log, TEXT("InteractionManager: Possession confirmed!"));
		GetWorld()->GetTimerManager().ClearTimer(PossessionCheckTimer);
		InitializeInteractionSystem();
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

	TraceManager.Initialize(Owner, World);
	ValidatorManager.Initialize(Owner, World);
	PickupManager.Initialize(Owner, World);
	DebugManager.Initialize(Owner, World);

	TraceManager.SetDebugManager(&DebugManager);

	UE_LOG(LogInteractionManager, Log, TEXT("InteractionManager: All sub-managers initialized"));
}

void UInteractionManager::ApplyQuickSettings()
{
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
// PRIMARY INTERFACE
// ═══════════════════════════════════════════════════════════════════════

void UInteractionManager::OnInteractPressed()
{
	if (!bInteractionEnabled || !bSystemInitialized)
	{
		return;
	}

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

	if (CurrentGroundItemID != -1)
	{
		PickupManager.StartHoldInteraction(CurrentGroundItemID);
		
		GetWorld()->GetTimerManager().SetTimer(
			HoldProgressTimer,
			this,
			&UInteractionManager::UpdateHoldProgress,
			0.016f,
			true
		);
	}
}

void UInteractionManager::OnInteractReleased()
{
	if (!bInteractionEnabled || !bSystemInitialized)
	{
		return;
	}

	if (PickupManager.IsHoldingForGroundItem())
	{
		float HoldProgress = PickupManager.GetHoldProgress();
		
		GetWorld()->GetTimerManager().ClearTimer(HoldProgressTimer);
		
		if (HoldProgress < 0.3f)
		{
			int32 ItemID = PickupManager.GetCurrentHoldItemID();
			PickupManager.CancelHoldInteraction();
			PickupGroundItemToInventory(ItemID);
		}
		else if (HoldProgress >= 1.0f)
		{
			int32 ItemID = PickupManager.GetCurrentHoldItemID();
			PickupManager.CancelHoldInteraction();
			PickupGroundItemAndEquip(ItemID);
		}
		else
		{
			PickupManager.CancelHoldInteraction();
		}
	}
}

void UInteractionManager::PickupAllNearbyItems()
{
	if (!bInteractionEnabled || !bSystemInitialized)
	{
		return;
	}
	
	int32 PickedUp = PickupManager.PickupAllNearby(GetOwner()->GetActorLocation());
	
	if (bDebugEnabled && PickedUp > 0)
	{
		UE_LOG(LogInteractionManager, Log, TEXT("InteractionManager: Picked up %d nearby items"), PickedUp);
	}
}

void UInteractionManager::CheckForInteractables()
{
	if (!bInteractionEnabled || !bSystemInitialized)
	{
		return;
	}
	
	TScriptInterface<IInteractable> NewInteractable = TraceManager.TraceForActorInteractable();
	
	int32 NewGroundItemID = -1;
	if (!NewInteractable.GetInterface())
	{
		TraceManager.FindNearestGroundItem(NewGroundItemID);
	}
	
	UpdateFocusState(NewInteractable);
	CurrentGroundItemID = NewGroundItemID;
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

void UInteractionManager::InteractWithActor(AActor* TargetActor)
{
	if (!TargetActor)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	FVector ClientLocation = Owner->GetActorLocation();

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
	if (NewInteractable == CurrentInteractable)
	{
		return;
	}
	
	AActor* Owner = GetOwner();
	
	if (CurrentInteractable.GetInterface())
	{
		IInteractable::Execute_OnEndFocus(CurrentInteractable.GetObject(), Owner);
	}
	
	CurrentInteractable = NewInteractable;
	
	if (CurrentInteractable.GetInterface())
	{
		IInteractable::Execute_OnBeginFocus(CurrentInteractable.GetObject(), Owner);
	}
	
	OnCurrentInteractableChanged.Broadcast(GetCurrentInteractable());
}

void UInteractionManager::UpdateHoldProgress()
{
	if (!PickupManager.IsHoldingForGroundItem())
	{
		GetWorld()->GetTimerManager().ClearTimer(HoldProgressTimer);
		return;
	}
	
	PickupManager.UpdateHoldProgress(0.016f);
	
	if (PickupManager.GetHoldProgress() >= 1.0f)
	{
		GetWorld()->GetTimerManager().ClearTimer(HoldProgressTimer);
		
		int32 ItemID = PickupManager.GetCurrentHoldItemID();
		PickupManager.CancelHoldInteraction();
		PickupGroundItemAndEquip(ItemID);
	}
}
