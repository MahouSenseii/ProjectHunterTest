// Interactable/Component/InteractableManager.cpp

#include "Interactable/Component/InteractableManager.h"
#include "Interactable/Widget/InteractableWidget.h"
#include "Components/PrimitiveComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"

UInteractableManager::UInteractableManager()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UInteractableManager::BeginPlay()
{
	Super::BeginPlay();

	// Auto-find meshes if none specified
	if (MeshesToHighlight.Num() == 0)
	{
		AutoFindMeshes();
	}

	// Create widget component
	if (bShowWidget && InteractionWidgetClass)
	{
		CreateWidgetComponent();
	}
}

void UInteractableManager::CreateWidgetComponent()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		UE_LOG(LogTemp, Error, TEXT("InteractableManager: No owner actor!"));
		return;
	}

	// Create widget component
	WidgetComponent = NewObject<UWidgetComponent>(Owner, UWidgetComponent::StaticClass(), TEXT("InteractionWidget"));
	if (!WidgetComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("InteractableManager: Failed to create WidgetComponent!"));
		return;
	}

	// Register component FIRST
	WidgetComponent->RegisterComponent();
	
	// ═══════════════════════════════════════════════════════════
	// CRITICAL QUALITY SETTINGS (Set BEFORE widget class!)
	// ═══════════════════════════════════════════════════════════
	
	// Set world space
	WidgetComponent->SetWidgetSpace(EWidgetSpace::World);
	
	// Geometry settings BEFORE widget creation
	WidgetComponent->SetGeometryMode(EWidgetGeometryMode::Plane);
	WidgetComponent->SetBlendMode(EWidgetBlendMode::Transparent);
	WidgetComponent->SetTwoSided(false);
	
	// Enable ticking
	WidgetComponent->SetTickMode(ETickMode::Enabled);
	WidgetComponent->SetWindowFocusable(false);
	
	// Set pivot BEFORE setting draw size
	WidgetComponent->SetPivot(FVector2D(0.5f, 1.0f)); // Bottom-center
	
	// ═══════════════════════════════════════════════════════════
	// RESOLUTION SETTINGS
	// ═══════════════════════════════════════════════════════════
	
	if (bUseDesiredSize)
	{
		// METHOD 1: Use desired size (BEST for quality)
		WidgetComponent->SetDrawSize(WidgetDrawSize); // Physical world size
		WidgetComponent->SetDrawAtDesiredSize(true);  // Render at widget's native resolution
	}
	else
	{
		// METHOD 2: Manual high-resolution render target
		FVector2D HighResSize = WidgetDrawSize * ResolutionScale;
		WidgetComponent->SetDrawSize(HighResSize);
		WidgetComponent->SetDrawAtDesiredSize(false);
	}
	
	// ═══════════════════════════════════════════════════════════
	// SET WIDGET CLASS AND INITIALIZE
	// ═══════════════════════════════════════════════════════════
	
	// NOW set the widget class (after all quality settings)
	WidgetComponent->SetWidgetClass(InteractionWidgetClass);
	
	// Set position
	WidgetComponent->SetRelativeLocation(WidgetOffset);
	
	// Attach to root
	WidgetComponent->AttachToComponent(
		Owner->GetRootComponent(),
		FAttachmentTransformRules::KeepRelativeTransform
	);
	
	// CRITICAL: Initialize widget to apply all settings
	WidgetComponent->InitWidget();
	
	// Force redraw to ensure quality settings are applied
	WidgetComponent->RequestRedraw();
	
	// ═══════════════════════════════════════════════════════════
	// FINAL SETTINGS
	// ═══════════════════════════════════════════════════════════
	
	// Background
	WidgetComponent->SetBackgroundColor(FLinearColor::Transparent);
	
	// Hidden by default
	WidgetComponent->SetVisibility(false);

	UE_LOG(LogTemp, Log, TEXT("InteractableManager: Created high-quality widget for %s (Type: %s)"), 
		*Owner->GetName(),
		*UEnum::GetValueAsString(Config.InteractionType));
}

void UInteractableManager::AutoFindMeshes()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Get all primitive components
	TArray<UPrimitiveComponent*> Primitives;
	Owner->GetComponents<UPrimitiveComponent>(Primitives);

	for (UPrimitiveComponent* Primitive : Primitives)
	{
		// Skip collision components
		if (Primitive->GetName().Contains(TEXT("Collision")) || 
		    Primitive->GetName().Contains(TEXT("Trigger")))
		{
			continue;
		}

		MeshesToHighlight.Add(Primitive);
	}

	UE_LOG(LogTemp, Log, TEXT("InteractableManager: Auto-found %d meshes on %s"), 
		MeshesToHighlight.Num(), *Owner->GetName());
}

// ═══════════════════════════════════════════════════════════════════════
// INTERACTABLE INTERFACE - BASIC
// ═══════════════════════════════════════════════════════════════════════

