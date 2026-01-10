// Loot/Subsystem/LootSubsystem.cpp

#include "Loot/Subsystem/LootSubsystem.h"
#include "Tower/Subsystem/GroundItemSubsystem.h"
#include "Item/ItemInstance.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY(LogLootSubsystem);

// ═══════════════════════════════════════════════════════════════════════
// SUBSYSTEM LIFECYCLE
// ═══════════════════════════════════════════════════════════════════════

void ULootSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	// Cache world reference for faster subsystem lookups
	CachedWorld = GetWorld();
	
	// Default registry path (can be overridden via config)
	if (LootSourceRegistryPath.IsNull())
	{
		LootSourceRegistryPath = TSoftObjectPtr<UDataTable>(
			FSoftObjectPath(TEXT("/Game/Data/Loot/DT_LootSourceRegistry"))
		);
	}
	
	LoadRegistry();
	
	UE_LOG(LogLootSubsystem, Log, TEXT("LootSubsystem initialized"));
}

void ULootSubsystem::Deinitialize()
{
	ClearLootTableCache();
	CachedRegistry = nullptr;
	CachedGroundItemSubsystem = nullptr;
	CachedWorld = nullptr;
	
	Super::Deinitialize();
	
	UE_LOG(LogLootSubsystem, Log, TEXT("LootSubsystem deinitialized"));
}

// ═══════════════════════════════════════════════════════════════════════
// PRIMARY API - GENERATION
// ═══════════════════════════════════════════════════════════════════════

FLootResultBatch ULootSubsystem::GenerateLoot(const FLootRequest& Request)
{
	FLootResultBatch Batch;
	Batch.SourceID = Request.SourceID;
	
	// Get source entry
	FLootSourceEntry Source;
	if (!GetSourceEntry(Request.SourceID, Source))
	{
		UE_LOG(LogLootSubsystem, Warning, TEXT("GenerateLoot: Source '%s' not found in registry"),
			*Request.SourceID.ToString());
		return Batch;
	}
	
	if (!Source.IsValid())
	{
		UE_LOG(LogLootSubsystem, Warning, TEXT("GenerateLoot: Source '%s' is disabled or invalid"),
			*Request.SourceID.ToString());
		return Batch;
	}
	
	// Get loot table
	const FLootTable* LootTable = GetLootTableFromSource(Source, Source.LootTableRowName);
	if (!LootTable)
	{
		UE_LOG(LogLootSubsystem, Warning, TEXT("GenerateLoot: Failed to load loot table for '%s'"),
			*Request.SourceID.ToString());
		return Batch;
	}
	
	// Build final settings
	FLootDropSettings FinalSettings = BuildFinalSettings(Source, Request);
	FinalSettings = ApplyGlobalModifiers(FinalSettings);
	FinalSettings = ApplyPlayerModifiers(FinalSettings, Request.PlayerLuck, Request.PlayerMagicFind);
	
	// Determine seed
	int32 Seed = Request.Seed != 0 ? Request.Seed : FMath::Rand();
	
	// Generate loot
	Batch = LootGenerator.GenerateLootWithSource(
		*LootTable,
		FinalSettings,
		Source.Category,
		Seed,
		GetWorld()
	);
	
	// Add currency and experience from source
	FRandomStream RandStream(Seed);
	if (Source.MaxCurrency > 0)
	{
		Batch.CurrencyDropped = RandStream.RandRange(Source.MinCurrency, Source.MaxCurrency);
		
		// Apply magic find to currency
		Batch.CurrencyDropped = FMath::RoundToInt(
			Batch.CurrencyDropped * (1.0f + Request.PlayerMagicFind * 0.005f)
		);
	}
	
	Batch.ExperienceReward = Source.ExperienceReward;
	Batch.SourceID = Request.SourceID;
	Batch.GenerationSeed = Seed;
	
	// Scale with player count if enabled
	if (Source.bScaleWithPlayerCount && Request.PlayerCount > 1)
	{
		float ScaleMultiplier = 1.0f + (Request.PlayerCount - 1) * 0.5f;
		Batch.CurrencyDropped = FMath::RoundToInt(Batch.CurrencyDropped * ScaleMultiplier);
	}
	
	// Broadcast event
	if (Batch.TotalItemCount > 0 || Batch.CurrencyDropped > 0)
	{
		OnLootGenerated.Broadcast(Batch, Request.SourceID);
	}
	
	UE_LOG(LogLootSubsystem, Log, TEXT("Generated loot from '%s': %d items, %d currency"),
		*Request.SourceID.ToString(), Batch.TotalItemCount, Batch.CurrencyDropped);
	
	return Batch;
}

