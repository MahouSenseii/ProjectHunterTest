// Interactable/Actors/LootChest/LootChest.cpp

#include "Interactable/Actors/LootChest/LootChest.h"
#include "Interactable/Component/InteractableManager.h"
#include "Loot/Subsystem/LootSubsystem.h"
#include "Item/ItemInstance.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

// For player stats - adjust include based on your stat system
// #include "Character/Component/PlayerStatsComponent.h"

DEFINE_LOG_CATEGORY(LogLootChest);

// ═══════════════════════════════════════════════════════════════════════
// CONSTRUCTOR
// ═══════════════════════════════════════════════════════════════════════

ALootChest::ALootChest()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	// Create components
	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	RootComponent = RootSceneComponent;

	ChestMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChestMesh"));
	ChestMesh->SetupAttachment(RootComponent);
	ChestMesh->SetCollisionProfileName(TEXT("BlockAll"));

	InteractableManager = CreateDefaultSubobject<UInteractableManager>(TEXT("InteractableManager"));

	// Initialize state
	ChestState = EChestState::CS_Closed;
	OpenAnimationProgress = 0.0f;
	LastInteractor = nullptr;
	CachedLootSubsystem = nullptr;
}

// ═══════════════════════════════════════════════════════════════════════
// LIFECYCLE
// ═══════════════════════════════════════════════════════════════════════

void ALootChest::BeginPlay()
{
	Super::BeginPlay();

	// Cache subsystems
	CacheSubsystems();

	// Setup interaction
	SetupInteraction();

	// Setup visuals
	SetupVisuals();

	// Validate source
	if (!IsSourceValid())
	{
		UE_LOG(LogLootChest, Warning, TEXT("%s: LootSourceID '%s' not found in registry"),
			*GetName(), *Config.LootSourceID.ToString());
	}

	UE_LOG(LogLootChest, Log, TEXT("%s: Initialized with source '%s'"),
		*GetName(), *Config.LootSourceID.ToString());
}

void ALootChest::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Update open animation
	if (ChestState == EChestState::CS_Opening && Config.bPlayOpenAnimation)
	{
		OpenAnimationProgress += DeltaTime / Config.OpenAnimationDuration;
		
		if (OpenAnimationProgress >= 1.0f)
		{
			OpenAnimationProgress = 1.0f;
			SetChestState(EChestState::CS_Open);
		}

		// TODO: Implement custom animation logic here
		// e.g., rotate lid mesh based on OpenAnimationProgress
	}
}

// ═══════════════════════════════════════════════════════════════════════
// INITIALIZATION
// ═══════════════════════════════════════════════════════════════════════

