// Character/Component/InteractionManager.cpp

#include "Character/Component/InteractionManager.h"
#include "Character/Component/InventoryManager.h"
#include "Character/Component/EquipmentManager.h"
#include "Interactable/Interface/Interactable.h"
#include "Interactable/Component/InteractableManager.h"
#include "Interactable/Widget/InteractableWidget.h"
#include "Tower/Subsystem/GroundItemSubsystem.h"
#include "Item/ItemInstance.h"
#include "Item/Library/ItemEnums.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "DrawDebugHelpers.h"
#include "Blueprint/UserWidget.h"
#include "Character/ALSPlayerCameraManager.h"
#include "Components/ALSDebugComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"

UInteractionManager::UInteractionManager()

	:ALSDebugComponent(nullptr)
	,GroundItemSubsystem(nullptr)
	,InventoryComponent(nullptr)
	,EquipmentComponent(nullptr)
	,PlayerController(nullptr)
{
	PrimaryComponentTick.bCanEverTick = true; // Enable for hold progress
	PrimaryComponentTick.TickInterval = 0.016f; // ~60fps
	
	// Enable replication
	SetIsReplicatedByDefault(true);
}

void UInteractionManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	// CurrentInteractable is intentionally NOT replicated (client-side prediction)
}

void UInteractionManager::BeginPlay()
{
	Super::BeginPlay();

	// Cache components
	CacheComponents();

	// Get ground item subsystem
	if (GetWorld())
	{
		GroundItemSubsystem = GetWorld()->GetSubsystem<UGroundItemSubsystem>();
	}

	// Only run interaction checks on owning client or listen server
	if (IsLocallyControlled())
	{
		// Create ground item widget (screen space)
		if (bShowWidget && InteractionWidgetClass && bUseScreenSpaceWidget)
		{
			if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			{
				UUserWidget* Widget = CreateWidget<UUserWidget>(PC, InteractionWidgetClass);
				if (Widget)
				{
					Widget->AddToViewport(100); // High Z-order
					Widget->SetVisibility(ESlateVisibility::Hidden);
					
					GroundItemWidget = Cast<UInteractableWidget>(Widget);
					if (!GroundItemWidget)
					{
						UE_LOG(LogTemp, Error, TEXT("InteractionManager: InteractionWidgetClass must inherit from UInteractableWidget!"));
					}
				}
			}
		}

		GetWorld()->GetTimerManager().SetTimer(
			InteractionCheckTimer,
			this,
			&UInteractionManager::CheckForInteractables,
			InteractionCheckFrequency,
			true
		);

		UE_LOG(LogTemp, Log, TEXT("InteractionManager: Initialized on %s (LOCAL)"), *GetOwner()->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("InteractionManager: Initialized on %s (REMOTE - no checks)"), *GetOwner()->GetName());
	}
}

void UInteractionManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clear timer
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(InteractionCheckTimer);
	}

	// Cleanup ground item widget
	if (GroundItemWidget)
	{
		GroundItemWidget->RemoveFromParent();
		GroundItemWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void UInteractionManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Update ground item hold progress
	if (bIsHoldingForGroundItem && CurrentGroundItemID != -1)
	{
		UpdateGroundItemHoldProgress(DeltaTime);
	}
}

bool UInteractionManager::IsLocallyControlled() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	// Check if this is the local player controller's pawn
	if (APawn* OwnerPawn = Cast<APawn>(Owner))
	{
		return OwnerPawn->IsLocallyControlled();
	}

	return false;
}

// ═══════════════════════════════════════════════════════════════════════
// CLIENT-SIDE DETECTION (Local Only)
// ═══════════════════════════════════════════════════════════════════════

void UInteractionManager::CheckForInteractables()
{
	// Only run on locally controlled client
	if (!IsLocallyControlled())
	{
		return;
	}

	FVector CameraLocation;
	FRotator CameraRotation;
	if (!GetCameraViewPoint(CameraLocation, CameraRotation))
	{
		return;
	}

	// PRIORITY 1: Check for actor-based interactables (doors, chests, NPCs)
	TScriptInterface<IInteractable> NewInteractable = TraceForInteractable();

	// PRIORITY 2: Check for ground items (if no actor found)
	int32 NewGroundItemID = -1;
	UItemInstance* GroundItem = nullptr;
	
	if (!NewInteractable.GetInterface() && GroundItemSubsystem)
	{
		GroundItem = GroundItemSubsystem->GetNearestItem(
			CameraLocation,
			InteractionDistance,
			NewGroundItemID
		);
	}

	// Update actor-based interactable (LOCAL UI FEEDBACK ONLY)
	if (NewInteractable != CurrentInteractable)
	{
		// End focus on old
		if (CurrentInteractable.GetInterface())
		{
			IInteractable::Execute_OnEndFocus(CurrentInteractable.GetObject(), GetOwner());
		}

		// Start focus on new
		CurrentInteractable = NewInteractable;
		
		if (CurrentInteractable.GetInterface())
		{
			IInteractable::Execute_OnBeginFocus(CurrentInteractable.GetObject(), GetOwner());
		}

		// Notify listeners (for custom logic)
		OnCurrentInteractableChanged.Broadcast(CurrentInteractable);
	}

	// Update ground item focus
	if (NewGroundItemID != CurrentGroundItemID)
	{
		// Cancel hold if switching items
		if (bIsHoldingForGroundItem && CurrentGroundItemID != -1)
		{
			CancelGroundItemHold();
		}

		CurrentGroundItemID = NewGroundItemID;
		UpdateGroundItemWidget(GroundItem);
	}
}