FLootResultBatch ULootSubsystem::GenerateAndSpawnLoot(
	const FLootRequest& Request,
	const FLootSpawnSettings& SpawnSettings)
{
	FLootResultBatch Results = GenerateLoot(Request);
	
	if (Results.TotalItemCount > 0)
	{
		SpawnLootResults(Results, SpawnSettings);
	}
	
	return Results;
}

// ═══════════════════════════════════════════════════════════════════════
// PRIMARY API - SPAWNING
// ═══════════════════════════════════════════════════════════════════════

void ULootSubsystem::SpawnLootResults(
	const FLootResultBatch& Results,
	const FLootSpawnSettings& SpawnSettings)
{
	if (!EnsureGroundItemSubsystem())
	{
		UE_LOG(LogLootSubsystem, Error, TEXT("SpawnLootResults: GroundItemSubsystem unavailable"));
		return;
	}
	
	if (Results.Results.Num() == 0)
	{
		return;
	}
	
	// Calculate spawn positions
	TArray<FVector> Positions = CalculateScatterPositions(SpawnSettings, Results.Results.Num());
	
	// Spawn each item
	for (int32 i = 0; i < Results.Results.Num(); i++)
	{
		const FLootResult& Result = Results.Results[i];
		
		if (!Result.IsValid())
		{
			continue;
		}
		
		FVector ItemLocation = Positions.IsValidIndex(i) ? Positions[i] : SpawnSettings.SpawnLocation;
		
		int32 GroundItemID = CachedGroundItemSubsystem->AddItemToGround(Result.Item, ItemLocation);
		
		if (GroundItemID >= 0)
		{
			OnLootSpawned.Broadcast(Result.Item, ItemLocation, GroundItemID);
		}
		else
		{
			UE_LOG(LogLootSubsystem, Warning, TEXT("Failed to spawn item on ground"));
		}
	}
	
	UE_LOG(LogLootSubsystem, Log, TEXT("Spawned %d items at %s"),
		Results.Results.Num(), *SpawnSettings.SpawnLocation.ToString());
}

int32 ULootSubsystem::SpawnItemAtLocation(UItemInstance* Item, FVector Location)
{
	if (!Item)
	{
		return -1;
	}
	
	if (!EnsureGroundItemSubsystem())
	{
		return -1;
	}
	
	int32 GroundItemID = CachedGroundItemSubsystem->AddItemToGround(Item, Location);
	
	if (GroundItemID >= 0)
	{
		OnLootSpawned.Broadcast(Item, Location, GroundItemID);
	}
	
	return GroundItemID;
}

TArray<FVector> ULootSubsystem::CalculateScatterPositions(
	const FLootSpawnSettings& Settings,
	int32 NumItems) const
{
	TArray<FVector> Positions;
	Positions.Reserve(NumItems);
	
	if (NumItems <= 0)
	{
		return Positions;
	}
	
	FVector BaseLocation = Settings.SpawnLocation;
	BaseLocation.Z += Settings.HeightOffset;
	
	// Single item - spawn at center
	if (NumItems == 1)
	{
		Positions.Add(BaseLocation);
		return Positions;
	}
	
	if (Settings.bRandomScatter)
	{
		// Random scatter within radius
		for (int32 i = 0; i < NumItems; i++)
		{
			float RandomAngle = FMath::FRand() * 2.0f * PI;
			float RandomRadius = FMath::FRand() * Settings.ScatterRadius;
			
			FVector Offset(
				FMath::Cos(RandomAngle) * RandomRadius,
				FMath::Sin(RandomAngle) * RandomRadius,
				0.0f
			);
			
			Positions.Add(BaseLocation + Offset);
		}
	}
	else
	{
		// Organized circle pattern
		float AngleStep = 2.0f * PI / NumItems;
		
		for (int32 i = 0; i < NumItems; i++)
		{
			float Angle = i * AngleStep;
			
			FVector Offset(
				FMath::Cos(Angle) * Settings.ScatterRadius,
				FMath::Sin(Angle) * Settings.ScatterRadius,
				0.0f
			);
			
			Positions.Add(BaseLocation + Offset);
		}
	}
	
	return Positions;
}

