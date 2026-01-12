// Item/Generation/AffixGenerator.cpp

#include "Item/Generation/AffixGenerator.h"
#include "Engine/DataTable.h"

// ═══════════════════════════════════════════════════════════════════════
// MAIN GENERATION
// ═══════════════════════════════════════════════════════════════════════

FPHItemStats FAffixGenerator::GenerateAffixes(
	const FItemBase& BaseItem,
	int32 ItemLevel,
	EItemRarity Rarity,
	int32 Seed,
	float CorruptionChance,
	bool bForceOneCorrupted) const
{
	FPHItemStats Stats;
	
	// Copy implicits from base item
	Stats.Implicits = BaseItem.ImplicitMods;
	for (FPHAttributeData& Implicit : Stats.Implicits)
	{
		Implicit.RollValue();
		Implicit.GenerateUID();
	}
	
	// Grade SS (EX-Rank): Use unique affixes from base item
	if (Rarity == EItemRarity::IR_GradeSS || BaseItem.bIsUnique)
	{
		Stats.Prefixes = BaseItem.UniqueAffixes;
		for (FPHAttributeData& Affix : Stats.Prefixes)
		{
			Affix.RollValue();
			Affix.GenerateUID();
		}
		Stats.bAffixesGenerated = true;
		return Stats;
	}
	
	// Get affix counts based on rarity
	int32 MinPrefixes, MaxPrefixes, MinSuffixes, MaxSuffixes;
	GetAffixCountByRarity(Rarity, MinPrefixes, MaxPrefixes, MinSuffixes, MaxSuffixes);
	
	// Roll random counts
	FRandomStream RandStream(Seed);
	int32 NumPrefixes = RandStream.RandRange(MinPrefixes, MaxPrefixes);
	int32 NumSuffixes = RandStream.RandRange(MinSuffixes, MaxSuffixes);
	
	// Track if we've rolled a corrupted affix (for bForceOneCorrupted)
	bool bHasRolledCorrupted = false;
	
	// Generate prefixes
	Stats.Prefixes = RollAffixesWithCorruption(
		EAffixes::AF_Prefix,
		NumPrefixes,
		ItemLevel,
		BaseItem.ItemType,
		BaseItem.ItemSubType,
		CorruptionChance,
		bForceOneCorrupted && !bHasRolledCorrupted,
		bHasRolledCorrupted,
		RandStream
	);
	
	// Generate suffixes
	Stats.Suffixes = RollAffixesWithCorruption(
		EAffixes::AF_Suffix,
		NumSuffixes,
		ItemLevel,
		BaseItem.ItemType,
		BaseItem.ItemSubType,
		CorruptionChance,
		bForceOneCorrupted && !bHasRolledCorrupted,
		bHasRolledCorrupted,
		RandStream
	);
	
	Stats.bAffixesGenerated = true;
	
	return Stats;
}

FPHItemStats FAffixGenerator::GenerateAffixesCustom(
	const FItemBase& BaseItem,
	int32 ItemLevel,
	int32 NumPrefixes,
	int32 NumSuffixes,
	int32 Seed) const
{
	// Call main function with no corruption
	return GenerateAffixes(BaseItem, ItemLevel, EItemRarity::IR_GradeD, Seed, 0.0f, false);
}

// ═══════════════════════════════════════════════════════════════════════
// AFFIX ROLLING WITH CORRUPTION
// ═══════════════════════════════════════════════════════════════════════