TScriptInterface<IInteractable> UInteractionManager::TraceForInteractable()
{
	if (!GetWorld())
	{
		return nullptr;
	}
	
	FVector CameraLocation;
	FRotator CameraRotation;
	if (!GetCameraViewPoint(CameraLocation, CameraRotation))
	{
		return nullptr;
	}
	
	const FVector CameraForward = CameraRotation.Vector();
	const FVector TraceEnd = CameraLocation + (CameraForward * InteractionDistance);

	// Stage 1: Fast line trace (early exit)
	FHitResult LineHit;
	if (!PerformLineTrace(CameraLocation, TraceEnd, LineHit))
	{
		return nullptr; // Nothing hit
	}

	// Stage 2: Gather all candidates via sphere trace
	TArray<FHitResult> SphereHits = PerformSphereTrace(CameraLocation, TraceEnd);
	
	if (SphereHits.Num() == 0)
	{
		// Fall back to line hit if sphere found nothing
		AActor* LineHitActor = LineHit.GetActor();
		UInteractableManager* Comp = LineHitActor ? LineHitActor->FindComponentByClass<UInteractableManager>() : nullptr;
		bool bImplements = LineHitActor && LineHitActor->GetClass()->ImplementsInterface(UInteractable::StaticClass());
		return CreateInteractableFromActor(LineHitActor, Comp, bImplements);
	}

	// Stage 3: Score and select best candidate
	TArray<FInteractableCandidate> Candidates = GatherCandidates(SphereHits, CameraLocation, CameraForward);
	TScriptInterface<IInteractable> Winner = SelectBestCandidate(Candidates);

	// Debug visualization
	DebugDrawWinner(Winner, CameraLocation, Candidates.Num());

	return Winner;
}

bool UInteractionManager::PerformLineTrace(FVector CameraLocation, FVector TraceEnd, FHitResult& OutHit)
{
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());
	QueryParams.bTraceComplex = false;

	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		OutHit,
		CameraLocation,
		TraceEnd,
		InteractionTraceChannel,
		QueryParams
	);

	DebugDrawLineTrace(CameraLocation, TraceEnd, bHit);
	
	return bHit;
}

TArray<FHitResult> UInteractionManager::PerformSphereTrace(FVector CameraLocation, FVector TraceEnd)
{
	TArray<FHitResult> Hits;
	
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());
	QueryParams.bTraceComplex = false;

	

	// Sweep sphere along the entire trace line
	GetWorld()->SweepMultiByChannel(
		Hits,
		CameraLocation,
		TraceEnd,
		FQuat::Identity,
		InteractionTraceChannel,
		FCollisionShape::MakeSphere(InteractionSphereRadius),
		QueryParams
	);

	// Debug draw sphere sweep along the line
	DebugDrawSphereTrace(CameraLocation, TraceEnd);
	
	return Hits;
}

TArray<UInteractionManager::FInteractableCandidate> UInteractionManager::GatherCandidates(
	const TArray<FHitResult>& Hits, 
	FVector CameraLocation, 
	FVector CameraForward)
{
	TArray<FInteractableCandidate> Candidates;
	Candidates.Reserve(Hits.Num());

	for (const FHitResult& Hit : Hits)
	{
		FInteractableCandidate Candidate = CreateCandidate(Hit, CameraLocation, CameraForward);
		
		if (!Candidate.Interactable.GetInterface())
		{
			continue; // Not interactable
		}

		if (!IsInFrontOfCamera(Candidate.DotProduct))
		{
			continue; // Behind camera
		}

		if (!IsInFrontOfPlayer(Candidate.Actor, Hit.ImpactPoint))
		{
			continue; // Behind player character
		}

		Candidates.Add(Candidate);
		DebugDrawCandidate(Candidate);
	}

	return Candidates;
}