void ALootChest::CacheSubsystems()
{
	if (UWorld* World = GetWorld())
	{
		CachedLootSubsystem = World->GetSubsystem<ULootSubsystem>();
		
		if (!CachedLootSubsystem)
		{
			UE_LOG(LogLootChest, Error, TEXT("%s: LootSubsystem not found!"), *GetName());
		}
	}
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

// ═══════════════════════════════════════════════════════════════════════
// INTERACTION CALLBACKS
// ═══════════════════════════════════════════════════════════════════════

void ALootChest::OnInteracted(AActor* Interactor)
{
	// Only allow interaction when closed
	if (ChestState != EChestState::CS_Closed)
	{
		UE_LOG(LogLootChest, Warning, TEXT("%s: Cannot interact - state is %d"),
			*GetName(), static_cast<int32>(ChestState));
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

	LastInteractor = Opener;

	// Change state to opening
	SetChestState(EChestState::CS_Opening);

	// Play feedback
	PlayOpenSound();
	PlayOpenVFX();

	// Start animation or skip to open
	if (Config.bPlayOpenAnimation)
	{
		PlayOpenAnimation();
	}
	else
	{
		SetChestState(EChestState::CS_Open);
	}

	// Broadcast event
	OnChestOpened(Opener);

	UE_LOG(LogLootChest, Log, TEXT("%s: Opened by %s"),
		*GetName(), Opener ? *Opener->GetName() : TEXT("Unknown"));
}

void ALootChest::ResetChest()
{
	// Clear last loot batch
	LastLootBatch.Clear();

	// Reset state
	SetChestState(EChestState::CS_Closed);

	// Clear interactor
	LastInteractor = nullptr;

	UE_LOG(LogLootChest, Log, TEXT("%s: Reset"), *GetName());
}

void ALootChest::ForceRespawn()
{
	// Clear respawn timer
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(RespawnTimer);
	}

	// Respawn immediately
	HandleRespawn();
}

bool ALootChest::IsSourceValid() const
{
	if (!CachedLootSubsystem || Config.LootSourceID.IsNone())
	{
		return false;
	}

	return CachedLootSubsystem->IsSourceRegistered(Config.LootSourceID);
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

	// Update interaction
	UpdateInteractionForState();

	// Handle state transitions
	switch (ChestState)
	{
	case EChestState::CS_Open:
		{
			// Generate and spawn loot when fully opened
			FLootResultBatch LootBatch = GenerateLoot();
			
			if (LootBatch.TotalItemCount > 0)
			{
				SpawnLoot(LootBatch);
				OnLootGenerated(LootBatch);
			}
			
			// Transition to looted
			SetChestState(EChestState::CS_Looted);
		}
		break;

	case EChestState::CS_Looted:
		{
			OnChestLooted();
			
			// Start respawn timer if enabled
			if (Config.bCanRespawn)
			{
				StartRespawnTimer();
			}
		}
		break;

	default:
		break;
	}

	UE_LOG(LogLootChest, Log, TEXT("%s: State changed %d → %d"),
		*GetName(), static_cast<int32>(OldState), static_cast<int32>(ChestState));
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
		if (Config.ClosedMesh)
		{
			ChestMesh->SetStaticMesh(Config.ClosedMesh);
		}
		break;

	case EChestState::CS_Open:
	case EChestState::CS_Looted:
		if (Config.OpenMesh)
		{
			ChestMesh->SetStaticMesh(Config.OpenMesh);
		}
		break;

	case EChestState::CS_Opening:
		// Animation handles this
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
// LOOT GENERATION (Delegates to LootSubsystem)
// ═══════════════════════════════════════════════════════════════════════

FLootResultBatch ALootChest::GenerateLoot()
{
	if (!CachedLootSubsystem)
	{
		UE_LOG(LogLootChest, Error, TEXT("%s: Cannot generate loot - LootSubsystem unavailable"),
			*GetName());
		return FLootResultBatch();
	}

	if (Config.LootSourceID.IsNone())
	{
		UE_LOG(LogLootChest, Warning, TEXT("%s: No LootSourceID configured"), *GetName());
		return FLootResultBatch();
	}

	// Build request
	FLootRequest Request = BuildLootRequest();

	// Generate loot via subsystem
	LastLootBatch = CachedLootSubsystem->GenerateLoot(Request);

	UE_LOG(LogLootChest, Log, TEXT("%s: Generated %d items, %d currency"),
		*GetName(), LastLootBatch.TotalItemCount, LastLootBatch.CurrencyDropped);

	return LastLootBatch;
}

FLootRequest ALootChest::BuildLootRequest() const
{
	FLootRequest Request(Config.LootSourceID);

	// Level override
	Request.OverrideLevel = Config.LevelOverride;

	// Player stats
	if (LastInteractor)
	{
		float Luck = 0.0f;
		float MagicFind = 0.0f;

		if (Config.bApplyPlayerLuck || Config.bApplyPlayerMagicFind)
		{
			GetPlayerLootStats(LastInteractor, Luck, MagicFind);
		}

		if (Config.bApplyPlayerLuck)
		{
			Request.PlayerLuck = Luck;
		}

		if (Config.bApplyPlayerMagicFind)
		{
			Request.PlayerMagicFind = MagicFind;
		}
	}

	return Request;
}

void ALootChest::GetPlayerLootStats(AActor* Player, float& OutLuck, float& OutMagicFind) const
{
	OutLuck = 0.0f;
	OutMagicFind = 0.0f;

	if (!Player)
	{
		return;
	}

	// TODO: Replace with your actual stat system
	// Example using a hypothetical PlayerStatsComponent:
	//
	// if (UPlayerStatsComponent* Stats = Player->FindComponentByClass<UPlayerStatsComponent>())
	// {
	//     OutLuck = Stats->GetLuck();
	//     OutMagicFind = Stats->GetMagicFind();
	// }

	// Or using Gameplay Ability System:
	//
	// if (UAbilitySystemComponent* ASC = Player->FindComponentByClass<UAbilitySystemComponent>())
	// {
	//     OutLuck = ASC->GetNumericAttribute(UYourAttributeSet::GetLuckAttribute());
	//     OutMagicFind = ASC->GetNumericAttribute(UYourAttributeSet::GetMagicFindAttribute());
	// }

	// Temporary: Check for a simple interface or use defaults
	// This prevents compile errors until you integrate your stat system
}

// ═══════════════════════════════════════════════════════════════════════
// LOOT SPAWNING
// ═══════════════════════════════════════════════════════════════════════

void ALootChest::SpawnLoot(const FLootResultBatch& LootBatch)
{
	if (!CachedLootSubsystem)
	{
		UE_LOG(LogLootChest, Error, TEXT("%s: Cannot spawn loot - LootSubsystem unavailable"),
			*GetName());
		return;
	}

	if (LootBatch.Results.Num() == 0)
	{
		UE_LOG(LogLootChest, Warning, TEXT("%s: No items to spawn"), *GetName());
		return;
	}

	// Build spawn settings
	FLootSpawnSettings SpawnSettings = BuildSpawnSettings();

	// Spawn via subsystem
	CachedLootSubsystem->SpawnLootResults(LootBatch, SpawnSettings);

	UE_LOG(LogLootChest, Log, TEXT("%s: Spawned %d items"),
		*GetName(), LootBatch.Results.Num());
}

FLootSpawnSettings ALootChest::BuildSpawnSettings() const
{
	FLootSpawnSettings Settings;

	Settings.SpawnLocation = GetActorLocation();
	Settings.ScatterRadius = Config.ScatterRadius;
	Settings.HeightOffset = Config.SpawnHeightOffset;
	Settings.bRandomScatter = Config.bRandomScatter;

	return Settings;
}

// ═══════════════════════════════════════════════════════════════════════
// VISUAL/AUDIO FEEDBACK
// ═══════════════════════════════════════════════════════════════════════

void ALootChest::PlayOpenAnimation()
{
	OpenAnimationProgress = 0.0f;
	// Animation is updated in Tick()
}

void ALootChest::PlayOpenSound()
{
	if (Config.OpenSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			Config.OpenSound,
			GetActorLocation()
		);
	}
}

void ALootChest::PlayOpenVFX()
{
	// Legacy particle system
	if (Config.OpenParticle)
	{
		UGameplayStatics::SpawnEmitterAtLocation(
			GetWorld(),
			Config.OpenParticle,
			GetActorLocation()
		);
	}

	// Niagara effect
	if (Config.OpenNiagaraEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			Config.OpenNiagaraEffect,
			GetActorLocation()
		);
	}
}

// ═══════════════════════════════════════════════════════════════════════
// RESPAWN
// ═══════════════════════════════════════════════════════════════════════

void ALootChest::StartRespawnTimer()
{
	if (!GetWorld() || Config.RespawnTime <= 0.0f)
	{
		return;
	}

	SetChestState(EChestState::CS_Respawning);

	GetWorld()->GetTimerManager().SetTimer(
		RespawnTimer,
		this,
		&ALootChest::HandleRespawn,
		Config.RespawnTime,
		false
	);

	UE_LOG(LogLootChest, Log, TEXT("%s: Will respawn in %.1f seconds"),
		*GetName(), Config.RespawnTime);
}

void ALootChest::HandleRespawn()
{
	// Clear previous loot batch
	LastLootBatch.Clear();

	// Reset animation
	OpenAnimationProgress = 0.0f;

	// Reset to closed state
	SetChestState(EChestState::CS_Closed);

	// Broadcast event
	OnChestRespawned();

	UE_LOG(LogLootChest, Log, TEXT("%s: Respawned"), *GetName());
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
	// Update visuals when state replicates
	UpdateMeshForState();
	UpdateInteractionForState();
}