TArray<FPHAttributeData> FAffixGenerator::RollAffixesWithCorruption(
	EAffixes AffixType,
	int32 Count,
	int32 ItemLevel,
	EItemType ItemType,
	EItemSubType ItemSubType,
	float CorruptionChance,
	bool bMustRollOneCorrupted,
	bool& bOutHasRolledCorrupted,
	FRandomStream& RandStream) const
{
	TArray<FPHAttributeData> RolledAffixes;
	TArray<FName> ExcludedAffixes;
	
	if (Count <= 0)
	{
		return RolledAffixes;
	}
	
	// Determine which slot should be forced corrupted (if required)
	int32 ForcedCorruptedSlot = -1;
	if (bMustRollOneCorrupted && !bOutHasRolledCorrupted)
	{
		ForcedCorruptedSlot = RandStream.RandRange(0, Count - 1);
	}
	
	for (int32 i = 0; i < Count; ++i)
	{
		// Determine if this affix should be corrupted
		bool bShouldBeCorrupted = false;
		
		if (i == ForcedCorruptedSlot)
		{
			bShouldBeCorrupted = true;
		}
		else if (CorruptionChance > 0.0f)
		{
			bShouldBeCorrupted = RandStream.FRand() < CorruptionChance;
		}
		
		// Build affix pool (positive or negative based on corruption)
		TArray<FPHAttributeData*> AvailableAffixes = BuildAffixPoolByCorruption(
			AffixType,
			ItemType,
			ItemSubType,
			ItemLevel,
			bShouldBeCorrupted,  // Filter by positive/negative
			ExcludedAffixes
		);
		
		if (AvailableAffixes.Num() == 0)
		{
			// If no corrupted affixes available, try positive ones instead
			if (bShouldBeCorrupted)
			{
				AvailableAffixes = BuildAffixPoolByCorruption(
					AffixType, ItemType, ItemSubType, ItemLevel,
					false, ExcludedAffixes
				);
			}
			
			if (AvailableAffixes.Num() == 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("AffixGenerator: No available affixes for type %d at level %d"),
					static_cast<int32>(AffixType), ItemLevel);
				continue;
			}
		}
		
		// Select random affix from pool
		const FPHAttributeData* SelectedAffix = SelectRandomAffix(AvailableAffixes, RandStream);
		if (!SelectedAffix)
		{
			continue;
		}
		
		// Create rolled instance with random value
		FPHAttributeData RolledAffix = CreateRolledAffix(*SelectedAffix, RandStream);
		RolledAffixes.Add(RolledAffix);
		
		// Track if we've rolled a corrupted affix
		if (RolledAffix.IsCorruptedAffix())
		{
			bOutHasRolledCorrupted = true;
		}
		
		// Exclude this affix from future rolls (prevent duplicates)
		ExcludedAffixes.Add(SelectedAffix->AttributeName);
	}
	
	return RolledAffixes;
}

TArray<FPHAttributeData*> FAffixGenerator::BuildAffixPoolByCorruption(
	EAffixes AffixType,
	EItemType ItemType,
	EItemSubType ItemSubType,
	int32 ItemLevel,
	bool bCorruptedOnly,
	const TArray<FName>& ExcludeAffixes) const
{
	TArray<FPHAttributeData*> Pool;
	
	UDataTable* AffixTable = GetAffixDataTable();
	if (!AffixTable)
	{
		return Pool;
	}
	
	TArray<FPHAttributeData*> AllAffixes;
	AffixTable->GetAllRows<FPHAttributeData>("BuildAffixPoolByCorruption", AllAffixes);
	
	for (FPHAttributeData* Affix : AllAffixes)
	{
		if (!Affix)
		{
			continue;
		}
		
		// Filter by type (Prefix/Suffix)
		if (Affix->AffixType != AffixType)
		{
			continue;
		}
		
		// Exclude already rolled affixes (prevent duplicates)
		if (ExcludeAffixes.Contains(Affix->AttributeName))
		{
			continue;
		}
		
		// Filter by item type
		if (!Affix->IsAllowedOnItemType(ItemType))
		{
			continue;
		}
		
		// Filter by item subtype
		if (!Affix->IsAllowedOnSubType(ItemSubType))
		{
			continue;
		}
		
		// Filter by item level
		if (!Affix->IsValidForItemLevel(ItemLevel))
		{
			continue;
		}
		
		// ═══════════════════════════════════════════════
		// CORRUPTION FILTER
		// ═══════════════════════════════════════════════
		bool bIsNegative = Affix->IsCorruptedAffix();
		
		if (bCorruptedOnly && !bIsNegative)
		{
			continue; // We want corrupted, this is positive
		}
		
		if (!bCorruptedOnly && bIsNegative)
		{
			continue; // We want positive, this is corrupted
		}
		
		Pool.Add(Affix);
	}
	
	return Pool;
}

