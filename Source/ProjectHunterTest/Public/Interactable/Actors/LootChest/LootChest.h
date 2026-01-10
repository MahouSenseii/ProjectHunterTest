// Interactable/Actors/LootChest/LootChest.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Loot/Library/LootStruct.h"
#include "LootChest.generated.h"

// Forward declarations
class UInteractableManager;
class UStaticMeshComponent;
class UItemInstance;
class ULootComponent;
class ULootSubsystem;
class UStatsManager;
class USoundBase;
class UNiagaraSystem;

DECLARE_LOG_CATEGORY_EXTERN(LogLootChest, Log, All);

// ═══════════════════════════════════════════════════════════════════════
// CHEST STATE
// ═══════════════════════════════════════════════════════════════════════

/**
 * Chest state machine
 */
UENUM(BlueprintType)
enum class EChestState : uint8
{
	CS_Closed     UMETA(DisplayName = "Closed"),
	CS_Opening    UMETA(DisplayName = "Opening"),
	CS_Open       UMETA(DisplayName = "Open"),
	CS_Looted     UMETA(DisplayName = "Looted"),
	CS_Respawning UMETA(DisplayName = "Respawning")
};

// ═══════════════════════════════════════════════════════════════════════
// SPLIT CONFIG STRUCTS (Single Responsibility)
// ═══════════════════════════════════════════════════════════════════════

/**
 * FChestVisualConfig - Mesh and appearance settings
 * SINGLE RESPONSIBILITY: Visual representation only
 */
USTRUCT(BlueprintType)
struct FChestVisualConfig
{
	GENERATED_BODY()

	/** Use static mesh or SkeletalMesh */ 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
	bool bUseStaticMesh = true;

	/** Mesh when closed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals" , meta = (EditCondition = "bUseStaticMesh"))
	UStaticMesh* ClosedMesh = nullptr;

	/** Mesh when open */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals", meta = (EditCondition = "bUseStaticMesh"))
	UStaticMesh* OpenMesh = nullptr;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals", meta = (EditCondition = "!bUseStaticMesh"))
	USkeletalMesh* SkeletalMesh = nullptr;
};

/**
 * FChestAnimationConfig - Animation settings
 * SINGLE RESPONSIBILITY: Animation behavior only
 */
USTRUCT(BlueprintType)
struct FChestAnimationConfig
{
	GENERATED_BODY()

	/** Play open animation? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	bool bPlayOpenAnimation = true;

	/** Open animation duration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation", meta = (EditCondition = "bPlayOpenAnimation", ClampMin = "0.1"))
	float OpenAnimationDuration = 0.5f;
};

/**
 * FChestFeedbackConfig - Audio and VFX settings
 * SINGLE RESPONSIBILITY: Sensory feedback only
 */
USTRUCT(BlueprintType)
struct FChestFeedbackConfig
{
	GENERATED_BODY()

	/** Sound when opened */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundBase* OpenSound = nullptr;

	/** Niagara effect when opened */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	UNiagaraSystem* OpenNiagaraEffect = nullptr;
};

/**
 * FChestRespawnConfig - Respawn behavior settings
 * SINGLE RESPONSIBILITY: Respawn logic only
 */
USTRUCT(BlueprintType)
struct FChestRespawnConfig
{
	GENERATED_BODY()

	/** Can this chest respawn? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Respawn")
	bool bCanRespawn = false;

	/** Time until respawn (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Respawn", meta = (EditCondition = "bCanRespawn", ClampMin = "0.0"))
	float RespawnTime = 300.0f;

	/** Re-roll loot on respawn? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Respawn", meta = (EditCondition = "bCanRespawn"))
	bool bRerollLootOnRespawn = true;
};

/**
 * FChestSpawnConfig - Loot spawn positioning
 * SINGLE RESPONSIBILITY: Item spawn placement only
 */
USTRUCT(BlueprintType)
struct FChestSpawnConfig
{
	GENERATED_BODY()

	/** Radius to scatter loot around chest */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "0.0"))
	float ScatterRadius = 150.0f;

