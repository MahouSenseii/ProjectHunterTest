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
class ULootSubsystem;
class UGroundItemSubsystem;
class USoundBase;
class UParticleSystem;
class UNiagaraSystem;

DECLARE_LOG_CATEGORY_EXTERN(LogLootChest, Log, All);

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

/**
 * FLootChestConfig - Visual and behavior configuration
 * 
 * SINGLE RESPONSIBILITY: Chest visuals, animation, and respawn settings
 * Loot generation is handled by SourceID → LootSubsystem
 */
USTRUCT(BlueprintType)
struct FLootChestConfig
{
	GENERATED_BODY()

	// ═══════════════════════════════════════════════
	// LOOT SOURCE (References DT_LootSourceRegistry)
	// ═══════════════════════════════════════════════

	/** Source ID in DT_LootSourceRegistry (e.g., "Chest_Common", "Chest_Rare") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	FName LootSourceID;

	/** Override level (0 = use source default) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot", meta = (ClampMin = "0", ClampMax = "100"))
	int32 LevelOverride = 0;

	/** Apply player luck bonus? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	bool bApplyPlayerLuck = true;

	/** Apply player magic find bonus? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	bool bApplyPlayerMagicFind = true;

	// ═══════════════════════════════════════════════
	// SPAWN SETTINGS
	// ═══════════════════════════════════════════════

	/** Radius to scatter loot around chest */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "0.0"))
	float ScatterRadius = 150.0f;

	/** Height offset for spawned items */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	float SpawnHeightOffset = 50.0f;

	/** Use random scatter (vs organized circle) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	bool bRandomScatter = true;

	// ═══════════════════════════════════════════════
	// VISUALS
	// ═══════════════════════════════════════════════

	/** Mesh when closed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
	UStaticMesh* ClosedMesh = nullptr;

	/** Mesh when open */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
	UStaticMesh* OpenMesh = nullptr;

	// ═══════════════════════════════════════════════
	// ANIMATION
	// ═══════════════════════════════════════════════

	/** Play open animation? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	bool bPlayOpenAnimation = true;

	/** Open animation duration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation", meta = (EditCondition = "bPlayOpenAnimation", ClampMin = "0.1"))
	float OpenAnimationDuration = 0.5f;

	// ═══════════════════════════════════════════════
	// AUDIO
	// ═══════════════════════════════════════════════

	/** Sound when opened */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	USoundBase* OpenSound = nullptr;

	// ═══════════════════════════════════════════════
	// VFX
	// ═══════════════════════════════════════════════

	/** Particle effect when opened (legacy) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	UParticleSystem* OpenParticle = nullptr;

	/** Niagara effect when opened */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	UNiagaraSystem* OpenNiagaraEffect = nullptr;

	// ═══════════════════════════════════════════════
	// RESPAWN
	// ═══════════════════════════════════════════════

	/** Can this chest respawn? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Respawn")
	bool bCanRespawn = false;

	/** Time until respawn (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Respawn", meta = (EditCondition = "bCanRespawn", ClampMin = "0.0"))
	float RespawnTime = 300.0f;

	/** Re-roll loot on respawn? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Respawn", meta = (EditCondition = "bCanRespawn"))
	bool bRerollLootOnRespawn = true;

	FLootChestConfig()
		: LevelOverride(0)
		, bApplyPlayerLuck(true)
		, bApplyPlayerMagicFind(true)
		, ScatterRadius(150.0f)
		, SpawnHeightOffset(50.0f)
		, bRandomScatter(true)
		, ClosedMesh(nullptr)
		, OpenMesh(nullptr)
		, bPlayOpenAnimation(true)
		, OpenAnimationDuration(0.5f)
		, OpenSound(nullptr)
		, OpenParticle(nullptr)
		, OpenNiagaraEffect(nullptr)
		, bCanRespawn(false)
		, RespawnTime(300.0f)
		, bRerollLootOnRespawn(true)
	{}
};

/**
 * ALootChest - Loot chest actor
 * 
 * SINGLE RESPONSIBILITY: Manage chest behavior and delegate loot generation
 * 
 * DESIGN:
 * - State machine (Closed → Opening → Open → Looted → Respawning)
 * - Delegates loot generation to ULootSubsystem
 * - Uses SourceID to reference loot registry
 * - Handles visuals, audio, and respawn locally
 * 
 * USAGE:
 *   1. Place chest in level
 *   2. Set Config.LootSourceID to registry entry (e.g., "Chest_Common")
 *   3. Configure visuals, respawn, etc.
 *   4. Player interacts → loot generates and spawns
 */
UCLASS()
class PROJECTHUNTERTEST_API ALootChest : public AActor
{
	GENERATED_BODY()
	
public:	
	ALootChest();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// ═══════════════════════════════════════════════
	// COMPONENTS
	// ═══════════════════════════════════════════════

	/** Root scene component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* RootSceneComponent;

	/** Chest mesh */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* ChestMesh;

	/** Interaction component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UInteractableManager* InteractableManager;

	// ═══════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════

	/** Chest configuration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot Chest", meta = (ShowOnlyInnerProperties))
	FLootChestConfig Config;

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
	void CacheSubsystems();

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
	// LOOT GENERATION (Delegates to LootSubsystem)
	// ═══════════════════════════════════════════════

	/**
	 * Generate loot using LootSubsystem
	 * @return Generated loot batch
	 */
	FLootResultBatch GenerateLoot();

	/**
	 * Build loot request from config
	 * @return Configured loot request
	 */
	FLootRequest BuildLootRequest() const;

	/**
	 * Get player stats for loot bonuses
	 * @param Player - Player actor
	 * @param OutLuck - Player's luck stat
	 * @param OutMagicFind - Player's magic find stat
	 */
	void GetPlayerLootStats(AActor* Player, float& OutLuck, float& OutMagicFind) const;

	// ═══════════════════════════════════════════════
	// LOOT SPAWNING
	// ═══════════════════════════════════════════════

	/**
	 * Spawn generated loot on ground
	 * @param LootBatch - Loot to spawn
	 */
	void SpawnLoot(const FLootResultBatch& LootBatch);

	/**
	 * Build spawn settings from config
	 * @return Configured spawn settings
	 */
	FLootSpawnSettings BuildSpawnSettings() const;

	// ═══════════════════════════════════════════════
	// VISUAL/AUDIO FEEDBACK
	// ═══════════════════════════════════════════════

	void PlayOpenAnimation();
	void PlayOpenSound();
	void PlayOpenVFX();

	// ═══════════════════════════════════════════════
	// RESPAWN
	// ═══════════════════════════════════════════════

	void StartRespawnTimer();
	void HandleRespawn();

	// ═══════════════════════════════════════════════
	// CACHED REFERENCES
	// ═══════════════════════════════════════════════

	UPROPERTY()
	ULootSubsystem* CachedLootSubsystem;

	// ═══════════════════════════════════════════════
	// TIMERS & ANIMATION
	// ═══════════════════════════════════════════════

	FTimerHandle OpenAnimationTimer;
	FTimerHandle RespawnTimer;
	float OpenAnimationProgress;
};