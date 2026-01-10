// Tower/Subsystem/GroundItemSubsystem.cpp

#include "Tower/Subsystem/GroundItemSubsystem.h"
#include "Tower/Actors/ISMContainerActor.h"
#include "Item/ItemInstance.h"
#include "Engine/World.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "DrawDebugHelpers.h"

DECLARE_LOG_CATEGORY_EXTERN(LogGroundItemSubsystem, Log, All);
DEFINE_LOG_CATEGORY(LogGroundItemSubsystem);

// ═══════════════════════════════════════════════════════════════════════
// SUBSYSTEM LIFECYCLE
// ═══════════════════════════════════════════════════════════════════════

void UGroundItemSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	UE_LOG(LogGroundItemSubsystem, Log, TEXT("GroundItemSubsystem: Initialized (container will be created on first use)"));
	
	// DON'T create container here - world might not be ready!
}

void UGroundItemSubsystem::Deinitialize()
{
	ClearAllItems();
	
	if (ISMContainerActor)
	{
		ISMContainerActor->Destroy();
		ISMContainerActor = nullptr;
	}
	
	Super::Deinitialize();
	
	UE_LOG(LogGroundItemSubsystem, Log, TEXT("GroundItemSubsystem: Deinitialized"));
}

// ═══════════════════════════════════════════════════════════════════════
// ISM MANAGEMENT
// ═══════════════════════════════════════════════════════════════════════

void UGroundItemSubsystem::EnsureISMContainerExists()
{
	// Already exists
	if (ISMContainerActor && IsValid(ISMContainerActor))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogGroundItemSubsystem, Error, TEXT("EnsureISMContainerExists: World is null!"));
		return;
	}

	// Check if world is ready for spawning
	if (!World->HasBegunPlay())
	{
		UE_LOG(LogGroundItemSubsystem, Warning, TEXT("EnsureISMContainerExists: World hasn't begun play yet, deferring creation"));
		return;
	}

	// Spawn the container actor
	FActorSpawnParameters Params;
	Params.Name = FName("GroundItems_ISMContainer");
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags = RF_Transient; // Don't save this actor
	
	ISMContainerActor = World->SpawnActor<AISMContainerActor>(
		AISMContainerActor::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		Params
	);
	
	if (ISMContainerActor)
	{
		UE_LOG(LogGroundItemSubsystem, Log, TEXT("GroundItemSubsystem: Created ISM container actor"));
	}
	else
	{
		UE_LOG(LogGroundItemSubsystem, Error, TEXT("GroundItemSubsystem: Failed to spawn ISM container actor!"));
	}
}

