// Loot/Generation/LootGenerator.cpp

#include "Loot/Generation/LootGenerator.h"
#include "Item/ItemInstance.h"
#include "Engine/DataTable.h"
#include "Loot/Library/LootEnum.h"

DEFINE_LOG_CATEGORY(LogLootGenerator);

// ═══════════════════════════════════════════════════════════════════════
// MAIN GENERATION FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════

FLootResultBatch FLootGenerator::GenerateLoot(
	const FLootTable& LootTable,
	const FLootDropSettings& Settings,
	int32 Seed,
	UObject* Outer) const
{
	FLootResultBatch Batch;
	
	// Use provided seed or generate new one
	if (Seed == 0)
	{
		Seed = FMath::Rand();
	}
	Batch.GenerationSeed = Seed;
	
	FRandomStream RandStream(Seed);
	
	// Filter entries based on settings
	TArray<FLootEntry> FilteredEntries = FilterEntries(LootTable.Entries, Settings);
	
	if (FilteredEntries.Num() == 0)
	{
		UE_LOG(LogLootGenerator, Verbose, TEXT("No valid entries after filtering"));
		return Batch;
	}
	
	// Calculate how many items to generate
	int32 DropCount = CalculateDropCount(LootTable, Settings, RandStream);
	
	if (DropCount <= 0)
	{
		return Batch;
	}
	
	// Select entries based on method
	TArray<int32> SelectedIndices;
	
	switch (LootTable.SelectionMethod)
	{
		case ELootSelectionMethod::LSM_Weighted:
			SelectedIndices = SelectWeighted(FilteredEntries, DropCount, LootTable.bAllowDuplicates, RandStream);
			break;
			
		case ELootSelectionMethod::LSM_Sequential:
			SelectedIndices = SelectSequential(FilteredEntries, Settings, RandStream);
			break;
			
		case ELootSelectionMethod::LSM_GuaranteedOne:
			SelectedIndices = SelectGuaranteedOne(FilteredEntries, RandStream);
			break;
			
		case ELootSelectionMethod::LSM_All:
			SelectedIndices = SelectAll(FilteredEntries, Settings, RandStream);
			break;
	}
	
	// Create items from selected entries
	for (int32 Index : SelectedIndices)
	{
		if (FilteredEntries.IsValidIndex(Index))
		{
			FLootResult Result = CreateItemFromEntry(FilteredEntries[Index], Settings, RandStream, Outer);
			Result.SourceEntryIndex = Index;
			
			if (Result.IsValid())
			{
				Batch.AddResult(Result);
			}
		}
	}
	
	UE_LOG(LogLootGenerator, Log, TEXT("Generated %d items from loot table"), Batch.TotalItemCount);
	
	return Batch;
}

FLootResultBatch FLootGenerator::GenerateLootFromHandle(
	const FDataTableRowHandle& TableHandle,
	const FLootDropSettings& Settings,
	int32 Seed,
	UObject* Outer) const
{
	const FLootTable* Table = GetLootTableFromHandle(TableHandle);
	
	if (!Table)
	{
		UE_LOG(LogLootGenerator, Warning, TEXT("Invalid loot table handle: %s"), 
			*TableHandle.RowName.ToString());
		return FLootResultBatch();
	}
	
	return GenerateLoot(*Table, Settings, Seed, Outer);
}

FLootResultBatch FLootGenerator::GenerateLootWithSource(
	const FLootTable& LootTable,
	const FLootDropSettings& Settings,
	ELootSourceType SourceType,
	int32 Seed,
	UObject* Outer) const
{
	FLootResultBatch Batch = GenerateLoot(LootTable, Settings, Seed, Outer);
	Batch.SourceType = SourceType;
	return Batch;
}

// ═══════════════════════════════════════════════════════════════════════
// FILTERED GENERATION
// ═══════════════════════════════════════════════════════════════════════

FLootResultBatch FLootGenerator::GenerateCorruptedLoot(
	const FLootTable& LootTable,
	const FLootDropSettings& Settings,
	int32 Seed,
	UObject* Outer) const
{
	// Create modified settings that only allow corrupted drops
	FLootDropSettings ModifiedSettings = Settings;
	ModifiedSettings.bOnlyCorruptedDrops = true;
	
	return GenerateLoot(LootTable, ModifiedSettings, Seed, Outer);
}