	/** Height offset for spawned items */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	float SpawnHeightOffset = 50.0f;

	/** Use random scatter (vs organized circle) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	bool bRandomScatter = true;

	/** Convert to FLootSpawnSettings for subsystem */
	FLootSpawnSettings ToSpawnSettings(FVector BaseLocation) const
	{
		FLootSpawnSettings Settings;
		Settings.SpawnLocation = BaseLocation;
		Settings.ScatterRadius = ScatterRadius;
		Settings.HeightOffset = SpawnHeightOffset;
		Settings.bRandomScatter = bRandomScatter;
		return Settings;
	}
};

// ═══════════════════════════════════════════════════════════════════════
// LOOT CHEST ACTOR
// ═══════════════════════════════════════════════════════════════════════

/**
 * ALootChest - Interactable loot chest actor
 * 
 * SINGLE RESPONSIBILITY: Coordinate chest behavior and delegate concerns
 * 
 * DESIGN:
 * - State machine (Closed → Opening → Open → Looted → Respawning)
 * - Delegates loot generation to ULootComponent
 * - Uses split config structs for clean organization
 * - Timer-based animation (no wasteful tick)
 * - Server-authoritative with client prediction
 * 
 * USAGE:
 *   1. Place chest in level
 *   2. Configure LootComponent.SourceID (e.g., "Chest_Common")
 *   3. Configure visual, animation, feedback settings
 *   4. Player interacts → loot generates and spawns
 */
UCLASS()
class PROJECTHUNTERTEST_API ALootChest : public AActor
{
	GENERATED_BODY()
	
public:	
	ALootChest();

	virtual void BeginPlay() override;
	// NOTE: No Tick() - uses timer-based animation instead

	// ═══════════════════════════════════════════════
	// COMPONENTS
	// ═══════════════════════════════════════════════

	/** Root scene component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* RootSceneComponent;

	/** Chest mesh */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* Static_ChestMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USkeletalMeshComponent* Skeletal_ChestMesh;

	/** Interaction component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UInteractableManager* InteractableManager;

	/** 
	 * Loot component - handles loot generation and spawning
	 * Configure SourceID here (e.g., "Chest_Common", "Chest_Rare")
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	ULootComponent* LootComponent;

	// ═══════════════════════════════════════════════
	// CONFIGURATION (Split by responsibility)
	// ═══════════════════════════════════════════════

	/** Visual appearance settings */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot Chest|Visuals")
	FChestVisualConfig VisualConfig;

	/** Animation settings */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot Chest|Animation")
	FChestAnimationConfig AnimationConfig;

	/** Audio/VFX feedback settings */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot Chest|Feedback")
	FChestFeedbackConfig FeedbackConfig;

	/** Respawn behavior settings */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot Chest|Respawn")
	FChestRespawnConfig RespawnConfig;

	/** Loot spawn positioning settings */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot Chest|Spawn")
	FChestSpawnConfig SpawnConfig;