// ═══════════════════════════════════════════════════════════════════════
// REGISTRY QUERIES
// ═══════════════════════════════════════════════════════════════════════

bool ULootSubsystem::IsSourceRegistered(FName SourceID) const
{
	if (!CachedRegistry)
	{
		return false;
	}
	
	return CachedRegistry->FindRow<FLootSourceEntry>(SourceID, TEXT("IsSourceRegistered")) != nullptr;
}

bool ULootSubsystem::GetSourceEntry(FName SourceID, FLootSourceEntry& OutEntry) const
{
	if (!CachedRegistry)
	{
		return false;
	}
	
	const FLootSourceEntry* Entry = CachedRegistry->FindRow<FLootSourceEntry>(
		SourceID, TEXT("GetSourceEntry"));
	
	if (Entry)
	{
		OutEntry = *Entry;
		return true;
	}
	
	return false;
}

TArray<FName> ULootSubsystem::GetAllSourceIDs() const
{
	TArray<FName> SourceIDs;
	
	if (!CachedRegistry)
	{
		return SourceIDs;
	}
	
	SourceIDs = CachedRegistry->GetRowNames();
	return SourceIDs;
}

TArray<FName> ULootSubsystem::GetSourceIDsByCategory(ELootSourceType Category) const
{
	TArray<FName> SourceIDs;
	
	if (!CachedRegistry)
	{
		return SourceIDs;
	}
	
	const TMap<FName, uint8*>& RowMap = CachedRegistry->GetRowMap();
	
	for (const TPair<FName, uint8*>& Pair : RowMap)
	{
		const FLootSourceEntry* Entry = reinterpret_cast<FLootSourceEntry*>(Pair.Value);
		if (Entry && Entry->Category == Category)
		{
			SourceIDs.Add(Pair.Key);
		}
	}
	
	return SourceIDs;
}

// ═══════════════════════════════════════════════════════════════════════
// CACHE MANAGEMENT
// ═══════════════════════════════════════════════════════════════════════

void ULootSubsystem::PreloadLootTables(const TArray<FName>& SourceIDs)
{
	for (const FName& SourceID : SourceIDs)
	{
		FLootSourceEntry Source;
		if (GetSourceEntry(SourceID, Source))
		{
			LoadLootTableAsync(Source);
		}
	}
	
	UE_LOG(LogLootSubsystem, Log, TEXT("Preloading %d loot tables"), SourceIDs.Num());
}

void ULootSubsystem::ClearLootTableCache()
{
	LootTableCache.Empty();
	UE_LOG(LogLootSubsystem, Log, TEXT("Loot table cache cleared"));
}

// ═══════════════════════════════════════════════════════════════════════
// INTERNAL - REGISTRY
// ═══════════════════════════════════════════════════════════════════════

void ULootSubsystem::LoadRegistry()
{
	if (LootSourceRegistryPath.IsNull())
	{
		UE_LOG(LogLootSubsystem, Warning, TEXT("LoadRegistry: No registry path configured"));
		return;
	}
	
	CachedRegistry = LootSourceRegistryPath.LoadSynchronous();
	
	if (CachedRegistry)
	{
		UE_LOG(LogLootSubsystem, Log, TEXT("Loaded loot registry with %d sources"), 
			CachedRegistry->GetRowNames().Num());
	}
	else
	{
		UE_LOG(LogLootSubsystem, Error, TEXT("Failed to load loot registry from '%s'"),
			*LootSourceRegistryPath.ToString());
	}
}

