// World/Subsystem/GroundItemSubsystem.cpp

#include "Tower/Subsystem/GroundItemSubsystem.h"
#include "Tower/Actors/ISMContainerActor.h"
#include "Item/ItemInstance.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Components/InstancedStaticMeshComponent.h"

void UGroundItemSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	UE_LOG(LogTemp, Log, TEXT("GroundItemSubsystem: Initialized (container will be created on first use)"));
	
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
	
	UE_LOG(LogTemp, Log, TEXT("GroundItemSubsystem: Deinitialized"));
}

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
		UE_LOG(LogTemp, Error, TEXT("GroundItemSubsystem::EnsureISMContainerExists - World is null!"));
		return;
	}

	// Check if world is ready for spawning
	if (!World->HasBegunPlay())
	{
		UE_LOG(LogTemp, Warning, TEXT("GroundItemSubsystem::EnsureISMContainerExists - World hasn't begun play yet, deferring creation"));
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
		UE_LOG(LogTemp, Log, TEXT("GroundItemSubsystem: Created ISM container actor"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GroundItemSubsystem: Failed to spawn ISM container actor!"));
	}
}

int32 UGroundItemSubsystem::AddItemToGround(UItemInstance* Item, FVector Location, FRotator Rotation)
{
	// Ensure container exists before adding items
	EnsureISMContainerExists();
	
	if (!ISMContainerActor)
	{
		UE_LOG(LogTemp, Error, TEXT("GroundItemSubsystem: Cannot add item - no container actor!"));
		return -1;
	}

	if (!Item || !Item->HasValidBaseData())
	{
		UE_LOG(LogTemp, Warning, TEXT("GroundItemSubsystem: Invalid item!"));
		return -1;
	}

	UStaticMesh* Mesh = Item->GetGroundMesh();
	if (!Mesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("GroundItemSubsystem: Item has no ground mesh!"));
		return -1;
	}

	// Get or create ISM for this mesh type
	UInstancedStaticMeshComponent* ISM = GetOrCreateISMComponent(Mesh);
	if (!ISM)
	{
		UE_LOG(LogTemp, Error, TEXT("GroundItemSubsystem: Failed to get/create ISM component!"));
		return -1;
	}

	// Add instance to ISM
	FTransform Transform(Rotation, Location, FVector::OneVector);
	int32 ISMInstanceIndex = ISM->AddInstance(Transform);

	if (ISMInstanceIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Error, TEXT("GroundItemSubsystem: Failed to add instance to ISM!"));
		return -1;
	}

	// Generate unique ID
	int32 ItemID = NextItemID++;

	// Store mappings
	GroundItems.Add(ItemID, Item);
	InstanceToComponent.Add(ItemID, ISM);
	InstanceLocations.Add(ItemID, Location);
	ItemToInstanceIndex.Add(ItemID, ISMInstanceIndex);

	UE_LOG(LogTemp, Log, TEXT("GroundItemSubsystem: Added item '%s' (ID: %d) at %s"), 
		*Item->GetDisplayName().ToString(), ItemID, *Location.ToString());

	return ItemID;
}

UItemInstance* UGroundItemSubsystem::RemoveItemFromGround(int32 ItemID)
{
	UItemInstance** FoundItem = GroundItems.Find(ItemID);
	if (!FoundItem)
	{
		UE_LOG(LogTemp, Warning, TEXT("GroundItemSubsystem: Item ID %d not found"), ItemID);
		return nullptr;
	}

	UItemInstance* Item = *FoundItem;

	// Find and remove from ISM
	if (UInstancedStaticMeshComponent** FoundISM = InstanceToComponent.Find(ItemID))
	{
		if (int32* FoundIndex = ItemToInstanceIndex.Find(ItemID))
		{
			(*FoundISM)->RemoveInstance(*FoundIndex);
			
			UE_LOG(LogTemp, Log, TEXT("GroundItemSubsystem: Removed item ID %d from ISM"), ItemID);
		}
	}

	// Remove from tracking
	GroundItems.Remove(ItemID);
	InstanceToComponent.Remove(ItemID);
	InstanceLocations.Remove(ItemID);
	ItemToInstanceIndex.Remove(ItemID);

	return Item;
}