FLootResultBatch FLootGenerator::GenerateLootByCorruptionType(
	const FLootTable& LootTable,
	const FLootDropSettings& Settings,
	ECorruptionType CorruptionFilter,
	int32 Seed,
	UObject* Outer) const
{
	FLootResultBatch Batch;
	
	if (Seed == 0)
	{
		Seed = FMath::Rand();
	}
	Batch.GenerationSeed = Seed;
	
	FRandomStream RandStream(Seed);
	
	// Get entries matching corruption type
	TArray<FLootEntry> FilteredEntries = LootTable.GetEntriesByCorruptionType(CorruptionFilter);
	
	if (FilteredEntries.Num() == 0)
	{
		UE_LOG(LogLootGenerator, Verbose, TEXT("No entries with corruption type %d"), 
			static_cast<int32>(CorruptionFilter));
		return Batch;
	}
	
	// Generate from filtered entries
	int32 DropCount = CalculateDropCount(LootTable, Settings, RandStream);
	TArray<int32> SelectedIndices = SelectWeighted(FilteredEntries, DropCount, LootTable.bAllowDuplicates, RandStream);
	
	for (int32 Index : SelectedIndices)
	{
		if (FilteredEntries.IsValidIndex(Index))
		{
			FLootResult Result = CreateItemFromEntry(FilteredEntries[Index], Settings, RandStream, Outer);
			if (Result.IsValid())
			{
				Batch.AddResult(Result);
			}
		}
	}
	
	return Batch;
}

// ═══════════════════════════════════════════════════════════════════════
// SINGLE ITEM GENERATION
// ═══════════════════════════════════════════════════════════════════════

FLootResult FLootGenerator::CreateItemFromEntry(
	const FLootEntry& Entry,
	const FLootDropSettings& Settings,
	FRandomStream& RandStream,
	UObject* Outer) const
{
	FLootResult Result;
	
	if (!Entry.IsValid())
	{
		return Result;
	}
	
	// Roll values
	int32 Quantity = RollQuantity(Entry, Settings, RandStream);
	int32 ItemLevel = RollItemLevel(Entry, Settings, RandStream);
	EItemRarity Rarity = DetermineRarity(Entry, Settings, RandStream);
	bool bShouldCorrupt = ShouldCorruptItem(Entry, Settings, RandStream);
	int32 ItemSeed = RandStream.RandHelper(INT32_MAX);
	
	// Create item instance
	UItemInstance* Item = CreateItemInstance(Entry, ItemLevel, Rarity, bShouldCorrupt, ItemSeed, Outer);
	
	if (Item)
	{
		Result.Item = Item;
		Result.Quantity = Quantity;
		Result.bWasCorrupted = bShouldCorrupt;
		
		// Set quantity on stackable items
		if (Quantity > 1 && Item->IsStackable())
		{
			Item->SetQuantity(Quantity);
		}
	}
	
	return Result;
}

// ═══════════════════════════════════════════════════════════════════════
// UTILITY FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════

int32 FLootGenerator::RollQuantity(
	const FLootEntry& Entry,
	const FLootDropSettings& Settings,
	FRandomStream& RandStream) const
{
	// Base quantity roll
	int32 BaseQuantity = RandStream.RandRange(Entry.MinQuantity, Entry.MaxQuantity);
	
	// Apply multiplier
	float Multiplier = Settings.QuantityMultiplier;
	
	// Add magic find bonus
	Multiplier += Settings.PlayerMagicFindBonus * 0.01f;
	
	int32 FinalQuantity = FMath::RoundToInt(BaseQuantity * Multiplier);
	
	return FMath::Max(1, FinalQuantity);
}

int32 FLootGenerator::RollItemLevel(
	const FLootEntry& Entry,
	const FLootDropSettings& Settings,
	FRandomStream& RandStream) const
{
	int32 BaseLevel;
	
	if (Entry.bUseItemLevel)
	{
		// Use entry's level range
		BaseLevel = RandStream.RandRange(Entry.MinItemLevel, Entry.MaxItemLevel);
	}
	else
	{
		// Use source level with variance
		int32 MinLevel = FMath::Max(1, Settings.SourceLevel - Settings.LevelVariance);
		int32 MaxLevel = FMath::Min(100, Settings.SourceLevel + Settings.LevelVariance);
		BaseLevel = RandStream.RandRange(MinLevel, MaxLevel);
	}
	
	return FMath::Clamp(BaseLevel, 1, 100);
}

