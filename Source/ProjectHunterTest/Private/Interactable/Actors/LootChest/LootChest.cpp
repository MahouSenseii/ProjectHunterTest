// Interactable/Actors/LootChest/LootChest.cpp

#include "Interactable/Actors/LootChest/LootChest.h"
#include "Interactable/Component/InteractableManager.h"
#include "Loot/Component/LootComponent.h"
#include "Loot/Subsystem/LootSubsystem.h"
#include "Character/Component/StatsManager.h"
#include "Item/ItemInstance.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
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

	// Create static chest mesh
	Static_ChestMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticChestMesh"));
	Static_ChestMesh->SetupAttachment(RootComponent);
	Static_ChestMesh->SetCollisionProfileName(TEXT("BlockAll"));

	// Create skeletal chest mesh
	Skeletal_ChestMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalChestMesh"));
	Skeletal_ChestMesh->SetupAttachment(RootComponent);
	Skeletal_ChestMesh->SetCollisionProfileName(TEXT("BlockAll"));
	// Use single node instance for direct animation control
	Skeletal_ChestMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);

	// Create interaction component
	InteractableManager = CreateDefaultSubobject<UInteractableManager>(TEXT("InteractableManager"));

	// Create loot component
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

	// Configure mesh visibility based on type selection
	ConfigureMeshVisibility();

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

	UE_LOG(LogLootChest, Log, TEXT("%s: Initialized with source '%s' (MeshType: %s)"),
		*GetName(), 
		*LootComponent->SourceID.ToString(),
		VisualConfig.bUseStaticMesh ? TEXT("Static") : TEXT("Skeletal"));
}

// ═══════════════════════════════════════════════════════════════════════
// INITIALIZATION
// ═══════════════════════════════════════════════════════════════════════

