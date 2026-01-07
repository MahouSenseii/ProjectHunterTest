// Character/Controller/HunterController.cpp
// SIMPLIFIED - NO TICK DEPENDENCY!

#include "Character/Controller/HunterController.h"
#include "EnhancedInputSubsystems.h"
#include "Character/Component/Interaction/InteractionManager.h"
#include "GameFramework/Pawn.h"

AHunterController::AHunterController()
{
}

void AHunterController::OnPossess(APawn* NewPawn)
{
	Super::OnPossess(NewPawn);
	
	// Cache component references
	CacheComponents();
}

// ═══════════════════════════════════════════════════════════════════════
// INPUT HANDLERS - Pure Delegation!
// ═══════════════════════════════════════════════════════════════════════

void AHunterController::Interact(const FInputActionValue& Value)
{
	if (!InteractionManager)
	{
		return;
	}

	// Just route to InteractionManager - that's it!
	if (Value.Get<bool>()) // Pressed
	{
		InteractionManager->OnInteractPressed();
	}
	else // Released
	{
		InteractionManager->OnInteractReleased();
	}
}

void AHunterController::PickupAllNearby(const FInputActionValue& Value)
{
	if (!InteractionManager || !Value.Get<bool>())
	{
		return;
	}

	// Route to InteractionManager
	InteractionManager->PickupAllNearbyItems();
}

void AHunterController::Menu(const FInputActionValue& Value) const
{
	// TODO: Implement menu logic
	if (Value.Get<bool>())
	{
		UE_LOG(LogTemp, Log, TEXT("Menu button pressed"));
	}
}

// ═══════════════════════════════════════════════════════════════════════
// INTERNAL METHODS
// ═══════════════════════════════════════════════════════════════════════

void AHunterController::CacheComponents()
{
	if (APawn* PossessedPawn = GetPawn())
	{
		// Cache InteractionManager
		InteractionManager = PossessedPawn->FindComponentByClass<UInteractionManager>();
		
		if (!InteractionManager)
		{
			UE_LOG(LogTemp, Warning, TEXT("HunterController: No InteractionManager found on %s"), 
				*PossessedPawn->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("HunterController: Cached InteractionManager"));
		}
	}
}