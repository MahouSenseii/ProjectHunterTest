// Character/Controller/HunterController.cpp - SIMPLIFIED
// Removes all non-existent method calls

#include "Character/Controller/HunterController.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Character/Component/InteractionManager.h"
#include "Interactable/Component/InteractableManager.h"
#include "Interactable/Interface/Interactable.h"
#include "Character/HunterBaseCharacter.h"

struct FEnhancedActionKeyMapping;

AHunterController::AHunterController()
{
	// Enable tick for smooth progress updates
	PrimaryActorTick.bCanEverTick = true;
}

void AHunterController::OnPossess(APawn* NewPawn)
{
	Super::OnPossess(NewPawn);
	CacheComponents();
}

void AHunterController::CacheComponents()
{
	if (APawn* PossessedPawn = GetPawn())
	{
		// Cache InteractionManager from character
		InteractionManager = PossessedPawn->FindComponentByClass<UInteractionManager>();
		
		if (!InteractionManager)
		{
			UE_LOG(LogTemp, Warning, TEXT("HunterController: No InteractionManager found on possessed pawn"));
		}
	}
}

// ═══════════════════════════════════════════════════════════════════════
// TICK - Smooth Progress Updates
// ═══════════════════════════════════════════════════════════════════════

void AHunterController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Update hold interaction progress (smooth progress bar!)
	if (bIsHoldingInteraction)
	{
		UInteractableManager* Interactable = InteractionManager ? InteractionManager->GetCurrentInteractable() : nullptr;
		if (!Interactable || !Interactable->Config.bCanInteract)
		{
			bIsHoldingInteraction = false;
			return;
		}

		// Get elapsed time since button press
		float ElapsedTime = GetElapsedSeconds(GetInputActionByName("Interact"));
		
		// Calculate progress (starts counting after TapHoldThreshold)
		float HoldDuration = Interactable->Config.HoldDuration;
		float Threshold = Interactable->Config.TapHoldThreshold;
		float Progress = FMath::Clamp((ElapsedTime - Threshold) / HoldDuration, 0.0f, 1.0f);

		// Update progress bar (SMOOTH!)
		Interactable->UpdateProgress(Progress, false);

		// Notify interactable of progress
		IInteractable::Execute_OnHoldInteractionUpdate(Interactable, this, Progress);

		// Check if completed
		if (Progress >= 1.0f)
		{
			// Hold completed!
			IInteractable::Execute_OnHoldInteractionComplete(Interactable, this);
			Interactable->SetProgressBarVisible(false);
			bIsHoldingInteraction = false;
			
			UE_LOG(LogTemp, Log, TEXT("Hold interaction completed!"));
		}
	}

	// Update mash interaction decay
	if (bIsMashingInteraction)
	{
		UInteractableManager* Interactable = InteractionManager ? InteractionManager->GetCurrentInteractable() : nullptr;
		if (!Interactable)
		{
			bIsMashingInteraction = false;
			return;
		}

		// Apply decay
		float TimeSinceLastMash = GetWorld()->GetTimeSeconds() - LastMashTime;
		float DecayRate = Interactable->Config.MashDecayRate;
		int32 RequiredCount = Interactable->Config.RequiredMashCount;
		
		// Decay mash count
		MashCount = FMath::Max(0, MashCount - static_cast<int32>(DecayRate * DeltaTime));
		float Progress = FMath::Clamp(static_cast<float>(MashCount) / RequiredCount, 0.0f, 1.0f);

		// Update progress
		Interactable->UpdateProgress(Progress, true); // Depleting = true
		
		IInteractable::Execute_OnMashInteractionUpdate(Interactable, this, MashCount, RequiredCount, Progress);

		// Check if failed (decayed to 0 and no recent mash)
		if (MashCount <= 0 && TimeSinceLastMash > 1.0f)
		{
			IInteractable::Execute_OnMashInteractionFailed(Interactable, this);
			Interactable->SetProgressBarVisible(false);
			bIsMashingInteraction = false;
			MashCount = 0;
			
			UE_LOG(LogTemp, Log, TEXT("Mash interaction failed (decayed)"));
		}
	}
}

// ═══════════════════════════════════════════════════════════════════════
// INTERACT - Main Input Handler
// ═══════════════════════════════════════════════════════════════════════

