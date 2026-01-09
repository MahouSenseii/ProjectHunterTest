// Character/Component/InteractionTraceManager.h
#pragma once

#include "CoreMinimal.h"
#include "Interactable/Library/InteractionEnumLibrary.h"
#include "InteractionTraceManager.generated.h" 

// Forward declarations
class IInteractable;
class UInteractableManager;
class UGroundItemSubsystem;
class UItemInstance;
class APlayerController;
class AALSPlayerCameraManager;
struct FInteractionDebugManager;
class AActor;
class UWorld;

DECLARE_LOG_CATEGORY_EXTERN(LogInteractionTraceManager, Log, All);

/**
 * Manages interaction traces for gameplay elements.
 * Handles the registration, update, and processing of
 * interaction traces in the game environment, ensuring
 * efficient and accurate trace management for various
 * interactive components.
 */

USTRUCT(BlueprintType)
struct PROJECTHUNTERTEST_API FInteractionTraceManager
{
    GENERATED_BODY()

public:
    FInteractionTraceManager();

    // ═══════════════════════════════════════════════
    // CONFIGURATION (Now Blueprint-editable!)
    // ═══════════════════════════════════════════════

    /** Maximum interaction distance */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace")
    float InteractionDistance = 300.0f;

    /** How often to check for interactables (per second) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace")
    float CheckFrequency = 0.1f;

    /** Trace channel for actor detection */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace")
    TEnumAsByte<ECollisionChannel> InteractionTraceChannel = ECC_Visibility;

    /** Use ALS camera target location for trace origin */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace")
    bool bUseALSCameraOrigin = true;

    /** Forward offset from camera pivot (X) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace|Offsets")
    float OffsetForward = 0.0f;

    /** Right offset from camera pivot (Y) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace|Offsets")
    float OffsetRight = 0.0f;

    /** Up offset from camera pivot (Z) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace|Offsets")
    float OffsetUp = 60.0f;

    // ═══════════════════════════════════════════════
    // INITIALIZATION
    // ═══════════════════════════════════════════════

    void Initialize(AActor* Owner, UWorld* World);
    void SetDebugManager(FInteractionDebugManager* InDebugManager);

    // ═══════════════════════════════════════════════
    // PRIMARY FUNCTIONS
    // ═══════════════════════════════════════════════

    TScriptInterface<IInteractable> TraceForActorInteractable();
    UItemInstance* FindNearestGroundItem(int32& OutItemID);
    bool GetCameraViewPoint(FVector& OutLocation, FRotator& OutRotation) const;
    FVector GetTraceStartLocation(const FVector& CameraLocation, const FRotator& CameraRotation) const;
    FVector GetTraceEndLocation(const FVector& CameraLocation, const FRotator& CameraRotation) const;
    const FHitResult& GetLastTraceResult() const { return LastTraceResult; }
    bool IsLocallyControlled() const;

private:
    // ═══════════════════════════════════════════════
    // CACHED REFERENCES (Not Blueprint-exposed)
    // ═══════════════════════════════════════════════

    
    AActor* OwnerActor = nullptr;
    UWorld* WorldContext = nullptr;
    APlayerController* CachedPlayerController = nullptr;
    AALSPlayerCameraManager* CachedALSCameraManager = nullptr;
    UGroundItemSubsystem* CachedGroundItemSubsystem = nullptr;
    FInteractionDebugManager* DebugManager = nullptr;
    
    FHitResult LastTraceResult;

    void CacheComponents();
    bool PerformLineTrace(const FVector& Start, const FVector& End, FHitResult& OutHit);
    bool IsActorInteractable(AActor* Actor) const;
};