UInstancedStaticMeshComponent* UGroundItemSubsystem::GetOrCreateISMComponent(UStaticMesh* Mesh)
{
	if (!Mesh)
	{
		return nullptr;
	}

	// Check cache first
	if (UInstancedStaticMeshComponent** FoundISM = MeshToISM.Find(Mesh))
	{
		if (*FoundISM && IsValid(*FoundISM))
		{
			return *FoundISM;
		}
	}

	// Need to create new ISM component
	EnsureISMContainerExists();
	
	if (!ISMContainerActor)
	{
		return nullptr;
	}

	// Create new ISM component on the container actor
	UInstancedStaticMeshComponent* NewISM = NewObject<UInstancedStaticMeshComponent>(
		ISMContainerActor,
		*FString::Printf(TEXT("ISM_%s"), *Mesh->GetName())
	);
	
	NewISM->SetStaticMesh(Mesh);
	NewISM->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	NewISM->SetCollisionResponseToAllChannels(ECR_Ignore);
	NewISM->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	NewISM->RegisterComponent();
	NewISM->AttachToComponent(ISMContainerActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

	// Cache it
	MeshToISM.Add(Mesh, NewISM);

	UE_LOG(LogGroundItemSubsystem, Log, TEXT("Created ISM component for mesh: %s"), *Mesh->GetName());

	return NewISM;
}

void UGroundItemSubsystem::ReindexAfterRemoval(UInstancedStaticMeshComponent* ISMComponent, int32 RemovedIndex)
{
	// FIX: When an ISM instance is removed at index N, all instances at index > N
	// are shifted down by 1. We need to update our tracking to match.
	
	for (TPair<int32, FGroundItemISMData>& Pair : ItemISMData)
	{
		FGroundItemISMData& Data = Pair.Value;
		
		// Only affect items in the same ISM component
		if (Data.ISMComponent != ISMComponent)
		{
			continue;
		}

		// If this item's index was higher than the removed index, shift it down
		if (Data.InstanceIndex > RemovedIndex)
		{
			Data.InstanceIndex--;
			
			UE_LOG(LogGroundItemSubsystem, Verbose, TEXT("ReindexAfterRemoval: Item %d index shifted from %d to %d"),
				Pair.Key, Data.InstanceIndex + 1, Data.InstanceIndex);
		}
	}
}

// ═══════════════════════════════════════════════════════════════════════
// PRIMARY API
// ═══════════════════════════════════════════════════════════════════════

int32 UGroundItemSubsystem::AddItemToGround(UItemInstance* Item, FVector Location, FRotator Rotation)
{
	// Ensure container exists before adding items
	EnsureISMContainerExists();
	
	if (!ISMContainerActor)
	{
		UE_LOG(LogGroundItemSubsystem, Error, TEXT("AddItemToGround: Cannot add item - no container actor!"));
		return -1;
	}

	if (!Item || !Item->HasValidBaseData())
	{
		UE_LOG(LogGroundItemSubsystem, Warning, TEXT("AddItemToGround: Invalid item!"));
		return -1;
	}

	UStaticMesh* Mesh = Item->GetGroundMesh();
	if (!Mesh)
	{
		UE_LOG(LogGroundItemSubsystem, Warning, TEXT("AddItemToGround: Item has no ground mesh!"));
		return -1;
	}

	// Get or create ISM for this mesh type
	UInstancedStaticMeshComponent* ISM = GetOrCreateISMComponent(Mesh);
	if (!ISM)
	{
		UE_LOG(LogGroundItemSubsystem, Error, TEXT("AddItemToGround: Failed to get/create ISM component!"));
		return -1;
	}

	// Add instance to ISM
	FTransform Transform(Rotation, Location, FVector::OneVector);
	int32 ISMInstanceIndex = ISM->AddInstance(Transform);

	if (ISMInstanceIndex == INDEX_NONE)
	{
		UE_LOG(LogGroundItemSubsystem, Error, TEXT("AddItemToGround: Failed to add instance to ISM!"));
		return -1;
	}

	// Generate unique ID
	int32 ItemID = NextItemID++;

	// Store all data
	GroundItems.Add(ItemID, Item);
	InstanceLocations.Add(ItemID, Location);
	
	// FIX: Use unified ISM tracking struct
	ItemISMData.Add(ItemID, FGroundItemISMData(ISM, ISMInstanceIndex, Mesh));

	UE_LOG(LogGroundItemSubsystem, Log, TEXT("AddItemToGround: Added item '%s' (ID: %d, ISMIndex: %d) at %s"), 
		*Item->GetDisplayName().ToString(), ItemID, ISMInstanceIndex, *Location.ToString());

	return ItemID;
}

UItemInstance* UGroundItemSubsystem::RemoveItemFromGround(int32 ItemID)
{
	// Find item
	UItemInstance** FoundItem = GroundItems.Find(ItemID);
	if (!FoundItem)
	{
		UE_LOG(LogGroundItemSubsystem, Warning, TEXT("RemoveItemFromGround: Item ID %d not found"), ItemID);
		return nullptr;
	}

	UItemInstance* Item = *FoundItem;

	// Find ISM data
	FGroundItemISMData* ISMData = ItemISMData.Find(ItemID);
	if (ISMData && ISMData->IsValid())
	{
		UInstancedStaticMeshComponent* ISM = ISMData->ISMComponent;
		int32 InstanceIndex = ISMData->InstanceIndex;

		// Remove the instance from ISM
		ISM->RemoveInstance(InstanceIndex);
		
		// FIX: Re-index all items in this ISM that had higher indices
		// This is critical - without this, subsequent removals fail!
		ReindexAfterRemoval(ISM, InstanceIndex);
		
		UE_LOG(LogGroundItemSubsystem, Log, TEXT("RemoveItemFromGround: Removed item ID %d (ISMIndex was %d)"), 
			ItemID, InstanceIndex);
	}
	else
	{
		UE_LOG(LogGroundItemSubsystem, Warning, TEXT("RemoveItemFromGround: No valid ISM data for item ID %d"), ItemID);
	}

	// Clean up tracking data
	GroundItems.Remove(ItemID);
	InstanceLocations.Remove(ItemID);
	ItemISMData.Remove(ItemID);

	return Item;
}

UItemInstance* UGroundItemSubsystem::GetItemByID(int32 ItemID) const
{
	UItemInstance* const* FoundItem = GroundItems.Find(ItemID);
	return FoundItem ? *FoundItem : nullptr;
}

UItemInstance* UGroundItemSubsystem::GetNearestItem(FVector Location, float MaxDistance, int32& OutItemID)
{
	OutItemID = -1;
	float ClosestDistSq = MaxDistance * MaxDistance;
	UItemInstance* ClosestItem = nullptr;

	for (const TPair<int32, FVector>& Pair : InstanceLocations)
	{
		float DistSq = FVector::DistSquared(Location, Pair.Value);
		
		if (DistSq < ClosestDistSq)
		{
			ClosestDistSq = DistSq;
			OutItemID = Pair.Key;
			
			if (UItemInstance* const* Item = GroundItems.Find(Pair.Key))
			{
				ClosestItem = *Item;
			}
		}
	}

	return ClosestItem;
}

int32 UGroundItemSubsystem::GetItemsInRadius(FVector Location, float Radius, TArray<int32>& OutItemIDs)
{
	OutItemIDs.Reset();
	float RadiusSq = Radius * Radius;

	for (const TPair<int32, FVector>& Pair : InstanceLocations)
	{
		float DistSq = FVector::DistSquared(Location, Pair.Value);
		
		if (DistSq <= RadiusSq)
		{
			OutItemIDs.Add(Pair.Key);
		}
	}

	return OutItemIDs.Num();
}

TArray<UItemInstance*> UGroundItemSubsystem::GetItemInstancesInRadius(FVector Location, float Radius)
{
	TArray<UItemInstance*> ItemsInRange;
	float RadiusSq = Radius * Radius;

	for (const TPair<int32, FVector>& Pair : InstanceLocations)
	{
		float DistSq = FVector::DistSquared(Location, Pair.Value);
		
		if (DistSq <= RadiusSq)
		{
			if (UItemInstance* const* Found = GroundItems.Find(Pair.Key))
			{
				ItemsInRange.Add(*Found);
			}
		}
	}

	return ItemsInRange;
}

int32 UGroundItemSubsystem::GetInstanceID(UItemInstance* Item) const
{
	if (!Item)
	{
		return -1;
	}

	for (const TPair<int32, UItemInstance*>& Pair : GroundItems)
	{
		if (Pair.Value == Item)
		{
			return Pair.Key;
		}
	}
	
	return -1;
}

void UGroundItemSubsystem::UpdateItemLocation(int32 ItemID, FVector NewLocation)
{
	// Find ISM data
	FGroundItemISMData* ISMData = ItemISMData.Find(ItemID);
	if (!ISMData || !ISMData->IsValid())
	{
		UE_LOG(LogGroundItemSubsystem, Warning, TEXT("UpdateItemLocation: No valid ISM data for item ID %d"), ItemID);
		return;
	}

	UInstancedStaticMeshComponent* ISM = ISMData->ISMComponent;
	int32 InstanceIndex = ISMData->InstanceIndex;

	// Get current transform
	FTransform CurrentTransform;
	ISM->GetInstanceTransform(InstanceIndex, CurrentTransform, true);

	// Update location
	FTransform NewTransform = CurrentTransform;
	NewTransform.SetLocation(NewLocation);

	// Apply to ISM
	ISM->UpdateInstanceTransform(InstanceIndex, NewTransform, true);

	// Update tracking
	InstanceLocations.Add(ItemID, NewLocation);

	UE_LOG(LogGroundItemSubsystem, Verbose, TEXT("UpdateItemLocation: Item %d moved to %s"), ItemID, *NewLocation.ToString());
}

void UGroundItemSubsystem::ClearAllItems()
{
	// Clear all ISM instances
	for (TPair<UStaticMesh*, UInstancedStaticMeshComponent*>& Pair : MeshToISM)
	{
		if (Pair.Value && IsValid(Pair.Value))
		{
			Pair.Value->ClearInstances();
		}
	}

	// Clear tracking data
	GroundItems.Empty();
	InstanceLocations.Empty();
	ItemISMData.Empty();
	
	// Don't reset NextItemID - keep it incrementing for uniqueness

	UE_LOG(LogGroundItemSubsystem, Log, TEXT("ClearAllItems: All ground items cleared"));
}

// ═══════════════════════════════════════════════════════════════════════
// DEBUG
// ═══════════════════════════════════════════════════════════════════════

#if WITH_EDITOR
void UGroundItemSubsystem::DebugDrawAllItems(float Duration)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (const TPair<int32, FVector>& Pair : InstanceLocations)
	{
		DrawDebugSphere(World, Pair.Value, 25.0f, 8, FColor::Yellow, false, Duration);
		
		if (UItemInstance* const* Found = GroundItems.Find(Pair.Key))
		{
			FString DebugText = FString::Printf(TEXT("[%d] %s"), Pair.Key, *(*Found)->GetDisplayName().ToString());
			DrawDebugString(World, Pair.Value + FVector(0, 0, 50), DebugText, nullptr, FColor::White, Duration);
		}
	}
	
	UE_LOG(LogGroundItemSubsystem, Log, TEXT("DebugDrawAllItems: Drew %d items for %.1fs"), InstanceLocations.Num(), Duration);
}
#endif