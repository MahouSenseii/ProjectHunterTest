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
	
	Super::Deinitialize();
	
	UE_LOG(LogLootSubsystem, Log, TEXT("LootSubsystem deinitialized"));
}

// ═══════════════════════════════════════════════════════════════════════
// PRIMARY GENERATION API
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
	
	// Apply global modifiers
	FinalSettings = ApplyGlobalModifiers(FinalSettings);
	
	// Apply player modifiers
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

FLootResultBatch ULootSubsystem::GenerateLootByID(
	FName SourceID,
	float PlayerLuck,
	float PlayerMagicFind,
	int32 Seed)
{
	FLootRequest Request(SourceID);
	Request.PlayerLuck = PlayerLuck;
	Request.PlayerMagicFind = PlayerMagicFind;
	Request.Seed = Seed;
	
	return GenerateLoot(Request);
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
// SPAWNING API
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
	
	TArray<FLootSourceEntry*> AllEntries;
	CachedRegistry->GetAllRows<FLootSourceEntry>(TEXT("GetSourceIDsByCategory"), AllEntries);
	
	TArray<FName> AllNames = CachedRegistry->GetRowNames();
	
	for (int32 i = 0; i < AllEntries.Num(); i++)
	{
		if (AllEntries[i] && AllEntries[i]->Category == Category)
		{
			SourceIDs.Add(AllNames[i]);
		}
	}
	
	return SourceIDs;
}

TArray<FName> ULootSubsystem::GetSourceIDsByTag(FName Tag) const
{
	TArray<FName> SourceIDs;
	
	if (!CachedRegistry)
	{
		return SourceIDs;
	}
	
	TArray<FLootSourceEntry*> AllEntries;
	CachedRegistry->GetAllRows<FLootSourceEntry>(TEXT("GetSourceIDsByTag"), AllEntries);
	
	TArray<FName> AllNames = CachedRegistry->GetRowNames();
	
	for (int32 i = 0; i < AllEntries.Num(); i++)
	{
		if (AllEntries[i] && AllEntries[i]->HasTag(Tag))
		{
			SourceIDs.Add(AllNames[i]);
		}
	}
	
	return SourceIDs;
}

// ═══════════════════════════════════════════════════════════════════════
// LOOT TABLE MANAGEMENT
// ═══════════════════════════════════════════════════════════════════════

void ULootSubsystem::PreloadLootTable(FName SourceID)
{
	FLootSourceEntry Source;
	if (!GetSourceEntry(SourceID, Source))
	{
		UE_LOG(LogLootSubsystem, Warning, TEXT("PreloadLootTable: Source '%s' not found"),
			*SourceID.ToString());
		return;
	}
	
	// Already loaded?
	if (CachedLootTables.Contains(SourceID))
	{
		return;
	}
	
	// Load synchronously (for now - could be async)
	UDataTable* LoadedTable = Source.LootTable.LoadSynchronous();
	
	if (LoadedTable)
	{
		CachedLootTables.Add(SourceID, LoadedTable);
		OnLootTableLoaded.Broadcast(SourceID, true);
		
		UE_LOG(LogLootSubsystem, Log, TEXT("Preloaded loot table for '%s'"), *SourceID.ToString());
	}
	else
	{
		OnLootTableLoaded.Broadcast(SourceID, false);
		
		UE_LOG(LogLootSubsystem, Warning, TEXT("Failed to preload loot table for '%s'"),
			*SourceID.ToString());
	}
}

void ULootSubsystem::PreloadLootTables(const TArray<FName>& SourceIDs)
{
	for (FName SourceID : SourceIDs)
	{
		PreloadLootTable(SourceID);
	}
}

void ULootSubsystem::UnloadLootTable(FName SourceID)
{
	CachedLootTables.Remove(SourceID);
	
	UE_LOG(LogLootSubsystem, Log, TEXT("Unloaded loot table for '%s'"), *SourceID.ToString());
}

void ULootSubsystem::ClearLootTableCache()
{
	CachedLootTables.Empty();
	
	UE_LOG(LogLootSubsystem, Log, TEXT("Cleared loot table cache"));
}

bool ULootSubsystem::IsLootTableLoaded(FName SourceID) const
{
	return CachedLootTables.Contains(SourceID);
}