UInteractionManager::FInteractableCandidate UInteractionManager::CreateCandidate(
	const FHitResult& Hit, 
	FVector CameraLocation, 
	FVector CameraForward)
{
	FInteractableCandidate Candidate;
	Candidate.Actor = Hit.GetActor();
	Candidate.ImpactPoint = Hit.ImpactPoint;

	if (!Candidate.Actor)
	{
		return Candidate;
	}

	// Check for InteractableManager component first
	UInteractableManager* InteractableComp = Candidate.Actor->FindComponentByClass<UInteractableManager>();
	bool bActorImplements = Candidate.Actor->GetClass()->ImplementsInterface(UInteractable::StaticClass());

	Candidate.Interactable = CreateInteractableFromActor(Candidate.Actor, InteractableComp, bActorImplements);

	if (!Candidate.Interactable.GetInterface())
	{
		return Candidate; // Not interactable
	}

	// Calculate metrics
	const FVector ToTarget = Candidate.ImpactPoint - CameraLocation;
	Candidate.DistanceSq = ToTarget.SizeSquared();
	Candidate.DotProduct = FVector::DotProduct(CameraForward, ToTarget.GetSafeNormal());

	// Calculate composite score
	Candidate.Score = CalculateInteractionScore(Candidate.DistanceSq, Candidate.DotProduct);

	return Candidate;
}

float UInteractionManager::CalculateInteractionScore(float DistanceSq, float DotProduct) const
{
	// Normalize metrics to [0, 1]
	const float MaxDistanceSq = FMath::Square(InteractionDistance);
	const float NormalizedDistance = 1.0f - FMath::Clamp(DistanceSq / MaxDistanceSq, 0.0f, 1.0f);
	const float NormalizedDot = (DotProduct + 1.0f) * 0.5f; // [-1, 1] -> [0, 1]

	// Weighted combination
	return (NormalizedDot * DotProductWeight) + (NormalizedDistance * DistanceWeight);
}

bool UInteractionManager::IsInFrontOfCamera(float DotProduct) const
{
	return DotProduct > 0.0f;
}

bool UInteractionManager::IsInFrontOfPlayer(AActor* TargetActor, FVector TargetLocation) const
{
	AActor* Owner = GetOwner();
	if (!Owner || !TargetActor)
	{
		return true; // Default to true if can't determine
	}

	const FVector OwnerForward = Owner->GetActorForwardVector();
	const FVector ToTarget = (TargetLocation - Owner->GetActorLocation()).GetSafeNormal();
	const float Dot = FVector::DotProduct(OwnerForward, ToTarget);

	return Dot > -0.5f; // Allow items slightly behind (270 degree view)
}

TScriptInterface<IInteractable> UInteractionManager::SelectBestCandidate(const TArray<FInteractableCandidate>& Candidates)
{
	if (Candidates.Num() == 0)
	{
		return nullptr;
	}

	// Find highest score
	const FInteractableCandidate* BestCandidate = &Candidates[0];
	for (const FInteractableCandidate& Candidate : Candidates)
	{
		if (Candidate.Score > BestCandidate->Score)
		{
			BestCandidate = &Candidate;
		}
	}

	return BestCandidate->Interactable;
}

void UInteractionManager::DebugDrawLineTrace(FVector Start, FVector End, bool bHit) const
{
#if !UE_BUILD_SHIPPING
	if (!ShouldShowDebugTraces()) return;

	DrawDebugLine(
		GetWorld(),
		Start,
		End,
		bHit ? FColor::Green : FColor::Red,
		false,
		InteractionCheckFrequency,
		0,
		2.0f
	);
#endif
}

void UInteractionManager::DebugDrawSphereTrace(FVector Start, FVector End) const
{
#if !UE_BUILD_SHIPPING
	if (!ShouldShowDebugTraces()) return;

	// Draw sphere at start
	DrawDebugSphere(
		GetWorld(),
		Start,
		InteractionSphereRadius,
		12,
		FColor::Cyan,
		false,
		InteractionCheckFrequency,
		0,
		1.0f
	);

	// Draw sphere at end
	DrawDebugSphere(
		GetWorld(),
		End,
		InteractionSphereRadius,
		12,
		FColor::Cyan,
		false,
		InteractionCheckFrequency,
		0,
		1.0f
	);

	// Draw line connecting them to show the sweep path
	DrawDebugLine(
		GetWorld(),
		Start,
		End,
		FColor::Cyan,
		false,
		InteractionCheckFrequency,
		0,
		1.0f
	);
#endif
}

