// Interactable/Actors/LootChest/LootChest.cpp

#include "Interactable/Actors/LootChest/LootChest.h"
#include "Interactable/Component/InteractableManager.h"
#include "Loot/Component/LootComponent.h"
#include "Loot/Subsystem/LootSubsystem.h"
#include "Character/Component/StatsManager.h"
#include "Item/ItemInstance.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogLootChest);

// ═══════════════════════════════════════════════════════════════════════
// CONSTRUCTOR
// ═══════════════════════════════════════════════════════════════════════

ALootChest::ALootChest()
{
	// OPTIMIZATION: No tick needed - we use timers for animation
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// Create root component
	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	RootComponent = RootSceneComponent;

	// Create chest mesh
	ChestMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChestMesh"));
	ChestMesh->SetupAttachment(RootComponent);
	ChestMesh->SetCollisionProfileName(TEXT("BlockAll"));

	// Create interaction component
	InteractableManager = CreateDefaultSubobject<UInteractableManager>(TEXT("InteractableManager"));

	// Create loot component (handles all loot generation/spawning)
	LootComponent = CreateDefaultSubobject<ULootComponent>(TEXT("LootComponent"));

	// Initialize state
	ChestState = EChestState::CS_Closed;
	LastInteractor = nullptr;
}

// ═══════════════════════════════════════════════════════════════════════
// LIFECYCLE
// ═══════════════════════════════════════════════════════════════════════

void ALootChest::BeginPlay()
{
	Super::BeginPlay();

	// Setup components
	SetupInteraction();
	SetupVisuals();
	SetupLootComponent();

	// Validate source
	if (!IsSourceValid())
	{
		UE_LOG(LogLootChest, Warning, TEXT("%s: LootComponent.SourceID '%s' not found in registry"),
			*GetName(), *LootComponent->SourceID.ToString());
	}

	UE_LOG(LogLootChest, Log, TEXT("%s: Initialized with source '%s'"),
		*GetName(), *LootComponent->SourceID.ToString());
}

// ═══════════════════════════════════════════════════════════════════════
// INITIALIZATION
// ═══════════════════════════════════════════════════════════════════════

void ALootChest::SetupInteraction()
{
	if (!InteractableManager)
	{
		return;
	}

	// Configure interaction
	InteractableManager->Config.bCanInteract = true;
	InteractableManager->Config.InteractionType = EInteractionType::IT_Tap;
	InteractableManager->Config.InteractionText = FText::FromString(TEXT("Open Chest"));
	InteractableManager->Config.ActionName = FName("Interact");

	// Setup highlight meshes
	if (ChestMesh)
	{
		InteractableManager->MeshesToHighlight.Add(ChestMesh);
	}

	// Bind interaction event
	InteractableManager->OnTapInteracted.AddDynamic(this, &ALootChest::OnInteracted);

	UE_LOG(LogLootChest, Log, TEXT("%s: Interaction setup complete"), *GetName());
}

void ALootChest::SetupVisuals()
{
	UpdateMeshForState();
}

void ALootChest::SetupLootComponent()
{
	if (!LootComponent)
	{
		UE_LOG(LogLootChest, Error, TEXT("%s: LootComponent is null!"), *GetName());
		return;
	}

	// Configure spawn settings from our config
	LootComponent->DefaultSpawnSettings = SpawnConfig.ToSpawnSettings(GetActorLocation());
}

// ═══════════════════════════════════════════════════════════════════════
// INTERACTION CALLBACKS
// ═══════════════════════════════════════════════════════════════════════

void ALootChest::OnInteracted(AActor* Interactor)
{
	// Only allow interaction when closed
	if (ChestState != EChestState::CS_Closed)
	{
		UE_LOG(LogLootChest, Verbose, TEXT("%s: Cannot interact - state is %s"),
			*GetName(), *UEnum::GetValueAsString(ChestState));
		return;
	}

	OpenChest(Interactor);
}

// ═══════════════════════════════════════════════════════════════════════
// PUBLIC INTERFACE
// ═══════════════════════════════════════════════════════════════════════