// ═══════════════════════════════════════════════════════════════════════
// CORRUPTION QUERIES
// ═══════════════════════════════════════════════════════════════════════

FLootResultBatch ULootSubsystem::GenerateCorruptedLoot(const FLootRequest& Request)
{
	// Create modified request that forces corrupted drops
	FLootRequest ModifiedRequest = Request;
	ModifiedRequest.bUseOverrideSettings = true;
	ModifiedRequest.OverrideSettings.bOnlyCorruptedDrops = true;
	
	return GenerateLoot(ModifiedRequest);
}

FLootResultBatch ULootSubsystem::GenerateLootByCorruptionType(
	const FLootRequest& Request,
	ECorruptionType CorruptionType)
{
	FLootResultBatch Batch;
	Batch.SourceID = Request.SourceID;
	
	FLootSourceEntry Source;
	if (!GetSourceEntry(Request.SourceID, Source))
	{
		return Batch;
	}
	
	const FLootTable* LootTable = GetLootTableFromSource(Source);
	if (!LootTable)
	{
		return Batch;
	}
	
	// Build settings and apply modifiers
	FLootDropSettings FinalSettings = BuildFinalSettings(Source, Request);
	FinalSettings = ApplyGlobalModifiers(FinalSettings);
	FinalSettings = ApplyPlayerModifiers(FinalSettings, Request.PlayerLuck, Request.PlayerMagicFind);
	
	int32 Seed = Request.Seed != 0 ? Request.Seed : FMath::Rand();
	
	// Generate filtered by corruption type
	Batch = LootGenerator.GenerateLootByCorruptionType(
		*LootTable,
		FinalSettings,
		CorruptionType,
		Seed,
		GetWorld()
	);
	
	Batch.SourceID = Request.SourceID;
	Batch.GenerationSeed = Seed;
	
	return Batch;
}

// ═══════════════════════════════════════════════════════════════════════
// UTILITY
// ═══════════════════════════════════════════════════════════════════════

FLootDropSettings ULootSubsystem::ApplyPlayerModifiers(
	const FLootDropSettings& BaseSettings,
	float Luck,
	float MagicFind) const
{
	FLootDropSettings Modified = BaseSettings;
	
	Modified.PlayerLuckBonus = Luck;
	Modified.PlayerMagicFindBonus = MagicFind;
	
	return Modified;
}

void ULootSubsystem::SetRegistryPath(TSoftObjectPtr<UDataTable> RegistryPath)
{
	LootSourceRegistryPath = RegistryPath;
}

void ULootSubsystem::ReloadRegistry()
{
	CachedRegistry = nullptr;
	ClearLootTableCache();
	LoadRegistry();
}

// ═══════════════════════════════════════════════════════════════════════
// DEBUG
// ═══════════════════════════════════════════════════════════════════════

#if WITH_EDITOR
void ULootSubsystem::DebugPrintRegistry() const
{
	if (!CachedRegistry)
	{
		UE_LOG(LogLootSubsystem, Warning, TEXT("DebugPrintRegistry: No registry loaded"));
		return;
	}
	
	TArray<FName> AllIDs = GetAllSourceIDs();
	
	UE_LOG(LogLootSubsystem, Log, TEXT("=== LOOT SOURCE REGISTRY (%d entries) ==="), AllIDs.Num());
	
	for (FName SourceID : AllIDs)
	{
		FLootSourceEntry Entry;
		if (GetSourceEntry(SourceID, Entry))
		{
			UE_LOG(LogLootSubsystem, Log, TEXT("  [%s] Category: %d, Level: %d, Enabled: %s"),
				*SourceID.ToString(),
				static_cast<int32>(Entry.Category),
				Entry.BaseLevel,
				Entry.bEnabled ? TEXT("Yes") : TEXT("No"));
		}
	}
}