void AHunterController::Interact(const FInputActionValue& Value)
{
	if (!InteractionManager)
	{
		return;
	}

	if (Value.Get<bool>()) // BUTTON PRESSED
	{
		bHasInteractBeenReleased = false;
		GetWorld()->GetTimerManager().ClearTimer(InteractionDelayHandle);

		UInteractableManager* Interactable = InteractionManager->GetCurrentInteractable();

		// Block if holding and interactable has changed
		if (SavedInteractable && Interactable != SavedInteractable && !bHasInteractBeenReleased)
		{
			return;
		}

		// Save the current interactable at press
		SavedInteractable = Interactable;

		if (!Interactable || !Interactable->Config.bCanInteract)
		{
			return;
		}

		// Check interaction type to determine behavior
		EInteractionType InteractionType = IInteractable::Execute_GetInteractionType(Interactable);

		switch (InteractionType)
		{
		case EInteractionType::IT_Tap:
		case EInteractionType::IT_Toggle:
			// Instant interactions - execute on release
			break;

		case EInteractionType::IT_Hold:
		case EInteractionType::IT_TapOrHold:
			// YOUR SMOOTH FEEDBACK! Show progress bar immediately
			Interactable->SetProgressBarVisible(true);
			
			// Start delayed check for hold input (uses Config threshold!)
			GetWorld()->GetTimerManager().SetTimer(
				InteractionDelayHandle,
				this,
				&AHunterController::RunInteractionCheck,
				Interactable->Config.TapHoldThreshold,
				false
			);
			break;

		case EInteractionType::IT_Mash:
			// Mashing - each press counts
			HandleMashPress(Interactable);
			break;

		case EInteractionType::IT_Continuous:
			// Continuous - start immediately, end on release
			IInteractable::Execute_OnHoldInteractionStart(Interactable, this);
			Interactable->SetProgressBarVisible(true);
			bIsHoldingInteraction = true;
			break;

		default:
			UE_LOG(LogTemp, Warning, TEXT("Unknown interaction type"));
			break;
		}
	}
	else // BUTTON RELEASED
	{
		bHasInteractBeenReleased = true;
		GetWorld()->GetTimerManager().ClearTimer(InteractionDelayHandle);

		UInteractableManager* Interactable = InteractionManager ? InteractionManager->GetCurrentInteractable() : nullptr;
		if (!Interactable)
		{
			SavedInteractable = nullptr;
			return;
		}

		float HeldTime = GetElapsedSeconds(GetInputActionByName("Interact"));
		EInteractionType InteractionType = IInteractable::Execute_GetInteractionType(Interactable);

		// Handle release based on interaction type
		switch (InteractionType)
		{
		case EInteractionType::IT_Tap:
		case EInteractionType::IT_Toggle:
			// Execute tap action
			IInteractable::Execute_OnInteract(Interactable, this);
			break;

		case EInteractionType::IT_TapOrHold:
			// YOUR SMOOTH TRANSITION! ✨
			if (HeldTime < Interactable->Config.TapHoldThreshold)
			{
				// Quick tap - execute TAP action
				IInteractable::Execute_OnInteract(Interactable, this);
				Interactable->SetProgressBarVisible(false);
				
				UE_LOG(LogTemp, Log, TEXT("TapOrHold: Executed TAP action (%.2fs < %.2fs)"), 
					HeldTime, Interactable->Config.TapHoldThreshold);
			}
			else
			{
				// Was holding - already handled in Tick or will complete naturally
				if (!bIsHoldingInteraction)
				{
					Interactable->SetProgressBarVisible(false);
				}
				
				UE_LOG(LogTemp, Log, TEXT("TapOrHold: Released during HOLD"));
			}
			bIsHoldingInteraction = false;
			break;

		case EInteractionType::IT_Hold:
			{
				// If released before completion, cancel
				float TotalHoldTime = Interactable->Config.TapHoldThreshold + Interactable->Config.HoldDuration;
				if (HeldTime < TotalHoldTime)
				{
					IInteractable::Execute_OnHoldInteractionCancelled(Interactable, this);
					Interactable->SetProgressBarVisible(false);
					bIsHoldingInteraction = false;
					
					UE_LOG(LogTemp, Log, TEXT("Hold cancelled (released at %.2fs / %.2fs)"), 
						HeldTime, TotalHoldTime);
				}
			}
			break;

		case EInteractionType::IT_Continuous:
			// Stop continuous interaction
			IInteractable::Execute_OnHoldInteractionCancelled(Interactable, this);
			Interactable->SetProgressBarVisible(false);
			bIsHoldingInteraction = false;
			
			UE_LOG(LogTemp, Log, TEXT("Continuous interaction stopped"));
			break;

		case EInteractionType::IT_Mash:
			// Mashing doesn't care about release
			break;

		default:
			break;
		}

		// Reset for next use
		DoOnce(MyDoOnce, true, false);
		SavedInteractable = nullptr;
	}
}

