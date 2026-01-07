// Character/Controller/HunterController.h
// SIMPLIFIED - NO TICK DEPENDENCY!
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "InputActionValue.h"
#include "Character/ALSPlayerController.h"
#include "HunterController.generated.h"

// Forward declarations
class UInteractionManager;
class UInputMappingContext;

/**
 * Hunter Player Controller (Simplified)
 * 
 * ZERO TICK DEPENDENCY!
 * - Just routes input to components
 * - No manual progress tracking
 * - No frame-rate coupling
 * - Clean and simple
 * 
 * All interaction logic handled by InteractionManager!
 */
UCLASS()
class PROJECTHUNTERTEST_API AHunterController : public AALSPlayerController
{
	GENERATED_BODY()

public:
	AHunterController();

	virtual void OnPossess(APawn* NewPawn) override;
	

	// ═══════════════════════════════════════════════
	// INPUT HANDLERS (Route to Components)
	// ═══════════════════════════════════════════════

	/**
	 * Interact input handler
	 * Routes to InteractionManager - that's it!
	 */
	void Interact(const FInputActionValue& Value);

	/**
	 * Pickup all nearby items
	 */
	void PickupAllNearby(const FInputActionValue& Value);

	/**
	 * Menu input handler
	 */
	void Menu(const FInputActionValue& Value) const;

protected:
	// ═══════════════════════════════════════════════
	// CACHED REFERENCES
	// ═══════════════════════════════════════════════

	/** Cached interaction manager */
	UPROPERTY()
	TObjectPtr<UInteractionManager> InteractionManager = nullptr;

	// ═══════════════════════════════════════════════
	// INTERNAL METHODS
	// ═══════════════════════════════════════════════

	/** Cache component references on possess */
	void CacheComponents();
};