UItemInstance* UGroundItemSubsystem::GetNearestItem(FVector Location, float MaxDistance, int32& OutItemID)
{
	float ClosestDistSq = MaxDistance * MaxDistance;
	UItemInstance* ClosestItem = nullptr;
	OutItemID = -1;

	for (const TPair<int32, FVector>& Pair : InstanceLocations)
	{
		float DistSq = FVector::DistSquared(Location, Pair.Value);
		if (DistSq < ClosestDistSq)
		{
			ClosestDistSq = DistSq;
			OutItemID = Pair.Key;
			if (UItemInstance** Found = GroundItems.Find(Pair.Key))
			{
				ClosestItem = *Found;
			}
		}
	}

	return ClosestItem;
}

TArray<UItemInstance*> UGroundItemSubsystem::GetItemsInRadius(FVector Location, float Radius)
{
	TArray<UItemInstance*> ItemsInRange;
	float RadiusSq = Radius * Radius;

	for (const TPair<int32, FVector>& Pair : InstanceLocations)
	{
		float DistSq = FVector::DistSquared(Location, Pair.Value);
		if (DistSq <= RadiusSq)
		{
			if (UItemInstance** Found = GroundItems.Find(Pair.Key))
			{
				ItemsInRange.Add(*Found);
			}
		}
	}

	return ItemsInRange;
}

void UGroundItemSubsystem::UpdateItemLocation(int32 ItemID, FVector NewLocation)
{
	if (UInstancedStaticMeshComponent** FoundISM = InstanceToComponent.Find(ItemID))
	{
		if (int32* FoundIndex = ItemToInstanceIndex.Find(ItemID))
		{
			FTransform CurrentTransform;
			(*FoundISM)->GetInstanceTransform(*FoundIndex, CurrentTransform, true);
			
			FTransform NewTransform = CurrentTransform;
			NewTransform.SetLocation(NewLocation);
			
			(*FoundISM)->UpdateInstanceTransform(*FoundIndex, NewTransform, true);
			InstanceLocations.Add(ItemID, NewLocation);
		}
	}
}

void UGroundItemSubsystem::ClearAllItems()
{
	// Remove all ISM instances
	for (const auto& Pair : MeshComponents)
	{
		if (Pair.Value)
		{
			Pair.Value->ClearInstances();
		}
	}

	GroundItems.Empty();
	InstanceToComponent.Empty();
	InstanceLocations.Empty();
	ItemToInstanceIndex.Empty();

	UE_LOG(LogTemp, Log, TEXT("GroundItemSubsystem: Cleared all items"));
}

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
		
		if (UItemInstance** Found = GroundItems.Find(Pair.Key))
		{
			DrawDebugString(World, Pair.Value + FVector(0, 0, 50), 
				(*Found)->GetDisplayName().ToString(), nullptr, FColor::White, Duration);
		}
	}
}

UInstancedStaticMeshComponent* UGroundItemSubsystem::GetOrCreateISMComponent(UStaticMesh* Mesh)
{
	if (!Mesh)
	{
		UE_LOG(LogTemp, Error, TEXT("GroundItemSubsystem::GetOrCreateISMComponent - Mesh is null!"));
		return nullptr;
	}

	// Check if we already have an ISM for this mesh type
	if (UInstancedStaticMeshComponent** Found = MeshComponents.Find(Mesh))
	{
		return *Found;
	}

	if (!ISMContainerActor)
	{
		UE_LOG(LogTemp, Error, TEXT("GroundItemSubsystem::GetOrCreateISMComponent - No ISM container actor!"));
		return nullptr;
	}

	// Create new ISM component
	UInstancedStaticMeshComponent* NewISM = NewObject<UInstancedStaticMeshComponent>(
		ISMContainerActor,
		*FString::Printf(TEXT("ISM_%s"), *Mesh->GetName())
	);

	if (!NewISM)
	{
		UE_LOG(LogTemp, Error, TEXT("GroundItemSubsystem::GetOrCreateISMComponent - Failed to create ISM component!"));
		return nullptr;
	}

	NewISM->SetStaticMesh(Mesh);
	NewISM->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	NewISM->SetCollisionResponseToAllChannels(ECR_Ignore);
	NewISM->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	NewISM->RegisterComponent();
	NewISM->AttachToComponent(
		ISMContainerActor->GetRootComponent(),
		FAttachmentTransformRules::KeepWorldTransform
	);

	MeshComponents.Add(Mesh, NewISM);

	UE_LOG(LogTemp, Log, TEXT("GroundItemSubsystem::GetOrCreateISMComponent - Created ISM for mesh: %s"), *Mesh->GetName());

	return NewISM;
}