void ALootChest::OpenChest(AActor* Opener)
{
	if (ChestState != EChestState::CS_Closed)
	{
		return;
	}

	// Server authority check
	if (!HasAuthority())
	{
		// TODO: RPC to server
		return;
	}

	LastInteractor = Opener;

	// Change state to opening
	SetChestState(EChestState::CS_Opening);

	// Play feedback
	PlayOpenSound();
	PlayOpenVFX();

	// Start animation (timer-based, not tick-based!)
	if (AnimationConfig.bPlayOpenAnimation)
	{
		StartOpenAnimation();
	}
	else
	{
		// Skip animation, go directly to open
		OnOpenAnimationComplete();
	}

	// Broadcast event
	OnChestOpened(Opener);

	UE_LOG(LogLootChest, Log, TEXT("%s: Opened by %s"),
		*GetName(), Opener ? *Opener->GetName() : TEXT("Unknown"));
}

void ALootChest::ResetChest()
{
	if (!HasAuthority())
	{
		return;
	}

	// Clear timers
	GetWorldTimerManager().ClearTimer(OpenAnimationTimer);
	GetWorldTimerManager().ClearTimer(RespawnTimer);

	// Reset state
	LastInteractor = nullptr;
	LastLootBatch = FLootResultBatch();

	SetChestState(EChestState::CS_Closed);

	UE_LOG(LogLootChest, Log, TEXT("%s: Reset to closed state"), *GetName());
}

void ALootChest::ForceRespawn()
{
	if (!HasAuthority())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(RespawnTimer);
	HandleRespawn();
}

bool ALootChest::IsSourceValid() const
{
	if (!LootComponent)
	{
		return false;
	}

	return LootComponent->IsSourceValid();
}

// ═══════════════════════════════════════════════════════════════════════
// NETWORKING
// ═══════════════════════════════════════════════════════════════════════

void ALootChest::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ALootChest, ChestState);
}

void ALootChest::OnRep_ChestState()
{
	// Client-side visual updates
	UpdateMeshForState();
	UpdateInteractionForState();

	// Play feedback on clients
	if (ChestState == EChestState::CS_Opening || ChestState == EChestState::CS_Open)
	{
		PlayOpenSound();
		PlayOpenVFX();
	}
}

// ═══════════════════════════════════════════════════════════════════════
// STATE MANAGEMENT
// ═══════════════════════════════════════════════════════════════════════

void ALootChest::SetChestState(EChestState NewState)
{
	if (ChestState == NewState)
	{
		return;
	}

	EChestState OldState = ChestState;
	ChestState = NewState;

	// Update visuals
	UpdateMeshForState();
	UpdateInteractionForState();

	UE_LOG(LogLootChest, Verbose, TEXT("%s: State changed from %s to %s"),
		*GetName(),
		*UEnum::GetValueAsString(OldState),
		*UEnum::GetValueAsString(NewState));
}

void ALootChest::UpdateMeshForState()
{
	if (!ChestMesh)
	{
		return;
	}

	switch (ChestState)
	{
	case EChestState::CS_Closed:
	case EChestState::CS_Respawning:
		if (VisualConfig.ClosedMesh)
		{
			ChestMesh->SetStaticMesh(VisualConfig.ClosedMesh);
		}
		break;

	case EChestState::CS_Open:
	case EChestState::CS_Looted:
		if (VisualConfig.OpenMesh)
		{
			ChestMesh->SetStaticMesh(VisualConfig.OpenMesh);
		}
		break;

	case EChestState::CS_Opening:
		// Animation in progress - mesh swap happens at completion
		break;
	}
}

void ALootChest::UpdateInteractionForState()
{
	if (!InteractableManager)
	{
		return;
	}

	// Only allow interaction when closed
	InteractableManager->Config.bCanInteract = (ChestState == EChestState::CS_Closed);
}

// ═══════════════════════════════════════════════════════════════════════
// LOOT (Delegates to LootComponent + StatsManager integration)
// ═══════════════════════════════════════════════════════════════════════

void ALootChest::GetPlayerLootStats(AActor* Player, float& OutLuck, float& OutMagicFind) const
{
	OutLuck = 0.0f;
	OutMagicFind = 0.0f;

	if (!Player)
	{
		return;
	}

	// FIX: Use existing StatsManager instead of placeholder!
	if (UStatsManager* Stats = Player->FindComponentByClass<UStatsManager>())
	{
		if (bApplyPlayerLuck)
		{
			OutLuck = Stats->GetLuck();
		}
		
		if (bApplyPlayerMagicFind)
		{
			OutMagicFind = Stats->GetMagicFind();
		}

		UE_LOG(LogLootChest, Verbose, TEXT("%s: Got player stats - Luck: %.2f, MagicFind: %.2f"),
			*GetName(), OutLuck, OutMagicFind);
	}
	else
	{
		UE_LOG(LogLootChest, Warning, TEXT("%s: Player %s has no StatsManager component"),
			*GetName(), *Player->GetName());
	}
}