void UInteractableManager::OnInteract_Implementation(AActor* Interactor)
{
	// For Tap or simple interactions
	switch (Config.InteractionType)
	{
	case EInteractionType::IT_Tap:
		OnTapInteracted.Broadcast(Interactor);
		UE_LOG(LogTemp, Log, TEXT("InteractableManager: Tap interact on %s"), *GetOwner()->GetName());
		break;
		
	case EInteractionType::IT_Toggle:
		OnTapInteracted.Broadcast(Interactor);
		UE_LOG(LogTemp, Log, TEXT("InteractableManager: Toggle interact on %s"), *GetOwner()->GetName());
		break;
		
	default:
		// Hold/Mash interactions handled by their own functions
		UE_LOG(LogTemp, Warning, TEXT("InteractableManager: OnInteract called on non-tap interaction type"));
		break;
	}
}

bool UInteractableManager::CanInteract_Implementation(AActor* Interactor) const
{
	return Config.bCanInteract;
}

void UInteractableManager::OnBeginFocus_Implementation(AActor* Interactor)
{
	// Apply highlight
	if (bEnableHighlight)
	{
		ApplyHighlight(true);
	}

	// Show widget
	if (WidgetComponent)
	{
		WidgetComponent->SetVisibility(true);

		// Update widget with interaction-type-specific text
		if (UInteractableWidget* Widget = Cast<UInteractableWidget>(WidgetComponent->GetWidget()))
		{
			UpdateWidgetText();
			
			// CRITICAL: Force widget update to ensure crisp rendering
			WidgetComponent->RequestRedraw();
		}
	}

	// Broadcast event
	OnFocusBegin.Broadcast(Interactor);

	UE_LOG(LogTemp, Verbose, TEXT("InteractableManager: Begin focus on %s (Type: %s)"), 
		*GetOwner()->GetName(), *UEnum::GetValueAsString(Config.InteractionType));
}

void UInteractableManager::OnEndFocus_Implementation(AActor* Interactor)
{
	// Remove highlight
	if (bEnableHighlight)
	{
		ApplyHighlight(false);
	}

	// Hide widget
	if (WidgetComponent)
	{
		WidgetComponent->SetVisibility(false);
	}

	// Broadcast event
	OnFocusEnd.Broadcast(Interactor);

	UE_LOG(LogTemp, Verbose, TEXT("InteractableManager: End focus on %s"), *GetOwner()->GetName());
}

EInteractionType UInteractableManager::GetInteractionType_Implementation() const
{
	return Config.InteractionType;
}

UInputAction* UInteractableManager::GetInputAction_Implementation() const
{
	return Config.InputAction;
}
FText UInteractableManager::GetInteractionText_Implementation() const
{
	// Return appropriate text based on interaction type
	switch (Config.InteractionType)
	{
	case EInteractionType::IT_Hold:
		return Config.HoldText;
		
	case EInteractionType::IT_Mash:
		return Config.MashText;
		
	case EInteractionType::IT_TapOrHold:
		// For TapOrHold, show both options
		return FText::Format(FText::FromString("{0} | {1}"), Config.TapText, Config.HoldActionText);
		
	default:
		return Config.InteractionText;
	}
}

FVector UInteractableManager::GetWidgetOffset_Implementation() const
{
	return WidgetOffset;
}

// ═══════════════════════════════════════════════════════════════════════
// INTERACTABLE INTERFACE - HOLD INTERACTION
// ═══════════════════════════════════════════════════════════════════════

float UInteractableManager::GetTapHoldThreshold_Implementation() const
{
	return Config.TapHoldThreshold;
}

float UInteractableManager::GetHoldDuration_Implementation() const
{
	return Config.HoldDuration;
}

void UInteractableManager::OnHoldInteractionStart_Implementation(AActor* Interactor)
{
	SetProgressBarVisible(true);
	UE_LOG(LogTemp, Log, TEXT("InteractableManager: Hold start on %s"), *GetOwner()->GetName());
}

void UInteractableManager::OnHoldInteractionUpdate_Implementation(AActor* Interactor, float Progress)
{
	UpdateProgress(Progress, false);
}

void UInteractableManager::OnHoldInteractionComplete_Implementation(AActor* Interactor)
{
	SetProgressBarVisible(false);
	OnHoldCompleted.Broadcast(Interactor);
	UE_LOG(LogTemp, Log, TEXT("InteractableManager: Hold completed on %s"), *GetOwner()->GetName());
}

void UInteractableManager::OnHoldInteractionCancelled_Implementation(AActor* Interactor)
{
	SetProgressBarVisible(false);
	OnHoldCancelled.Broadcast(Interactor);
	UE_LOG(LogTemp, Log, TEXT("InteractableManager: Hold cancelled on %s"), *GetOwner()->GetName());
}

FText UInteractableManager::GetHoldInteractionText_Implementation() const
{
	return Config.HoldText;
}

// ═══════════════════════════════════════════════════════════════════════
// INTERACTABLE INTERFACE - MASH INTERACTION
// ═══════════════════════════════════════════════════════════════════════

int32 UInteractableManager::GetRequiredMashCount_Implementation() const
{
	return Config.RequiredMashCount;
}

