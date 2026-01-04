// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/ALSPlayerController.h"
#include "Interactable/Library/InteractionEnumLibrary.h"
#include "HunterController.generated.h"

class UInputAction;
class UInteractableManager;
class UInteractionManager;
struct FInputActionValue;

/**
 * Player controller for Hunter character
 * Extends ALS controller with interaction support
 * Supports 6 interaction types with smooth tap/hold transitions
 */
UCLASS()
class PROJECTHUNTERTEST_API AHunterController : public AALSPlayerController
{
	GENERATED_BODY()
	
public:
	AHunterController();

	virtual void OnPossess(APawn* NewPawn) override;
	virtual void Tick(float DeltaTime) override;
	
protected:
	// ═══════════════════════════════════════════════
	// INTERACTION INPUT
	// ═══════════════════════════════════════════════
	
	/** Called when interact button is pressed/released */
	void Interact(const FInputActionValue& Value);

	/** Delayed check after RequiredHeldTime to determine tap vs hold */
	void RunInteractionCheck();

private:
	// ═══════════════════════════════════════════════
	// CACHED COMPONENTS
	// ═══════════════════════════════════════════════
	
	/** Cached interaction manager from possessed character */
	UPROPERTY()
	TObjectPtr<UInteractionManager> InteractionManager;

	UPROPERTY()
	TObjectPtr<UInteractableManager> SavedInteractable = nullptr;

	UPROPERTY()
	TObjectPtr<UInteractableManager> CurrentInteractable = nullptr;

	/** Cache components from possessed character */
	void CacheComponents();

	// ═══════════════════════════════════════════════
	// INTERACTION STATE
	// ═══════════════════════════════════════════════
	
	/** Has interact button been released */
	bool bHasInteractBeenReleased = false;
	
	/** Has input type been checked */
	bool bHasCheckedInputType = false;
	
	/** Currently in hold interaction (updates progress in Tick) */
	bool bIsHoldingInteraction = false;

	/** Currently in mash interaction (updates decay in Tick) */
	bool bIsMashingInteraction = false;

	/** Current mash count */
	int32 MashCount = 0;

	/** Time of last mash press (for decay calculation) */
	float LastMashTime = 0.0f;
	
	/** Timer handle for delayed interaction check */
	FTimerHandle InteractionDelayHandle;
	
	/** DoOnce state for smooth transitions */
	FDoOnceState MyDoOnce;
	
	/** Cached hold time */
	float CachedHoldTime = 0.0f;

	// ═══════════════════════════════════════════════
	// INTERACTION HELPERS
	// ═══════════════════════════════════════════════
	
	/** Handle mash button press */
	void HandleMashPress(UInteractableManager* Interactable);
	
	/** Check input type (legacy support) */
	EInteractType CheckInputType(const float Elapsed, const float InRequiredHeldTime, const bool bReset);
	
	/** Get elapsed time for an input action */
	float GetElapsedSeconds(const UInputAction* Action) const;
	
	/** Get input action by name */
	const UInputAction* GetInputActionByName(const FString& InString) const;
	
	/** DoOnce pattern implementation */
	bool DoOnce(FDoOnceState& State, bool bReset, bool bStartClosed);

	/** Initialize interaction with object */
	void InitializeInteraction(UInteractableManager* Interactable);
	
	/** Start interaction with object */
	void StartInteraction(UInteractableManager* Interactable, const bool WasHeld);
	
	/** End interaction with object */
	void EndInteraction(const UInteractableManager* Interactable);
	
	/** Remove interaction from object */
	void RemoveInteraction(UInteractableManager* Interactable);

public:
	// ═══════════════════════════════════════════════
	// BLUEPRINT INTERFACE
	// ═══════════════════════════════════════════════
	
	/** Get current interactable object */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	AActor* GetCurrentInteractableObject();

	/** Initialize interaction with object */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void InitializeInteractionWithObject(UInteractableManager* Interactable);

	/** Start interaction with object */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void StartInteractionWithObject(UInteractableManager* Interactable, const bool WasHeld);

	/** End interaction with object */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void EndInteractionWithObject(UInteractableManager* Interactable);

	/** Remove interaction from object */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void RemoveInteractionFromObject(UInteractableManager* Interactable);

	// ═══════════════════════════════════════════════
	// NETWORK REPLICATION
	// ═══════════════════════════════════════════════
	
	/** Client: Initialize interaction */
	UFUNCTION(Client, Reliable)
	void ClientInitializeInteractionWithObject(UInteractableManager* Interactable);

	/** Server: Start interaction */
	UFUNCTION(Server, Reliable)
	void ServerStartInteractionWithObject(UInteractableManager* Interactable, const bool WasHeld);

	/** Client: Start interaction */
	UFUNCTION(Client, Reliable)
	void ClientStartInteractionWithObject(UInteractableManager* Interactable, const bool WasHeld);

	/** Server: End interaction */
	UFUNCTION(Server, Reliable)
	void ServerEndInteractionWithObject(UInteractableManager* Interactable);

	/** Client: End interaction */
	UFUNCTION(Client, Reliable)
	void ClientEndInteractionWithObject(UInteractableManager* Interactable);

	/** Server: Remove interaction */
	UFUNCTION(Server, Reliable)
	void ServerRemoveInteractionFromObject(UInteractableManager* Interactable);

	/** Client: Remove interaction */
	UFUNCTION(Client, Reliable)
	void ClientRemoveInteractionFromObject(UInteractableManager* Interactable);

	// ═══════════════════════════════════════════════
	// PUBLIC INTERFACE
	// ═══════════════════════════════════════════════
	
	/** Set current interactable */
	void SetCurrentInteractable(UInteractableManager* InInteractable);
	
	/** Remove current interactable */
	void RemoveCurrentInteractable(UInteractableManager* RemovedInteractable);

	/** Menu input handler */
	void Menu(const FInputActionValue& Value) const;
};