EItemRarity FLootGenerator::DetermineRarity(
	const FLootEntry& Entry,
	const FLootDropSettings& Settings,
	FRandomStream& RandStream) const
{
	// Use override if set
	if (Entry.OverrideRarity != EItemRarity::IR_None)
	{
		return Entry.OverrideRarity;
	}
	
	// Check minimum rarity requirement
	if (Settings.MinimumItemRarity != EItemRarity::IR_None)
	{
		return Settings.MinimumItemRarity;
	}
	
	// Roll for rarity upgrade
	float UpgradeChance = Settings.RarityBonusChance;
	
	// Add luck bonus
	UpgradeChance += Settings.PlayerLuckBonus * 0.005f;
	
	// Base rarity from source
	EItemRarity BaseRarity = EItemRarity::IR_GradeF;
	
	switch (Settings.SourceRarity)
	{
		case EDropRarity::DR_Common:
			BaseRarity = EItemRarity::IR_GradeF;
			break;
		case EDropRarity::DR_Uncommon:
			BaseRarity = EItemRarity::IR_GradeE;
			break;
		case EDropRarity::DR_Rare:
			BaseRarity = EItemRarity::IR_GradeD;
			break;
		case EDropRarity::DR_Epic:
			BaseRarity = EItemRarity::IR_GradeC;
			break;
		case EDropRarity::DR_Legendary:
			BaseRarity = EItemRarity::IR_GradeB;
			break;
		case EDropRarity::DR_Mythical:
			BaseRarity = EItemRarity::IR_GradeA;
			break;
	}
	
	// Chance to upgrade rarity
	if (UpgradeChance > 0.0f && RandStream.FRand() < UpgradeChance)
	{
		int32 RarityInt = static_cast<int32>(BaseRarity);
		int32 MaxRarity = static_cast<int32>(EItemRarity::IR_GradeS);
		
		if (RarityInt < MaxRarity)
		{
			BaseRarity = static_cast<EItemRarity>(RarityInt + 1);
		}
	}
	
	return BaseRarity;
}

bool FLootGenerator::ShouldCorruptItem(
	const FLootEntry& Entry,
	const FLootDropSettings& Settings,
	FRandomStream& RandStream) const
{
	// Already corrupted entry
	if (Entry.bIsCorrupted)
	{
		return true;
	}
	
	// Can't be corrupted
	if (!Entry.bCanBeCorrupted)
	{
		return false;
	}
	
	// Roll corruption chance
	return Settings.CorruptionChance > 0.0f && RandStream.FRand() < Settings.CorruptionChance;
}

// ═══════════════════════════════════════════════════════════════════════
// INTERNAL SELECTION METHODS
// ═══════════════════════════════════════════════════════════════════════

TArray<int32> FLootGenerator::SelectWeighted(
	const TArray<FLootEntry>& Entries,
	int32 NumToSelect,
	bool bAllowDuplicates,
	FRandomStream& RandStream) const
{
	TArray<int32> Selected;
	
	if (Entries.Num() == 0 || NumToSelect <= 0)
	{
		return Selected;
	}
	
	// Build weight array
	TArray<float> Weights;
	float TotalWeight = 0.0f;
	
	for (const FLootEntry& Entry : Entries)
	{
		float Weight = Entry.GetEffectiveWeight();
		Weights.Add(Weight);
		TotalWeight += Weight;
	}
	
	if (TotalWeight <= 0.0f)
	{
		return Selected;
	}
	
	// Select items
	TSet<int32> UsedIndices;
	
	for (int32 i = 0; i < NumToSelect; i++)
	{
		float Roll = RandStream.FRand() * TotalWeight;
		float CumulativeWeight = 0.0f;
		
		for (int32 j = 0; j < Entries.Num(); j++)
		{
			// Skip if duplicates not allowed and already selected
			if (!bAllowDuplicates && UsedIndices.Contains(j))
			{
				continue;
			}
			
			CumulativeWeight += Weights[j];
			
			if (Roll <= CumulativeWeight)
			{
				Selected.Add(j);
				UsedIndices.Add(j);
				
				// Update total weight if no duplicates
				if (!bAllowDuplicates)
				{
					TotalWeight -= Weights[j];
				}
				break;
			}
		}
		
		// Exit if we've used all entries
		if (!bAllowDuplicates && UsedIndices.Num() >= Entries.Num())
		{
			break;
		}
	}
	
	return Selected;
}