void UInteractionManager::DebugDrawCandidate(const FInteractableCandidate& Candidate) const
{
#if !UE_BUILD_SHIPPING
	if (!ShouldShowDebugTraces() || !Candidate.Interactable.GetInterface()) return;

	// Color by score
	FColor DebugColor = FColor::MakeRedToGreenColorFromScalar(Candidate.Score);
	DrawDebugPoint(GetWorld(), Candidate.ImpactPoint, 12.0f, DebugColor, false, InteractionCheckFrequency);
	
	// Show score text
	const float MaxDistanceSq = FMath::Square(InteractionDistance);
	const float NormalizedDistance = 1.0f - FMath::Clamp(Candidate.DistanceSq / MaxDistanceSq, 0.0f, 1.0f);
	const float NormalizedDot = (Candidate.DotProduct + 1.0f) * 0.5f;
	
	DrawDebugString(
		GetWorld(),
		Candidate.ImpactPoint + FVector(0, 0, 20),
		FString::Printf(TEXT("%.2f (D:%.2f A:%.2f)"), Candidate.Score, NormalizedDistance, NormalizedDot),
		nullptr,
		FColor::White,
		InteractionCheckFrequency,
		false
	);
#endif
}

void UInteractionManager::DebugDrawWinner(
	const TScriptInterface<IInteractable>& Winner, 
	FVector CameraLocation, 
	int32 CandidateCount) const
{
#if !UE_BUILD_SHIPPING
	if (!ShouldShowDebugTraces() || !Winner.GetInterface()) return;

	AActor* WinnerActor = Cast<AActor>(Winner.GetObject());
	if (!WinnerActor)
	{
		UActorComponent* Component = Cast<UActorComponent>(Winner.GetObject());
		if (Component)
		{
			WinnerActor = Component->GetOwner();
		}
	}

	if (!WinnerActor) return;

	// Yellow sphere around winner
	DrawDebugSphere(
		GetWorld(), 
		WinnerActor->GetActorLocation(), 
		30.0f, 
		12, 
		FColor::Yellow, 
		false, 
		InteractionCheckFrequency, 
		0, 
		3.0f
	);
	
	// Green line to winner
	DrawDebugLine(
		GetWorld(),
		CameraLocation,
		WinnerActor->GetActorLocation(),
		FColor::Green,
		false,
		InteractionCheckFrequency,
		0,
		4.0f
	);
	
	UE_LOG(LogTemp, Verbose, TEXT("Selected: %s (Candidates: %d)"), 
		*WinnerActor->GetName(), CandidateCount);
#endif
}

TScriptInterface<IInteractable> UInteractionManager::CreateInteractableFromActor(
	AActor* Actor, 
	UInteractableManager* Component, 
	bool bActorImplements)
{
	if (!Actor)
	{
		return nullptr;
	}

	TScriptInterface<IInteractable> Interactable;
	
	if (Component)
	{
		Interactable.SetObject(Component);
		Interactable.SetInterface(Cast<IInteractable>(Component));
	}
	else if (bActorImplements)
	{
		Interactable.SetObject(Actor);
		Interactable.SetInterface(Cast<IInteractable>(Actor));
	}

	if (!Interactable.GetInterface())
	{
		return nullptr;
	}

	if (!IInteractable::Execute_CanInteract(Interactable.GetObject(), GetOwner()))
	{
		return nullptr;
	}

	return Interactable;
}

// ═══════════════════════════════════════════════════════════════════════
// INPUT HANDLING
// ═══════════════════════════════════════════════════════════════════════

void UInteractionManager::OnInteractPressed()
{
	// Priority 1: Actor-based interactable
	if (CurrentInteractable.GetInterface())
	{
		AActor* TargetActor = Cast<AActor>(CurrentInteractable.GetObject());
		if (!TargetActor)
		{
			UActorComponent* Component = Cast<UActorComponent>(CurrentInteractable.GetObject());
			if (Component) TargetActor = Component->GetOwner();
		}

		if (TargetActor)
		{
			ServerInteractWithActor(TargetActor, GetOwner()->GetActorLocation());
		}
		return;
	}

	// Priority 2: Ground item - start hold timer
	if (CurrentGroundItemID != -1 && bEnableDirectEquip)
	{
		bIsHoldingForGroundItem = true;
		GroundItemHoldProgress = 0.0f;

		// Update widget for hold state
		if (GroundItemWidget && GroundItemSubsystem)
		{
			UItemInstance* Item = GroundItemSubsystem->GetGroundItems().FindRef(CurrentGroundItemID);
			UpdateGroundItemWidgetForHold(Item);
		}

		UE_LOG(LogTemp, Log, TEXT("Started holding for ground item %d"), CurrentGroundItemID);
	}
}