void ULootSubsystem::DebugValidateTables() const
{
	if (!CachedRegistry)
	{
		UE_LOG(LogLootSubsystem, Warning, TEXT("DebugValidateTables: No registry loaded"));
		return;
	}
	
	TArray<FName> AllIDs = GetAllSourceIDs();
	int32 ValidCount = 0;
	int32 InvalidCount = 0;
	
	for (FName SourceID : AllIDs)
	{
		FLootSourceEntry Entry;
		if (GetSourceEntry(SourceID, Entry))
		{
			if (!Entry.LootTable.IsNull())
			{
				UDataTable* Table = Entry.LootTable.LoadSynchronous();
				if (Table)
				{
					ValidCount++;
				}
				else
				{
					UE_LOG(LogLootSubsystem, Warning, TEXT("  [%s] Failed to load table: %s"),
						*SourceID.ToString(),
						*Entry.LootTable.ToString());
					InvalidCount++;
				}
			}
			else
			{
				UE_LOG(LogLootSubsystem, Warning, TEXT("  [%s] No table reference"),
					*SourceID.ToString());
				InvalidCount++;
			}
		}
	}
	
	UE_LOG(LogLootSubsystem, Log, TEXT("Validation: %d valid, %d invalid"), ValidCount, InvalidCount);
}
#endif

// ═══════════════════════════════════════════════════════════════════════
// INTERNAL HELPERS
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
		UE_LOG(LogLootSubsystem, Log, TEXT("Loaded loot source registry: %s (%d entries)"),
			*CachedRegistry->GetName(),
			CachedRegistry->GetRowNames().Num());
	}
	else
	{
		UE_LOG(LogLootSubsystem, Warning, TEXT("Failed to load loot source registry from: %s"),
			*LootSourceRegistryPath.ToString());
	}
}

UDataTable* ULootSubsystem::GetOrLoadLootTable(FName SourceID)
{
	// Check cache first
	if (UDataTable** CachedTable = CachedLootTables.Find(SourceID))
	{
		return *CachedTable;
	}
	
	// Get source entry
	FLootSourceEntry Source;
	if (!GetSourceEntry(SourceID, Source))
	{
		return nullptr;
	}
	
	// Load table
	UDataTable* LoadedTable = Source.LootTable.LoadSynchronous();
	
	if (LoadedTable)
	{
		// Cache it
		CachedLootTables.Add(SourceID, LoadedTable);
		OnLootTableLoaded.Broadcast(SourceID, true);
	}
	
	return LoadedTable;
}

const FLootTable* ULootSubsystem::GetLootTableFromSource(const FLootSourceEntry& Source, FName RowName)
{
	if (Source.LootTable.IsNull())
	{
		return nullptr;
	}
	
	UDataTable* DataTable = Source.LootTable.LoadSynchronous();
	if (!DataTable)
	{
		return nullptr;
	}
	
	// If specific row requested, use that
	if (!RowName.IsNone())
	{
		return DataTable->FindRow<FLootTable>(RowName, TEXT("GetLootTableFromSource"));
	}
	
	// Otherwise get first row (common pattern for single-table sources)
	TArray<FName> RowNames = DataTable->GetRowNames();
	if (RowNames.Num() > 0)
	{
		return DataTable->FindRow<FLootTable>(RowNames[0], TEXT("GetLootTableFromSource"));
	}
	
	return nullptr;
}

FLootDropSettings ULootSubsystem::BuildFinalSettings(
	const FLootSourceEntry& Source,
	const FLootRequest& Request) const
{
	// Start with source defaults
	FLootDropSettings Settings = Source.DefaultSettings;
	
	// Override with request settings if specified
	if (Request.bUseOverrideSettings)
	{
		Settings = Request.OverrideSettings;
	}
	
	// Apply source rarity
	Settings.SourceRarity = Source.SourceRarity;
	
	// Override level if specified
	if (Request.OverrideLevel > 0)
	{
		Settings.SourceLevel = Request.OverrideLevel;
	}
	else
	{
		Settings.SourceLevel = Source.BaseLevel;
	}
	
	// Boss sources get guaranteed drops
	if (Source.bIsBoss)
	{
		Settings.MinDrops = FMath::Max(1, Settings.MinDrops);
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

bool ULootSubsystem::EnsureGroundItemSubsystem()
{
	if (CachedGroundItemSubsystem)
	{
		return true;
	}
	
	if (UWorld* World = GetWorld())
	{
		CachedGroundItemSubsystem = World->GetSubsystem<UGroundItemSubsystem>();
	}
	
	return CachedGroundItemSubsystem != nullptr;
}