void ALootChest::GenerateAndSpawnLoot(AActor* Opener)
{
	if (!LootComponent)
	{
		UE_LOG(LogLootChest, Error, TEXT("%s: Cannot generate loot - LootComponent unavailable"),
			*GetName());
		return;
	}

	// Get player stats
	float Luck = 0.0f;
	float MagicFind = 0.0f;
	GetPlayerLootStats(Opener, Luck, MagicFind);

	// Update spawn location (in case chest moved)
	LootComponent->DefaultSpawnSettings = SpawnConfig.ToSpawnSettings(GetActorLocation());

	// Generate and spawn loot via component
	LastLootBatch = LootComponent->DropLoot(Luck, MagicFind);

	// Broadcast event
	OnLootGenerated(LastLootBatch);

	UE_LOG(LogLootChest, Log, TEXT("%s: Generated %d items, %d currency"),
		*GetName(), LastLootBatch.TotalItemCount, LastLootBatch.CurrencyDropped);

	// Transition to looted state
	SetChestState(EChestState::CS_Looted);

	// Broadcast looted event
	OnChestLooted();

	// Start respawn timer if enabled
	if (RespawnConfig.bCanRespawn)
	{
		StartRespawnTimer();
	}
}

// ═══════════════════════════════════════════════════════════════════════
// ANIMATION (Timer-based - OPTIMIZATION: No wasteful Tick!)
// ═══════════════════════════════════════════════════════════════════════

void ALootChest::StartOpenAnimation()
{
	// Use timer instead of Tick for animation
	GetWorldTimerManager().SetTimer(
		OpenAnimationTimer,
		this,
		&ALootChest::OnOpenAnimationComplete,
		AnimationConfig.OpenAnimationDuration,
		false  // No looping
	);

	// TODO: If you need interpolation during animation, you could:
	// 1. Use a Timeline component (Blueprint-friendly)
	// 2. Use FTimerDelegate with smaller intervals
	// 3. Use a Latent Action

	UE_LOG(LogLootChest, Verbose, TEXT("%s: Started open animation (%.2fs)"),
		*GetName(), AnimationConfig.OpenAnimationDuration);
}

void ALootChest::OnOpenAnimationComplete()
{
	// Animation complete - transition to open state
	SetChestState(EChestState::CS_Open);

	// Now generate and spawn the loot
	GenerateAndSpawnLoot(LastInteractor);

	UE_LOG(LogLootChest, Verbose, TEXT("%s: Open animation complete"), *GetName());
}

// ═══════════════════════════════════════════════════════════════════════
// VISUAL/AUDIO FEEDBACK
// ═══════════════════════════════════════════════════════════════════════

void ALootChest::PlayOpenSound()
{
	if (FeedbackConfig.OpenSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			FeedbackConfig.OpenSound,
			GetActorLocation()
		);
	}
}

void ALootChest::PlayOpenVFX()
{
	if (FeedbackConfig.OpenNiagaraEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			FeedbackConfig.OpenNiagaraEffect,
			GetActorLocation(),
			GetActorRotation()
		);
	}
}

// ═══════════════════════════════════════════════════════════════════════
// RESPAWN
// ═══════════════════════════════════════════════════════════════════════

void ALootChest::StartRespawnTimer()
{
	if (!RespawnConfig.bCanRespawn || RespawnConfig.RespawnTime <= 0.0f)
	{
		return;
	}

	SetChestState(EChestState::CS_Respawning);

	GetWorldTimerManager().SetTimer(
		RespawnTimer,
		this,
		&ALootChest::HandleRespawn,
		RespawnConfig.RespawnTime,
		false
	);

	UE_LOG(LogLootChest, Log, TEXT("%s: Respawn timer started (%.1fs)"),
		*GetName(), RespawnConfig.RespawnTime);
}

void ALootChest::HandleRespawn()
{
	// Reset loot component source if needed
	if (RespawnConfig.bRerollLootOnRespawn && LootComponent)
	{
		// The component will generate new loot on next interaction
		// No action needed here - it uses fresh random seeds each time
	}

	// Reset state
	LastInteractor = nullptr;
	LastLootBatch = FLootResultBatch();

	SetChestState(EChestState::CS_Closed);

	// Broadcast event
	OnChestRespawned();

	UE_LOG(LogLootChest, Log, TEXT("%s: Respawned"), *GetName());
}