void UInteractionManager::OnInteractReleased()
{
	// If holding for ground item
	if (bIsHoldingForGroundItem)
	{
		const bool bHeldLongEnough = (GroundItemHoldProgress >= 1.0f);
		const int32 ItemID = CurrentGroundItemID;
		
		// Reset state
		bIsHoldingForGroundItem = false;
		GroundItemHoldProgress = 0.0f;

		// Hide progress bar
		if (GroundItemWidget)
		{
			GroundItemWidget->SetProgress(0.0f, false);
			GroundItemWidget->SetProgressBarVisible(false);

			// Restore normal text
			if (GroundItemSubsystem)
			{
				UItemInstance* Item = GroundItemSubsystem->GetGroundItems().FindRef(ItemID);
				UpdateGroundItemWidgetNormal(Item);
			}
		}

		if (bHeldLongEnough)
		{
			// HELD: Equip directly
			ServerPickupAndEquipGroundItem(ItemID, GetOwner()->GetActorLocation());
			UE_LOG(LogTemp, Log, TEXT("Ground item held - equipping directly"));
		}
		else
		{
			// TAPPED: Add to inventory
			ServerPickupGroundItem(ItemID, GetOwner()->GetActorLocation());
			UE_LOG(LogTemp, Log, TEXT("Ground item tapped - adding to inventory"));
		}
		
		return;
	}
}

void UInteractionManager::PickupAllNearbyItems()
{
	FVector ClientLocation = GetOwner()->GetActorLocation();
	ServerPickupNearbyItems(ClientLocation);
}

// ═══════════════════════════════════════════════════════════════════════
// GROUND ITEM HELPERS
// ═══════════════════════════════════════════════════════════════════════

void UInteractionManager::UpdateGroundItemHoldProgress(float DeltaTime)
{
	// Verify item still exists
	if (GroundItemSubsystem && !GroundItemSubsystem->GetGroundItems().Contains(CurrentGroundItemID))
	{
		CancelGroundItemHold();
		UE_LOG(LogTemp, Warning, TEXT("Ground item disappeared while holding"));
		return;
	}

	// Increment progress
	GroundItemHoldProgress += DeltaTime / GroundItemHoldDuration;
	GroundItemHoldProgress = FMath::Clamp(GroundItemHoldProgress, 0.0f, 1.0f);

	// Update widget
	if (GroundItemWidget)
	{
		GroundItemWidget->SetProgress(GroundItemHoldProgress, false);
		GroundItemWidget->SetProgressBarVisible(true);
	}
}

void UInteractionManager::UpdateGroundItemWidget(UItemInstance* Item)
{
	if (!GroundItemWidget) return;

	if (Item && CurrentGroundItemID != -1)
	{
		UpdateGroundItemWidgetNormal(Item);
		GroundItemWidget->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		GroundItemWidget->SetVisibility(ESlateVisibility::Hidden);
		CurrentGroundItemID = -1;
	}
}

void UInteractionManager::UpdateGroundItemWidgetNormal(UItemInstance* Item)
{
	if (!GroundItemWidget || !Item) return;

	FText InteractionText;
	if (bEnableDirectEquip && bShowEquipHint)
	{
		InteractionText = FText::Format(
			FText::FromString("Pick Up {0}\nHold to Equip"),
			Item->GetDisplayName()
		);
	}
	else
	{
		InteractionText = FText::Format(
			FText::FromString("Pick Up {0}"),
			Item->GetDisplayName()
		);
	}

	GroundItemWidget->SetInteractionData(FName("PickUp"), InteractionText);
}

void UInteractionManager::UpdateGroundItemWidgetForHold(UItemInstance* Item)
{
	if (!GroundItemWidget || !Item) return;

	FText InteractionText = FText::Format(
		FText::FromString("Equipping {0}..."),
		Item->GetDisplayName()
	);

	GroundItemWidget->SetInteractionData(FName("Equip"), InteractionText);
}

void UInteractionManager::CancelGroundItemHold()
{
	bIsHoldingForGroundItem = false;
	GroundItemHoldProgress = 0.0f;

	if (GroundItemWidget)
	{
		GroundItemWidget->SetProgress(0.0f, false);
		GroundItemWidget->SetProgressBarVisible(false);

		// Restore normal text
		if (GroundItemSubsystem && CurrentGroundItemID != -1)
		{
			UItemInstance* Item = GroundItemSubsystem->GetGroundItems().FindRef(CurrentGroundItemID);
			UpdateGroundItemWidgetNormal(Item);
		}
	}
}