	/** Apply player luck bonus to loot? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot Chest|Loot")
	bool bApplyPlayerLuck = true;

	/** Apply player magic find bonus to loot? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot Chest|Loot")
	bool bApplyPlayerMagicFind = true;

	// ═══════════════════════════════════════════════
	// STATE
	// ═══════════════════════════════════════════════

	/** Current chest state */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ChestState, Category = "Loot Chest|State")
	EChestState ChestState;

	/** Last generated loot batch (for reference) */
	UPROPERTY(BlueprintReadOnly, Category = "Loot Chest|State")
	FLootResultBatch LastLootBatch;

	/** Who opened this chest? */
	UPROPERTY(BlueprintReadOnly, Category = "Loot Chest|State")
	AActor* LastInteractor;

	// ═══════════════════════════════════════════════
	// EVENTS (Blueprint implementable)
	// ═══════════════════════════════════════════════

	/** Called when chest is opened */
	UFUNCTION(BlueprintImplementableEvent, Category = "Loot Chest|Events")
	void OnChestOpened(AActor* Opener);

	/** Called when loot is generated */
	UFUNCTION(BlueprintImplementableEvent, Category = "Loot Chest|Events")
	void OnLootGenerated(const FLootResultBatch& LootBatch);

	/** Called when chest is fully looted */
	UFUNCTION(BlueprintImplementableEvent, Category = "Loot Chest|Events")
	void OnChestLooted();

	/** Called when chest respawns */
	UFUNCTION(BlueprintImplementableEvent, Category = "Loot Chest|Events")
	void OnChestRespawned();

	// ═══════════════════════════════════════════════
	// PUBLIC INTERFACE
	// ═══════════════════════════════════════════════

	/**
	 * Open the chest
	 * @param Opener - Who opened it
	 */
	UFUNCTION(BlueprintCallable, Category = "Loot Chest")
	void OpenChest(AActor* Opener);

	/**
	 * Reset chest to closed state
	 */
	UFUNCTION(BlueprintCallable, Category = "Loot Chest")
	void ResetChest();

	/**
	 * Force respawn (ignores timer)
	 */
	UFUNCTION(BlueprintCallable, Category = "Loot Chest")
	void ForceRespawn();

	// ═══════════════════════════════════════════════
	// GETTERS
	// ═══════════════════════════════════════════════

	UFUNCTION(BlueprintPure, Category = "Loot Chest")
	EChestState GetChestState() const { return ChestState; }

	UFUNCTION(BlueprintPure, Category = "Loot Chest")
	bool IsOpen() const { return ChestState == EChestState::CS_Open; }

	UFUNCTION(BlueprintPure, Category = "Loot Chest")
	bool IsLooted() const { return ChestState == EChestState::CS_Looted; }

	UFUNCTION(BlueprintPure, Category = "Loot Chest")
	bool IsSourceValid() const;

	// ═══════════════════════════════════════════════
	// NETWORKING
	// ═══════════════════════════════════════════════

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_ChestState();

protected:
	// ═══════════════════════════════════════════════
	// INITIALIZATION
	// ═══════════════════════════════════════════════

	void SetupInteraction();
	void SetupVisuals();
	void SetupLootComponent();

	// ═══════════════════════════════════════════════
	// INTERACTION CALLBACKS
	// ═══════════════════════════════════════════════

	UFUNCTION()
	void OnInteracted(AActor* Interactor);

	// ═══════════════════════════════════════════════
	// STATE MANAGEMENT
	// ═══════════════════════════════════════════════

	void SetChestState(EChestState NewState);
	void UpdateMeshForState();
	void UpdateInteractionForState();

	// ═══════════════════════════════════════════════
	// LOOT (Delegates to LootComponent)
	// ═══════════════════════════════════════════════

	/**
	 * Get player stats for loot generation
	 * @param Player - Player actor
	 * @param OutLuck - Player's luck stat
	 * @param OutMagicFind - Player's magic find stat
	 */
	void GetPlayerLootStats(AActor* Player, float& OutLuck, float& OutMagicFind) const;

	/**
	 * Generate and spawn loot using LootComponent
	 * @param Opener - Who opened the chest
	 */
	void GenerateAndSpawnLoot(AActor* Opener);

	// ═══════════════════════════════════════════════
	// ANIMATION (Timer-based, not Tick-based)
	// ═══════════════════════════════════════════════

	void StartOpenAnimation();
	void OnOpenAnimationComplete();

	// ═══════════════════════════════════════════════
	// VISUAL/AUDIO FEEDBACK
	// ═══════════════════════════════════════════════

	void PlayOpenSound();
	void PlayOpenVFX();

	// ═══════════════════════════════════════════════
	// RESPAWN
	// ═══════════════════════════════════════════════

	void StartRespawnTimer();
	void HandleRespawn();

	// ═══════════════════════════════════════════════
	// TIMERS
	// ═══════════════════════════════════════════════

	FTimerHandle OpenAnimationTimer;
	FTimerHandle RespawnTimer;
};