void ALootChest::ConfigureMeshVisibility()
{
	if (VisualConfig.bUseStaticMesh)
	{
		// Static mesh mode
		if (Static_ChestMesh)
		{
			Static_ChestMesh->SetVisibility(true);
			Static_ChestMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
		if (Skeletal_ChestMesh)
		{
			Skeletal_ChestMesh->SetVisibility(false);
			Skeletal_ChestMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	else
	{
		// Skeletal mesh mode
		if (Static_ChestMesh)
		{
			Static_ChestMesh->SetVisibility(false);
			Static_ChestMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		if (Skeletal_ChestMesh)
		{
			Skeletal_ChestMesh->SetVisibility(true);
			Skeletal_ChestMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			
			// Setup skeletal mesh and animation
			if (VisualConfig.SkeletalMesh)
			{
				Skeletal_ChestMesh->SetSkeletalMesh(VisualConfig.SkeletalMesh);
			}
			
			// Set to closed position (frame 0)
			SetSkeletalAnimationPosition(0.0f);
		}
	}

	UE_LOG(LogLootChest, Verbose, TEXT("%s: Configured mesh visibility (Static: %s, Skeletal: %s)"),
		*GetName(),
		Static_ChestMesh && Static_ChestMesh->IsVisible() ? TEXT("Visible") : TEXT("Hidden"),
		Skeletal_ChestMesh && Skeletal_ChestMesh->IsVisible() ? TEXT("Visible") : TEXT("Hidden"));
}

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

	// Setup highlight meshes (only add the visible one)
	if (VisualConfig.bUseStaticMesh)
	{
		if (Static_ChestMesh)
		{
			InteractableManager->MeshesToHighlight.Add(Static_ChestMesh);
		}
	}
	else
	{
		if (Skeletal_ChestMesh)
		{
			InteractableManager->MeshesToHighlight.Add(Skeletal_ChestMesh);
		}
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

	// Clear any existing timers
	GetWorldTimerManager().ClearTimer(OpenAnimationTimer);
	GetWorldTimerManager().ClearTimer(CloseAnimationTimer);
	GetWorldTimerManager().ClearTimer(RespawnTimer);

	// If using skeletal mesh with animation, play reverse animation
	if (!VisualConfig.bUseStaticMesh && AnimationConfig.bPlayOpenAnimation && VisualConfig.OpenAnimation)
	{
		SetChestState(EChestState::CS_Closing);
		PlayCloseSound();
		StartCloseAnimation();
	}
	else
	{
		// Static mesh or no animation - immediate reset
		LastInteractor = nullptr;
		LastLootBatch = FLootResultBatch();
		SetChestState(EChestState::CS_Closed);
		
		UE_LOG(LogLootChest, Log, TEXT("%s: Reset to closed state (immediate)"), *GetName());
	}
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
	switch (ChestState)
	{
	case EChestState::CS_Opening:
	case EChestState::CS_Open:
		PlayOpenSound();
		PlayOpenVFX();
		if (ChestState == EChestState::CS_Opening && !VisualConfig.bUseStaticMesh)
		{
			PlaySkeletalAnimation(false); // Forward
		}
		break;

	case EChestState::CS_Closing:
		PlayCloseSound();
		if (!VisualConfig.bUseStaticMesh)
		{
			PlaySkeletalAnimation(true); // Reverse
		}
		break;

	default:
		break;
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
	if (VisualConfig.bUseStaticMesh)
	{
		// Static mesh mode - swap meshes based on state
		if (!Static_ChestMesh)
		{
			return;
		}

		switch (ChestState)
		{
		case EChestState::CS_Closed:
		case EChestState::CS_Respawning:
			if (VisualConfig.ClosedMesh)
			{
				Static_ChestMesh->SetStaticMesh(VisualConfig.ClosedMesh);
			}
			break;

		case EChestState::CS_Open:
		case EChestState::CS_Looted:
			if (VisualConfig.OpenMesh)
			{
				Static_ChestMesh->SetStaticMesh(VisualConfig.OpenMesh);
			}
			break;

		case EChestState::CS_Opening:
		case EChestState::CS_Closing:
			// Intermediate state - keep current mesh
			break;
		}
	}
	else
	{
		// Skeletal mesh mode - animation handles visual state
		// Only need to set position for instant state changes (no animation)
		if (!Skeletal_ChestMesh || !VisualConfig.OpenAnimation)
		{
			return;
		}

		switch (ChestState)
		{
		case EChestState::CS_Closed:
		case EChestState::CS_Respawning:
			// Ensure at closed position
			SetSkeletalAnimationPosition(0.0f);
			break;

		case EChestState::CS_Open:
		case EChestState::CS_Looted:
			// Ensure at open position
			SetSkeletalAnimationPosition(1.0f);
			break;

		case EChestState::CS_Opening:
		case EChestState::CS_Closing:
			// Animation in progress - don't interfere
			break;
		}
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

float ALootChest::GetAnimationDuration() const
{
	// For skeletal mesh, use actual animation length
	if (!VisualConfig.bUseStaticMesh && VisualConfig.OpenAnimation)
	{
		const float AnimLength = VisualConfig.OpenAnimation->GetPlayLength();
		const float PlayRate = FMath::Max(AnimationConfig.AnimationPlayRate, 0.1f);
		return AnimLength / PlayRate;
	}

	// For static mesh, use configured duration
	return AnimationConfig.OpenAnimationDuration;
}

void ALootChest::StartOpenAnimation()
{
	const float Duration = GetAnimationDuration();

	// Start animation based on mesh type
	if (!VisualConfig.bUseStaticMesh)
	{
		PlaySkeletalAnimation(false); // Forward
	}

	// Set timer for completion callback
	GetWorldTimerManager().SetTimer(
		OpenAnimationTimer,
		this,
		&ALootChest::OnOpenAnimationComplete,
		Duration,
		false
	);

	UE_LOG(LogLootChest, Verbose, TEXT("%s: Started open animation (%.2fs, %s)"),
		*GetName(), Duration, VisualConfig.bUseStaticMesh ? TEXT("Static") : TEXT("Skeletal"));
}

void ALootChest::OnOpenAnimationComplete()
{
	// Animation complete - transition to open state
	SetChestState(EChestState::CS_Open);

	// Now generate and spawn the loot
	GenerateAndSpawnLoot(LastInteractor);

	UE_LOG(LogLootChest, Verbose, TEXT("%s: Open animation complete"), *GetName());
}

void ALootChest::StartCloseAnimation()
{
	const float Duration = GetAnimationDuration();

	// Start reverse animation
	if (!VisualConfig.bUseStaticMesh)
	{
		PlaySkeletalAnimation(true); // Reverse
	}

	// Set timer for completion callback
	GetWorldTimerManager().SetTimer(
		CloseAnimationTimer,
		this,
		&ALootChest::OnCloseAnimationComplete,
		Duration,
		false
	);

	UE_LOG(LogLootChest, Verbose, TEXT("%s: Started close animation (%.2fs, reverse)"),
		*GetName(), Duration);
}

void ALootChest::OnCloseAnimationComplete()
{
	// Reset state data
	LastInteractor = nullptr;
	LastLootBatch = FLootResultBatch();

	// Transition to closed state
	SetChestState(EChestState::CS_Closed);

	UE_LOG(LogLootChest, Log, TEXT("%s: Reset to closed state (animation complete)"), *GetName());
}

// ═══════════════════════════════════════════════════════════════════════
// SKELETAL MESH ANIMATION HELPERS
// ═══════════════════════════════════════════════════════════════════════

void ALootChest::PlaySkeletalAnimation(bool bReverse)
{
	if (!Skeletal_ChestMesh || !VisualConfig.OpenAnimation)
	{
		UE_LOG(LogLootChest, Warning, TEXT("%s: Cannot play skeletal animation - missing component or animation"),
			*GetName());
		return;
	}

	const float PlayRate = AnimationConfig.AnimationPlayRate * (bReverse ? -1.0f : 1.0f);
	const float StartPosition = bReverse ? VisualConfig.OpenAnimation->GetPlayLength() : 0.0f;

	// Play animation with specified rate and start position
	Skeletal_ChestMesh->PlayAnimation(VisualConfig.OpenAnimation, false);
	Skeletal_ChestMesh->SetPlayRate(PlayRate);
	Skeletal_ChestMesh->SetPosition(StartPosition);

	UE_LOG(LogLootChest, Verbose, TEXT("%s: Playing skeletal animation (Rate: %.2f, Start: %.2f, Reverse: %s)"),
		*GetName(), PlayRate, StartPosition, bReverse ? TEXT("Yes") : TEXT("No"));
}

void ALootChest::StopSkeletalAnimation()
{
	if (!Skeletal_ChestMesh)
	{
		return;
	}

	Skeletal_ChestMesh->Stop();
}

void ALootChest::SetSkeletalAnimationPosition(float NormalizedPosition)
{
	if (!Skeletal_ChestMesh || !VisualConfig.OpenAnimation)
	{
		return;
	}

	// Clamp to valid range
	NormalizedPosition = FMath::Clamp(NormalizedPosition, 0.0f, 1.0f);

	// Calculate actual position in animation
	const float AnimLength = VisualConfig.OpenAnimation->GetPlayLength();
	const float TargetPosition = NormalizedPosition * AnimLength;

	// Set animation to specific frame (stopped)
	Skeletal_ChestMesh->SetAnimation(VisualConfig.OpenAnimation);
	Skeletal_ChestMesh->SetPosition(TargetPosition);
	Skeletal_ChestMesh->SetPlayRate(0.0f); // Stopped

	UE_LOG(LogLootChest, Verbose, TEXT("%s: Set skeletal animation position to %.2f (%.2fs)"),
		*GetName(), NormalizedPosition, TargetPosition);
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

void ALootChest::PlayCloseSound()
{
	if (FeedbackConfig.CloseSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			FeedbackConfig.CloseSound,
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
	// Play close animation if configured
	if (RespawnConfig.bPlayCloseAnimationOnRespawn && 
		!VisualConfig.bUseStaticMesh && 
		VisualConfig.OpenAnimation &&
		AnimationConfig.bPlayOpenAnimation)
	{
		PlayCloseSound();
		StartCloseAnimation();
		// Close animation will set state to CS_Closed when complete
	}
	else
	{
		// Immediate respawn
		LastInteractor = nullptr;
		LastLootBatch = FLootResultBatch();
		SetChestState(EChestState::CS_Closed);
	}

	// Broadcast event
	OnChestRespawned();

	UE_LOG(LogLootChest, Log, TEXT("%s: Respawned"), *GetName());
}