EEquipmentSlot UInteractionManager::DetermineEquipmentSlot(UItemInstance* Item) const
{
	if (!Item) return EEquipmentSlot::ES_None;

	EItemType ItemType = Item->GetItemType();
	EItemSubType SubType = Item->GetItemSubType();

	switch (ItemType)
	{
		case EItemType::IT_Weapon:
			return Item->bIsTwoHanded() ? EEquipmentSlot::ES_TwoHand : EEquipmentSlot::ES_MainHand;

		case EItemType::IT_Armor:
			switch (SubType)
			{
				case EItemSubType::IST_Helmet: return EEquipmentSlot::ES_Head;
				case EItemSubType::IST_Chest: return EEquipmentSlot::ES_Chest;
				case EItemSubType::IST_Gloves: return EEquipmentSlot::ES_Hands;
				case EItemSubType::IST_Boots: return EEquipmentSlot::ES_Feet;
				case EItemSubType::IST_Legs: return EEquipmentSlot::ES_Legs;
				default: break;
			}
			break;

		case EItemType::IT_Accessory:
			switch (SubType)
			{
				case EItemSubType::IST_Ring: return EEquipmentSlot::ES_Ring1; // TODO: Find empty ring slot
				case EItemSubType::IST_Amulet: return EEquipmentSlot::ES_Amulet;
				case EItemSubType::IST_Belt: return EEquipmentSlot::ES_Belt;
				default: break;
			}
			break;

		default:
			break;
	}

	return EEquipmentSlot::ES_None;
}

// ═══════════════════════════════════════════════════════════════════════
// SERVER RPCs (Validated and executed on server)
// ═══════════════════════════════════════════════════════════════════════

void UInteractionManager::ServerInteractWithActor_Implementation(AActor* TargetActor, FVector ClientLocation)
{
	// Server validation
	if (!ValidateInteraction(TargetActor, ClientLocation))
	{
		UE_LOG(LogTemp, Warning, TEXT("Server: Rejected interaction from %s with %s (validation failed)"), 
			*GetOwner()->GetName(), 
			TargetActor ? *TargetActor->GetName() : TEXT("NULL"));
		return;
	}

	// Execute interaction (server has authority)
	UInteractableManager* InteractableComp = TargetActor->FindComponentByClass<UInteractableManager>();
	if (InteractableComp)
	{
		IInteractable::Execute_OnInteract(InteractableComp, GetOwner());
		UE_LOG(LogTemp, Log, TEXT("Server: Executed interaction from %s with %s"), 
			*GetOwner()->GetName(), *TargetActor->GetName());
	}
	else if (TargetActor->GetClass()->ImplementsInterface(UInteractable::StaticClass()))
	{
		IInteractable::Execute_OnInteract(TargetActor, GetOwner());
		UE_LOG(LogTemp, Log, TEXT("Server: Executed interaction from %s with %s"), 
			*GetOwner()->GetName(), *TargetActor->GetName());
	}
}

void UInteractionManager::ServerPickupGroundItem_Implementation(int32 ItemID, FVector ClientLocation)
{
	if (!GroundItemSubsystem || !InventoryComponent)
	{
		return;
	}

	if (!ValidateGroundItemDistance(ItemID, ClientLocation))
	{
		return;
	}

	// Get item location before removing
	const FVector* ItemLocation = GroundItemSubsystem->GetInstanceLocations().Find(ItemID);
	if (!ItemLocation)
	{
		return;
	}

	const FVector SavedLocation = *ItemLocation;

	// Remove from ground
	UItemInstance* Item = GroundItemSubsystem->RemoveItemFromGround(ItemID);
	if (!Item)
	{
		return;
	}

	// Add to inventory
	if (InventoryComponent->AddItem(Item))
	{
		UE_LOG(LogTemp, Log, TEXT("Server: Picked up ground item %d to inventory"), ItemID);
	}
	else
	{
		// Failed to add - put back on ground
		GroundItemSubsystem->AddItemToGround(Item, SavedLocation);
		UE_LOG(LogTemp, Warning, TEXT("Server: Inventory full, item returned to ground"));
	}
}