float UInteractableManager::GetMashDecayRate_Implementation() const
{
	return Config.MashDecayRate;
}

void UInteractableManager::OnMashInteractionStart_Implementation(AActor* Interactor)
{
	SetProgressBarVisible(true);
	OnMashProgress.Broadcast(Interactor, 0, Config.RequiredMashCount);
	UE_LOG(LogTemp, Log, TEXT("InteractableManager: Mash start on %s"), *GetOwner()->GetName());
}

void UInteractableManager::OnMashInteractionUpdate_Implementation(AActor* Interactor, int32 CurrentCount, int32 RequiredCount, float Progress)
{
	UpdateProgress(Progress, false);
	OnMashProgress.Broadcast(Interactor, CurrentCount, RequiredCount);
	UE_LOG(LogTemp, Verbose, TEXT("InteractableManager: Mash progress %d/%d (%.1f%%)"), 
		CurrentCount, RequiredCount, Progress * 100.0f);
}

void UInteractableManager::OnMashInteractionComplete_Implementation(AActor* Interactor)
{
	SetProgressBarVisible(false);
	OnMashCompleted.Broadcast(Interactor);
	UE_LOG(LogTemp, Log, TEXT("InteractableManager: Mash completed on %s"), *GetOwner()->GetName());
}

void UInteractableManager::OnMashInteractionFailed_Implementation(AActor* Interactor)
{
	SetProgressBarVisible(false);
	OnMashFailed.Broadcast(Interactor);
	UE_LOG(LogTemp, Log, TEXT("InteractableManager: Mash failed on %s"), *GetOwner()->GetName());
}

FText UInteractableManager::GetMashInteractionText_Implementation() const
{
	return Config.MashText;
}

// ═══════════════════════════════════════════════════════════════════════
// INTERACTABLE INTERFACE - TOOLTIP
// ═══════════════════════════════════════════════════════════════════════

bool UInteractableManager::HasTooltip_Implementation() const
{
	// Can be overridden in Blueprint
	return false;
}

UObject* UInteractableManager::GetTooltipData_Implementation() const
{
	// Can be overridden in Blueprint
	return nullptr;
}

FVector UInteractableManager::GetTooltipWorldLocation_Implementation() const
{
	return GetOwner()->GetActorLocation() + WidgetOffset;
}

// ═══════════════════════════════════════════════════════════════════════
// PROGRESS BAR SUPPORT
// ═══════════════════════════════════════════════════════════════════════

void UInteractableManager::UpdateProgress(float Progress, bool bIsDepleting)
{
	if (!WidgetComponent)
	{
		return;
	}

	if (UInteractableWidget* Widget = Cast<UInteractableWidget>(WidgetComponent->GetWidget()))
	{
		Widget->SetProgress(Progress);
	}
}

void UInteractableManager::SetProgressBarVisible(bool bVisible)
{
	if (!WidgetComponent)
	{
		return;
	}

	if (UInteractableWidget* Widget = Cast<UInteractableWidget>(WidgetComponent->GetWidget()))
	{
		Widget->SetProgressBarVisible(bVisible);
	}
}

// ═══════════════════════════════════════════════════════════════════════
// HELPER FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════

void UInteractableManager::UpdateWidgetText()
{
	if (!WidgetComponent)
	{
		return;
	}

	UInteractableWidget* Widget = Cast<UInteractableWidget>(WidgetComponent->GetWidget());
	if (!Widget)
	{
		return;
	}
	
	if (!Config.InputAction)
	{
		UE_LOG(LogTemp, Error, TEXT("InteractableManager: InputAction not set on %s! Widget will not show key icon."), 
			*GetOwner()->GetName());

		Widget->SetInteractionData(GetInputAction(), GetDisplayTextForCurrentType());
	}
}

FText UInteractableManager::GetDisplayTextForCurrentType() const
{
	switch (Config.InteractionType)
	{
	case EInteractionType::IT_Tap:
		return Config.InteractionText;
			
	case EInteractionType::IT_Hold:
		return Config.HoldText;
			
	case EInteractionType::IT_Mash:
		return Config.MashText;
			
	case EInteractionType::IT_TapOrHold:
		// Show both options on separate lines
		return FText::Format(
			FText::FromString("{0}\n{1}"),
			Config.TapText,
			Config.HoldActionText
		);
			
	case EInteractionType::IT_Toggle:
	case EInteractionType::IT_Continuous:
	default:
		return Config.InteractionText;
	}
}	

// ═══════════════════════════════════════════════════════════════════════
// HIGHLIGHT
// ═══════════════════════════════════════════════════════════════════════

void UInteractableManager::ApplyHighlight(bool bHighlight)
{
	for (UPrimitiveComponent* Mesh : MeshesToHighlight)
	{
		if (!Mesh)
		{
			continue;
		}

		if (bHighlight)
		{
			// Enable custom depth
			Mesh->SetRenderCustomDepth(true);
			Mesh->SetCustomDepthStencilValue(HighlightStencilValue);
		}
		else
		{
			// Disable custom depth
			Mesh->SetRenderCustomDepth(false);
		}
	}
}