// ═══════════════════════════════════════════════════════════════════════
// RUN INTERACTION CHECK - Delayed Threshold Check
// ═══════════════════════════════════════════════════════════════════════

void AHunterController::RunInteractionCheck()
{
	CachedHoldTime = GetElapsedSeconds(GetInputActionByName("Interact"));

	UInteractableManager* Interactable = InteractionManager ? InteractionManager->GetCurrentInteractable() : nullptr;
	SavedInteractable = Interactable;
	
	if (!Interactable || !Interactable->Config.bCanInteract)
	{
		return;
	}

	EInteractionType InteractionType = IInteractable::Execute_GetInteractionType(Interactable);

	// Handle based on interaction type
	switch (InteractionType)
	{
	case EInteractionType::IT_Hold:
		// User is still holding past threshold - start hold interaction
		if (!bHasInteractBeenReleased)
		{
			IInteractable::Execute_OnHoldInteractionStart(Interactable, this);
			bIsHoldingInteraction = true;
			
			UE_LOG(LogTemp, Log, TEXT("Hold interaction started"));
		}
		break;

	case EInteractionType::IT_TapOrHold:
		if (!bHasInteractBeenReleased && DoOnce(MyDoOnce, false, false))
		{
			IInteractable::Execute_OnHoldInteractionStart(Interactable, this);
			bIsHoldingInteraction = true;
			
			UE_LOG(LogTemp, Log, TEXT("TapOrHold: Transitioned to HOLD action (smooth!)"));
		}
		break;

	default:
		// Other types don't use timer-based checks
		break;
	}

	bHasCheckedInputType = false;
}

// ═══════════════════════════════════════════════════════════════════════
// HANDLE MASH PRESS - Mash Interaction Handler
// ═══════════════════════════════════════════════════════════════════════

void AHunterController::HandleMashPress(UInteractableManager* Interactable)
{
	if (!Interactable)
	{
		return;
	}

	// Start mashing if not already started
	if (!bIsMashingInteraction)
	{
		bIsMashingInteraction = true;
		MashCount = 0;
		IInteractable::Execute_OnMashInteractionStart(Interactable, this);
		Interactable->SetProgressBarVisible(true);
		
		UE_LOG(LogTemp, Log, TEXT("Mash interaction started"));
	}

	// Increment count
	MashCount++;
	LastMashTime = GetWorld()->GetTimeSeconds();

	// Get required count
	int32 RequiredCount = Interactable->Config.RequiredMashCount;
	
	// Calculate progress
	float Progress = FMath::Clamp(
		static_cast<float>(MashCount) / RequiredCount,
		0.0f,
		1.0f
	);

	// Update progress bar
	Interactable->UpdateProgress(Progress, false);

	// Update interactable
	IInteractable::Execute_OnMashInteractionUpdate(Interactable, this, MashCount, RequiredCount, Progress);

	// Check if completed
	if (MashCount >= RequiredCount)
	{
		IInteractable::Execute_OnMashInteractionComplete(Interactable, this);
		Interactable->SetProgressBarVisible(false);
		bIsMashingInteraction = false;
		MashCount = 0;
		
		UE_LOG(LogTemp, Log, TEXT("Mash interaction completed! (%d/%d)"), MashCount, RequiredCount);
	}
	else
	{
		UE_LOG(LogTemp, Verbose, TEXT("Mash progress: %d/%d (%.1f%%)"), MashCount, RequiredCount, Progress * 100.0f);
	}
}

// ═══════════════════════════════════════════════════════════════════════
// HELPER FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════

EInteractType AHunterController::CheckInputType(const float Elapsed, const float InRequiredHeldTime, const bool bReset)
{
	if (bReset)
	{
		bHasCheckedInputType = false;
	}

	if (!(Elapsed > InRequiredHeldTime))
	{
		return EInteractType::Single;
	}

	if (DoOnce(MyDoOnce, bReset, false))
	{
		return EInteractType::Holding;
	}
	
	return EInteractType::Mashing;
}