const FPHAttributeData* FAffixGenerator::SelectRandomAffix(
	const TArray<FPHAttributeData*>& AvailableAffixes,
	FRandomStream& RandStream) const
{
	if (AvailableAffixes.Num() == 0)
	{
		return nullptr;
	}
	
	int32 TotalWeight = 0;
	for (const FPHAttributeData* Affix : AvailableAffixes)
	{
		TotalWeight += Affix->GetWeight();
	}
	
	if (TotalWeight <= 0)
	{
		return AvailableAffixes[RandStream.RandRange(0, AvailableAffixes.Num() - 1)];
	}
	
	int32 RandomValue = RandStream.RandRange(0, TotalWeight - 1);
	int32 CurrentWeight = 0;
	
	for (const FPHAttributeData* Affix : AvailableAffixes)
	{
		CurrentWeight += Affix->GetWeight();
		if (RandomValue < CurrentWeight)
		{
			return Affix;
		}
	}
	
	return AvailableAffixes.Last();
}

FPHAttributeData FAffixGenerator::CreateRolledAffix(
	const FPHAttributeData& TemplateAffix,
	FRandomStream& RandStream) const
{
	FPHAttributeData RolledAffix = TemplateAffix;
	RolledAffix.RollValue();
	RolledAffix.GenerateUID();
	return RolledAffix;
}

// ═══════════════════════════════════════════════════════════════════════
// AFFIX COUNT HELPERS
// ═══════════════════════════════════════════════════════════════════════

void FAffixGenerator::GetAffixCountByRarity(
	EItemRarity Rarity,
	int32& OutMinPrefixes,
	int32& OutMaxPrefixes,
	int32& OutMinSuffixes,
	int32& OutMaxSuffixes)
{
	switch (Rarity)
	{
		case EItemRarity::IR_GradeF:
			OutMinPrefixes = 0;
			OutMaxPrefixes = 0;
			OutMinSuffixes = 0;
			OutMaxSuffixes = 0;
			break;
			
		case EItemRarity::IR_GradeE:
			OutMinPrefixes = 0;
			OutMaxPrefixes = 1;
			OutMinSuffixes = 0;
			OutMaxSuffixes = 1;
			break;
			
		case EItemRarity::IR_GradeD:
			OutMinPrefixes = 1;
			OutMaxPrefixes = 1;
			OutMinSuffixes = 0;
			OutMaxSuffixes = 1;
			break;
			
		case EItemRarity::IR_GradeC:
			OutMinPrefixes = 1;
			OutMaxPrefixes = 2;
			OutMinSuffixes = 1;
			OutMaxSuffixes = 1;
			break;
			
		case EItemRarity::IR_GradeB:
			OutMinPrefixes = 1;
			OutMaxPrefixes = 2;
			OutMinSuffixes = 1;
			OutMaxSuffixes = 2;
			break;
			
		case EItemRarity::IR_GradeA:
			OutMinPrefixes = 2;
			OutMaxPrefixes = 3;
			OutMinSuffixes = 2;
			OutMaxSuffixes = 2;
			break;
			
		case EItemRarity::IR_GradeS:
			OutMinPrefixes = 2;
			OutMaxPrefixes = 3;
			OutMinSuffixes = 2;
			OutMaxSuffixes = 3;
			break;
			
		case EItemRarity::IR_GradeSS:
		default:
			OutMinPrefixes = 3;
			OutMaxPrefixes = 3;
			OutMinSuffixes = 3;
			OutMaxSuffixes = 3;
			break;
	}
}

// ═══════════════════════════════════════════════════════════════════════
// DATATABLE ACCESS
// ═══════════════════════════════════════════════════════════════════════

UDataTable* FAffixGenerator::GetAffixDataTable() const
{
	if (CachedAffixTable && IsValid(CachedAffixTable))
	{
		return CachedAffixTable;
	}
	
	if (bLoadAttempted && !CachedAffixTable)
	{
		return nullptr;
	}
	
	bLoadAttempted = true;
	CachedAffixTable = Cast<UDataTable>(AffixDataTablePath.TryLoad());
	
	if (!CachedAffixTable)
	{
		UE_LOG(LogTemp, Error, TEXT("AffixGenerator: Failed to load affix DataTable from '%s'"),
			*AffixDataTablePath.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("AffixGenerator: Loaded affix DataTable with %d rows"),
			CachedAffixTable->GetRowNames().Num());
	}
	
	return CachedAffixTable;
}