TArray<int32> FLootGenerator::SelectSequential(
	const TArray<FLootEntry>& Entries,
	const FLootDropSettings& Settings,
	FRandomStream& RandStream) const
{
	TArray<int32> Selected;
	
	for (int32 i = 0; i < Entries.Num(); i++)
	{
		const FLootEntry& Entry = Entries[i];
		
		// Roll against drop chance with multiplier
		float FinalChance = Entry.DropChance * Settings.DropChanceMultiplier;
		
		if (RandStream.FRand() <= FinalChance)
		{
			Selected.Add(i);
		}
	}
	
	return Selected;
}

TArray<int32> FLootGenerator::SelectGuaranteedOne(
	const TArray<FLootEntry>& Entries,
	FRandomStream& RandStream) const
{
	TArray<int32> Selected;
	
	if (Entries.Num() == 0)
	{
		return Selected;
	}
	
	// Select one guaranteed using weighted random
	TArray<int32> Guaranteed = SelectWeighted(Entries, 1, false, RandStream);
	Selected.Append(Guaranteed);
	
	// Then roll for additional drops
	for (int32 i = 0; i < Entries.Num(); i++)
	{
		// Skip the guaranteed one
		if (Selected.Contains(i))
		{
			continue;
		}
		
		if (RandStream.FRand() <= Entries[i].DropChance)
		{
			Selected.Add(i);
		}
	}
	
	return Selected;
}

TArray<int32> FLootGenerator::SelectAll(
	const TArray<FLootEntry>& Entries,
	const FLootDropSettings& Settings,
	FRandomStream& RandStream) const
{
	TArray<int32> Selected;
	
	for (int32 i = 0; i < Entries.Num(); i++)
	{
		float FinalChance = Entries[i].DropChance * Settings.DropChanceMultiplier;
		
		if (RandStream.FRand() <= FinalChance)
		{
			Selected.Add(i);
		}
	}
	
	return Selected;
}

TArray<FLootEntry> FLootGenerator::FilterEntries(
	const TArray<FLootEntry>& Entries,
	const FLootDropSettings& Settings) const
{
	TArray<FLootEntry> Filtered;
	
	for (const FLootEntry& Entry : Entries)
	{
		if (!Entry.IsValid())
		{
			continue;
		}
		
		// Filter corrupted entries
		if (Settings.bOnlyCorruptedDrops && !Entry.bIsCorrupted)
		{
			continue;
		}
		
		if (Settings.bExcludeCorruptedEntries && Entry.bIsCorrupted)
		{
			continue;
		}
		
		Filtered.Add(Entry);
	}
	
	return Filtered;
}

int32 FLootGenerator::CalculateDropCount(
	const FLootTable& Table,
	const FLootDropSettings& Settings,
	FRandomStream& RandStream) const
{
	// Use table limits if set
	int32 Min = Table.MinSelections > 0 ? Table.MinSelections : Settings.MinDrops;
	int32 Max = Table.MaxSelections > 0 ? Table.MaxSelections : Settings.MaxDrops;
	
	// Apply multiplier to max
	Max = FMath::RoundToInt(Max * (1.0f + Settings.PlayerMagicFindBonus * 0.01f));
	
	// Ensure min <= max
	Max = FMath::Max(Min, Max);
	
	return RandStream.RandRange(Min, Max);
}

UItemInstance* FLootGenerator::CreateItemInstance(
	const FLootEntry& Entry,
	int32 ItemLevel,
	EItemRarity Rarity,
	bool bCorrupted,
	int32 Seed,
	UObject* Outer) const
{
	// Create new item instance
	UItemInstance* Item = NewObject<UItemInstance>(Outer);
	
	if (!Item)
	{
		UE_LOG(LogLootGenerator, Error, TEXT("Failed to create ItemInstance"));
		return nullptr;
	}
	
	// Set seed before initialization
	Item->SetSeed(Seed);
	
	// Initialize from DataTable handle
	if (Entry.ItemRowHandle.DataTable)
	{
		Item->Initialize(
			Entry.ItemRowHandle,
			ItemLevel,
			Rarity,
			Entry.bGenerateAffixes
		);
	}
	else if (Entry.ItemClass)
	{
		// TODO: Handle class-based item creation if needed
		UE_LOG(LogLootGenerator, Warning, TEXT("Class-based item creation not yet implemented"));
	}
	
	// Apply corruption if needed
	if (bCorrupted)
	{
		// TODO: Apply corruption effect to item
		// This could add corrupted implicit, change rarity, etc.
		UE_LOG(LogLootGenerator, Verbose, TEXT("Applied corruption to item"));
	}
	
	return Item;
}