float AHunterController::GetElapsedSeconds(const UInputAction* Action) const
{
	if (const auto EnhancedInput = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(this->GetLocalPlayer()))
	{
		if (const auto LocalPlayerInput = EnhancedInput->GetPlayerInput())
		{
			if (const auto ActionData = LocalPlayerInput->FindActionInstanceData(Action))
			{
				return ActionData->GetElapsedTime();
			}
		}
	}
	return 0.0f;
}

const UInputAction* AHunterController::GetInputActionByName(const FString& InString) const
{
	const UInputMappingContext* Context = DefaultInputMappingContext;
	TObjectPtr<const UInputAction> FoundAction = nullptr;

	if (Context)
	{
		const TArray<FEnhancedActionKeyMapping>& Mappings = Context->GetMappings();
		for (const FEnhancedActionKeyMapping& Keymapping : Mappings)
		{
			if (Keymapping.Action && Keymapping.Action->GetFName() == InString)
			{
				FoundAction = Keymapping.Action;
				break;
			}
		}
	}
	
	return FoundAction;
}

bool AHunterController::DoOnce(FDoOnceState& State, bool bReset, bool bStartClosed)
{
	if (bReset)
	{
		State.bHasBeenInitialized = true;
		State.bIsClosed = false;
		return false;
	}

	if (!State.bHasBeenInitialized)
	{
		State.bHasBeenInitialized = true;
		if (bStartClosed)
		{
			State.bIsClosed = true;
			return false;
		}
	}

	if (!State.bIsClosed)
	{
		State.bIsClosed = true;
		return true;
	}

	return false;
}

// ═══════════════════════════════════════════════════════════════════════
// STUBS FOR OLD RPC FUNCTIONS (Remove these if you don't need networking)
// ═══════════════════════════════════════════════════════════════════════

AActor* AHunterController::GetCurrentInteractableObject_Implementation()
{
	if (IsValid(CurrentInteractable))
	{
		return CurrentInteractable->GetOwner();
	}
	return nullptr;
}

void AHunterController::InitializeInteractionWithObject_Implementation(UInteractableManager* Interactable)
{
	// Not used with new system
}

void AHunterController::StartInteractionWithObject_Implementation(UInteractableManager* Interactable, const bool WasHeld)
{
	// Not used with new system
}

void AHunterController::EndInteractionWithObject_Implementation(UInteractableManager* Interactable)
{
	// Not used with new system
}

void AHunterController::RemoveInteractionFromObject_Implementation(UInteractableManager* Interactable)
{
	// Not used with new system
}

void AHunterController::ClientInitializeInteractionWithObject_Implementation(UInteractableManager* Interactable)
{
	// Not used with new system
}

void AHunterController::ServerStartInteractionWithObject_Implementation(UInteractableManager* Interactable, const bool WasHeld)
{
	// Not used with new system
}

void AHunterController::ClientStartInteractionWithObject_Implementation(UInteractableManager* Interactable, const bool WasHeld)
{
	// Not used with new system
}

void AHunterController::ServerEndInteractionWithObject_Implementation(UInteractableManager* Interactable)
{
	// Not used with new system
}

void AHunterController::ClientEndInteractionWithObject_Implementation(UInteractableManager* Interactable)
{
	// Not used with new system
}

void AHunterController::ServerRemoveInteractionFromObject_Implementation(UInteractableManager* Interactable)
{
	// Not used with new system
}

void AHunterController::ClientRemoveInteractionFromObject_Implementation(UInteractableManager* Interactable)
{
	// Not used with new system
}

void AHunterController::InitializeInteraction(UInteractableManager* Interactable)
{
	// Not used with new system
}

void AHunterController::StartInteraction(UInteractableManager* Interactable, const bool WasHeld)
{
	// Not used with new system
}

void AHunterController::EndInteraction(const UInteractableManager* Interactable)
{
	// Not used with new system
}

void AHunterController::RemoveInteraction(UInteractableManager* Interactable)
{
	// Not used with new system
}

void AHunterController::SetCurrentInteractable(UInteractableManager* InInteractable)
{
	CurrentInteractable = InInteractable;
}

void AHunterController::RemoveCurrentInteractable(UInteractableManager* RemovedInteractable)
{
	CurrentInteractable = nullptr;
}

void AHunterController::Menu(const FInputActionValue& Value) const
{
	// Implement menu logic here if needed
}