// ═══════════════════════════════════════════════════════════════════════
// INTERNAL - LOOT TABLE LOADING
// ═══════════════════════════════════════════════════════════════════════

const FLootTable* ULootSubsystem::GetLootTableFromSource(const FLootSourceEntry& Source, FName RowName)
{
	if (Source.LootTable.IsNull())
	{
		return nullptr;
	}
	
	// Check cache first
	UDataTable* CachedTable;
	if (UDataTable** FoundTable = LootTableCache.Find(Source.LootTableRowName))
	{
		CachedTable = *FoundTable;
	}
	else
	{
		// Load synchronously
		CachedTable = Source.LootTable.LoadSynchronous();
		
		if (CachedTable)
		{
			LootTableCache.Add(Source.LootTableRowName, CachedTable);
			OnLootTableLoaded.Broadcast(Source.LootTableRowName, true);
		}
		else
		{
			OnLootTableLoaded.Broadcast(Source.LootTableRowName, false);
			return nullptr;
		}
	}
	
	if (!CachedTable)
	{
		return nullptr;
	}
	
	// Get the specific row
	return CachedTable->FindRow<FLootTable>(RowName, TEXT("GetLootTableFromSource"));
}

bool ULootSubsystem::LoadLootTableAsync(const FLootSourceEntry& Source)
{
	// For now, just load synchronously
	// TODO: Implement async loading with FStreamableManager if needed
	const FLootTable* Table = GetLootTableFromSource(Source, Source.LootTableRowName);
	return Table != nullptr;
}

// ═══════════════════════════════════════════════════════════════════════
// INTERNAL - SETTINGS BUILDING
// ═══════════════════════════════════════════════════════════════════════

FLootDropSettings ULootSubsystem::BuildFinalSettings(
	const FLootSourceEntry& Source, 
	const FLootRequest& Request) const
{
	FLootDropSettings Settings = Source.DefaultSettings;
	
	// Apply level override
	if (Request.OverrideLevel > 0)
	{
		Settings.SourceLevel = Request.OverrideLevel;
	}
	else if (Source.DefaultSettings.SourceLevel > 0)
	{
		Settings.SourceLevel = Source.DefaultSettings.SourceLevel;
	}
	
	return Settings;
}

FLootDropSettings ULootSubsystem::ApplyGlobalModifiers(const FLootDropSettings& Settings) const
{
	FLootDropSettings Modified = Settings;
	
	Modified.DropChanceMultiplier *= GlobalDropChanceMultiplier;
	Modified.QuantityMultiplier *= GlobalQuantityMultiplier;
	Modified.CorruptionChance = FMath::Clamp(
		Modified.CorruptionChance + GlobalCorruptionChanceModifier, 
		0.0f, 1.0f
	);
	
	return Modified;
}

FLootDropSettings ULootSubsystem::ApplyPlayerModifiers(
	const FLootDropSettings& Settings, 
	float Luck, 
	float MagicFind) const
{
	FLootDropSettings Modified = Settings;
	
	// Luck affects rarity (increases chance of better drops)
	// Each point of luck = +1% rarity bonus chance
	Modified.RarityBonusChance += Luck * 0.01f;
	
	// Magic Find affects quantity
	// Each point = +0.5% quantity
	Modified.QuantityMultiplier *= (1.0f + MagicFind * 0.005f);
	
	// Store for use in generation
	Modified.PlayerLuckBonus = Luck;
	Modified.PlayerMagicFindBonus = MagicFind;
	
	return Modified;
}

// ═══════════════════════════════════════════════════════════════════════
// INTERNAL - SUBSYSTEM CACHING
// ═══════════════════════════════════════════════════════════════════════

bool ULootSubsystem::EnsureGroundItemSubsystem()
{
	if (CachedGroundItemSubsystem && IsValid(CachedGroundItemSubsystem))
	{
		return true;
	}
	
	// Use cached world for faster lookup
	if (!CachedWorld)
	{
		CachedWorld = GetWorld();
	}
	
	if (CachedWorld)
	{
		CachedGroundItemSubsystem = CachedWorld->GetSubsystem<UGroundItemSubsystem>();
	}
	
	return CachedGroundItemSubsystem != nullptr;
}