void UInteractionManager::ServerPickupAndEquipGroundItem_Implementation(int32 ItemID, FVector ClientLocation)
{
	if (!GroundItemSubsystem || !EquipmentComponent)
	{
		return;
	}

	if (!ValidateGroundItemDistance(ItemID, ClientLocation))
	{
		return;
	}

	// Get item location before removing
	const FVector* ItemLocation = GroundItemSubsystem->GetInstanceLocations().Find(ItemID);
	if (!ItemLocation)
	{
		return;
	}

	const FVector SavedLocation = *ItemLocation;

	// Remove from ground
	UItemInstance* Item = GroundItemSubsystem->RemoveItemFromGround(ItemID);
	if (!Item)
	{
		return;
	}

	// Determine equipment slot
	EEquipmentSlot TargetSlot = DetermineEquipmentSlot(Item);
	if (TargetSlot == EEquipmentSlot::ES_None)
	{
		// Item can't be equipped - add to inventory instead
		if (InventoryComponent)
		{
			if (InventoryComponent->AddItem(Item))
			{
				UE_LOG(LogTemp, Log, TEXT("Server: Item can't be equipped, added to inventory"));
			}
			else
			{
				// Inventory full - return to ground
				GroundItemSubsystem->AddItemToGround(Item, SavedLocation);
			}
		}
		return;
	}

	// Try to equip directly
	UItemInstance* UnequippedItem = nullptr; // EquipmentComponent->EquipItem(Item, TargetSlot);
	
	// If something was unequipped, add to inventory
	if (UnequippedItem && InventoryComponent)
	{
		if (!InventoryComponent->AddItem(UnequippedItem))
		{
			// Inventory full - drop unequipped item on ground
			GroundItemSubsystem->AddItemToGround(UnequippedItem, SavedLocation);
			UE_LOG(LogTemp, Warning, TEXT("Server: Inventory full, dropped unequipped item"));
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Server: Equipped ground item %d directly"), ItemID);
}

void UInteractionManager::ServerPickupNearbyItems_Implementation(FVector ClientLocation)
{
	// Server-side pickup all with validation
	if (!GroundItemSubsystem || !InventoryComponent)
	{
		return;
	}

	// Validate client isn't too far from reported location (anti-cheat)
	FVector ServerLocation = GetOwner()->GetActorLocation();
	float DistanceFromReported = FVector::Distance(ServerLocation, ClientLocation);
	
	if (DistanceFromReported > ServerValidationBuffer)
	{
		UE_LOG(LogTemp, Warning, TEXT("Server: Rejected pickup all from %s (position mismatch: %.1f units)"), 
			*GetOwner()->GetName(), DistanceFromReported);
		return;
	}

	// Use server's actual position for pickup
	TArray<UItemInstance*> Items = GroundItemSubsystem->GetItemsInRadius(
		ServerLocation,
		PickupAllRadius
	);

	int32 PickedUp = 0;
	
	for (UItemInstance* Item : Items)
	{
		int32 ItemID = -1;
		for (const TPair<int32, UItemInstance*>& Pair : GroundItemSubsystem->GetGroundItems())
		{
			if (Pair.Value == Item)
			{
				ItemID = Pair.Key;
				break;
			}
		}

		if (ItemID == -1)
		{
			continue;
		}

		if (InventoryComponent->AddItem(Item))
		{
			GroundItemSubsystem->RemoveItemFromGround(ItemID);
			PickedUp++;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Server: Picked up %d items for %s"), PickedUp, *GetOwner()->GetName());
}

// ═══════════════════════════════════════════════════════════════════════
// VALIDATION (Server-side)
// ═══════════════════════════════════════════════════════════════════════

bool UInteractionManager::ValidateInteraction(AActor* TargetActor, FVector ClientLocation) const
{
	if (!TargetActor)
	{
		return false;
	}

	// Validate distance
	FVector TargetLocation = TargetActor->GetActorLocation();
	float Distance = FVector::Distance(ClientLocation, TargetLocation);
	
	if (Distance > InteractionDistance + ServerValidationBuffer)
	{
		UE_LOG(LogTemp, Warning, TEXT("Server: Interaction too far (%.1f > %.1f)"), 
			Distance, InteractionDistance + ServerValidationBuffer);
		return false;
	}

	// Validate interactable
	if (!IsValidInteractable(TargetActor, GetOwner()))
	{
		return false;
	}

	return true;
}

bool UInteractionManager::ValidateGroundItemDistance(int32 ItemID, FVector ClientLocation) const
{
	if (!GroundItemSubsystem)
	{
		return false;
	}

	const FVector* ItemLocation = GroundItemSubsystem->GetInstanceLocations().Find(ItemID);
	if (!ItemLocation)
	{
		UE_LOG(LogTemp, Warning, TEXT("Server: Ground item %d not found"), ItemID);
		return false;
	}

	const float Distance = FVector::Distance(ClientLocation, *ItemLocation);
	if (Distance > InteractionDistance + ServerValidationBuffer)
	{
		UE_LOG(LogTemp, Warning, TEXT("Server: Ground item interaction too far (%.1f > %.1f)"),
			Distance, InteractionDistance + ServerValidationBuffer);
		return false;
	}

	return true;
}

bool UInteractionManager::IsValidInteractable(AActor* Actor, AActor* Interactor) const
{
	if (!Actor)
	{
		return false;
	}

	// Check component
	UInteractableManager* InteractableComp = Actor->FindComponentByClass<UInteractableManager>();
	if (InteractableComp)
	{
		return IInteractable::Execute_CanInteract(InteractableComp, Interactor);
	}

	// Check actor interface
	if (Actor->GetClass()->ImplementsInterface(UInteractable::StaticClass()))
	{
		return IInteractable::Execute_CanInteract(Actor, Interactor);
	}

	return false;
}

// ═══════════════════════════════════════════════════════════════════════
// HELPER FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════

void UInteractionManager::CacheComponents()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Cache player controller
	if (APawn* OwnerPawn = Cast<APawn>(Owner))
	{
		PlayerController = Cast<APlayerController>(OwnerPawn->GetController());
		
		// Cache ALS camera manager if available
		if (PlayerController && PlayerController->PlayerCameraManager)
		{
			ALSCameraManager = Cast<AALSPlayerCameraManager>(PlayerController->PlayerCameraManager);
			if (ALSCameraManager)
			{
				UE_LOG(LogTemp, Log, TEXT("InteractionManager: Found ALS Camera Manager - using enhanced trace origin"));
			}
		}
	}

	// Cache inventory component
	InventoryComponent = Owner->FindComponentByClass<UInventoryManager>();
	if (!InventoryComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("InteractionManager: No InventoryManager found on %s"), *Owner->GetName());
	}

	// Cache equipment component
	EquipmentComponent = Owner->FindComponentByClass<UEquipmentManager>();
	if (!EquipmentComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("InteractionManager: No EquipmentManager found on %s"), *Owner->GetName());
	}

	// Cache ALS debug component (optional)
	ALSDebugComponent = Owner->FindComponentByClass<UALSDebugComponent>();
}
bool UInteractionManager::GetCameraViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	if (!PlayerController)
	{
		if (AActor* Owner = GetOwner())
		{
			OutLocation = Owner->GetActorLocation();
			OutRotation = Owner->GetActorRotation();
			return true;
		}
		return false;
	}

	// Get camera rotation
	FVector RawCameraLoc;
	PlayerController->GetPlayerViewPoint(RawCameraLoc, OutRotation);
    
	// Calculate EXACTLY like ALS does
	AActor* Owner = GetOwner();
	if (Owner)
	{
		FVector PivotLocation = Owner->GetActorLocation();
        
		// ALS's formula
		OutLocation = PivotLocation
			+ UKismetMathLibrary::GetForwardVector(OutRotation) * InteractionOffsetX
			+ UKismetMathLibrary::GetRightVector(OutRotation) * InteractionOffsetY
			+ UKismetMathLibrary::GetUpVector(OutRotation) * InteractionOffsetZ;
        
		return true;
	}
    
	OutLocation = RawCameraLoc;
	return true;
}
FVector UInteractionManager::GetTraceStartLocation(const FVector& CameraLocation, const FRotator& CameraRotation) const
{
	// If not using ALS camera or ALS camera not available, use standard camera location
	if (!bUseALSCamera || !ALSCameraManager)
	{
		return CameraLocation;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return CameraLocation;
	}

	// ═══════════════════════════════════════════════════════════════════════
	// ALS-AWARE TRACE START CALCULATION
	// ═══════════════════════════════════════════════════════════════════════
	
	// In third-person, the camera can be very far from the character
	// Players expect interactions to work from the "crosshair" (center of screen)
	// This is NOT the camera location, but rather a point along the camera's forward vector
	
	FVector CharacterLocation = Owner->GetActorLocation();
	FVector ToCameraVector = CameraLocation - CharacterLocation;
	float CameraDistance = ToCameraVector.Size();

	// If camera is close (first-person or tight third-person), use camera location directly
	if (CameraDistance < MaxCameraDistanceForTrace)
	{
		return CameraLocation;
	}

	// Camera is far from character - pull trace start closer to character
	// This matches player expectation: they interact with what's in the center of their screen,
	// not what's near the camera floating 10 meters behind them
	
	FVector AdjustedStart = CharacterLocation + ToCameraVector.GetSafeNormal() * MaxCameraDistanceForTrace;

	// Alternative: Use character's eye height as minimum trace start
	// This feels more natural than tracing from way behind the character
	FVector EyeLocation = CharacterLocation + FVector(0, 0, Owner->GetSimpleCollisionHalfHeight() * 0.9f);
	
	// Blend between adjusted position and eye position based on camera angle
	FVector CameraForward = CameraRotation.Vector();
	FVector ToEye = (EyeLocation - AdjustedStart).GetSafeNormal();
	float DotToEye = FVector::DotProduct(CameraForward, ToEye);
	
	// If camera is looking away from character, prefer eye location
	if (DotToEye < 0.0f)
	{
		return EyeLocation;
	}
	// Otherwise use adjusted start (camera direction, but closer to character)
	return AdjustedStart;
}

bool UInteractionManager::ShouldShowDebugTraces() const
{
#if !UE_BUILD_SHIPPING
	// Use ALS debug component if available
	if (ALSDebugComponent)
	{
		return ALSDebugComponent->GetShowTraces(); 
	}

	// Fallback: Check console variable or always show in dev builds
	return false; // Set to true to always show debug
#else
	return false;
#endif
}
