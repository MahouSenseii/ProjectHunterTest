// Item/Generation/AffixGenerator.cpp

#include "Item/Generation/AffixGenerator.h"
#include "Engine/DataTable.h"

// ═══════════════════════════════════════════════
// MAIN GENERATION
// ═══════════════════════════════════════════════

FPHItemStats FAffixGenerator::GenerateAffixes(
	const FItemBase& BaseItem,
	int32 ItemLevel,
	EItemRarity Rarity,
	int32 Seed) const
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
	if (Rarity == EItemRarity::IR_GradeSS || BaseItem.bIsItemUnique)
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
	
	// Generate prefixes
	Stats.Prefixes = RollAffixes(
		EAffixes::AF_Prefix,
		NumPrefixes,
		ItemLevel,
		BaseItem.ItemType,
		BaseItem.ItemSubType,
		RandStream
	);
	
	// Generate suffixes
	Stats.Suffixes = RollAffixes(
		EAffixes::AF_Suffix,
		NumSuffixes,
		ItemLevel,
		BaseItem.ItemType,
		BaseItem.ItemSubType,
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
	FPHItemStats Stats;
	
	// Copy implicits
	Stats.Implicits = BaseItem.ImplicitMods;
	for (FPHAttributeData& Implicit : Stats.Implicits)
	{
		Implicit.RollValue();
		Implicit.GenerateUID();
	}
	
	// Generate affixes with custom counts
	FRandomStream RandStream(Seed);
	
	Stats.Prefixes = RollAffixes(
		EAffixes::AF_Prefix,
		NumPrefixes,
		ItemLevel,
		BaseItem.ItemType,
		BaseItem.ItemSubType,
		RandStream
	);
	
	Stats.Suffixes = RollAffixes(
		EAffixes::AF_Suffix,
		NumSuffixes,
		ItemLevel,
		BaseItem.ItemType,
		BaseItem.ItemSubType,
		RandStream
	);
	
	Stats.bAffixesGenerated = true;
	
	return Stats;
}

// ═══════════════════════════════════════════════
// AFFIX ROLLING
// ═══════════════════════════════════════════════

TArray<FPHAttributeData> FAffixGenerator::RollAffixes(
	EAffixes AffixType,
	int32 Count,
	int32 ItemLevel,
	EItemType ItemType,
	EItemSubType ItemSubType,
	FRandomStream& RandStream) const
{
	TArray<FPHAttributeData> RolledAffixes;
	TArray<FName> ExcludedAffixes;  // Track already rolled affixes
	
	if (Count <= 0)
	{
		return RolledAffixes;
	}
	
	for (int32 i = 0; i < Count; ++i)
	{
		// Build available affix pool
		TArray<FPHAttributeData*> AvailableAffixes = BuildAffixPool(
			AffixType,
			ItemType,
			ItemSubType,
			ExcludedAffixes
		);
		
		if (AvailableAffixes.Num() == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("AffixGenerator: No available affixes for type %d, stopping at %d/%d"),
				static_cast<int32>(AffixType), i, Count);
			break;
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
		
		// Exclude this affix from future rolls (prevent duplicates)
		ExcludedAffixes.Add(SelectedAffix->AttributeName);
	}
	
	return RolledAffixes;
}

const FPHAttributeData* FAffixGenerator::SelectRandomAffix(
	const TArray<FPHAttributeData*>& AvailableAffixes,
	FRandomStream& RandStream) const
{
	if (AvailableAffixes.Num() == 0)
	{
		return nullptr;
	}
	
	// Weighted selection based on RankPoints
	// Higher tier affixes (RP_10) = rarer = lower weight
	// Lower tier affixes (RP_1) = common = higher weight
	int32 TotalWeight = 0;
	for (const FPHAttributeData* Affix : AvailableAffixes)
	{
		TotalWeight += Affix->GetWeight();
	}
	
	if (TotalWeight <= 0)
	{
		return nullptr;
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

TArray<FPHAttributeData*> FAffixGenerator::BuildAffixPool(
	EAffixes AffixType,
	EItemType ItemType,
	EItemSubType ItemSubType,
	const TArray<FName>& ExcludeAffixes) const
{
	TArray<FPHAttributeData*> Pool;
	
	UDataTable* AffixTable = GetAffixDataTable();
	if (!AffixTable)
	{
		UE_LOG(LogTemp, Error, TEXT("AffixGenerator: Affix DataTable not found at path: %s"),
			*AffixDataTablePath.ToString());
		return Pool;
	}
	
	// Get all rows from DT_Affixes
	TArray<FPHAttributeData*> AllAffixes;
	AffixTable->GetAllRows<FPHAttributeData>("BuildAffixPool", AllAffixes);
	
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
		
		// Filter by item type (uses helper function)
		if (!Affix->IsAllowedOnItemType(ItemType))
		{
			continue;
		}
		
		// Filter by item subtype (uses helper function)
		if (!Affix->IsAllowedOnSubType(ItemSubType))
		{
			continue;
		}
		
		// Add to pool
		Pool.Add(Affix);
	}
	
	return Pool;
}

FPHAttributeData FAffixGenerator::CreateRolledAffix(
	const FPHAttributeData& TemplateAffix,
	FRandomStream& RandStream) const
{
	// Copy template affix from DataTable
	FPHAttributeData RolledAffix = TemplateAffix;
	
	// Roll random value within min-max range
	RolledAffix.RollValue();
	
	// Generate unique instance ID
	RolledAffix.GenerateUID();
	
	return RolledAffix;
}

// ═══════════════════════════════════════════════
// AFFIX COUNT HELPERS
// ═══════════════════════════════════════════════

void FAffixGenerator::GetAffixCountByRarity(
	EItemRarity Rarity,
	int32& OutMinPrefixes,
	int32& OutMaxPrefixes,
	int32& OutMinSuffixes,
	int32& OutMaxSuffixes)
{
	// Hunter Manga / ORV style affix counts (F-SS Grade System)
	switch (Rarity)
	{
		case EItemRarity::IR_GradeF:  // Grade F (Common) - No affixes
			OutMinPrefixes = 0;
			OutMaxPrefixes = 0;
			OutMinSuffixes = 0;
			OutMaxSuffixes = 0;
			break;
			
		case EItemRarity::IR_GradeE:  // Grade E (Uncommon) - 1-2 affixes
			OutMinPrefixes = 0;
			OutMaxPrefixes = 1;
			OutMinSuffixes = 0;
			OutMaxSuffixes = 1;
			break;
			
		case EItemRarity::IR_GradeD:  // Grade D (Rare) - 2-3 affixes
			OutMinPrefixes = 1;
			OutMaxPrefixes = 2;
			OutMinSuffixes = 1;
			OutMaxSuffixes = 2;
			break;
			
		case EItemRarity::IR_GradeC:  // Grade C (Elite) - 3-4 affixes
			OutMinPrefixes = 1;
			OutMaxPrefixes = 2;
			OutMinSuffixes = 2;
			OutMaxSuffixes = 3;
			break;
			
		case EItemRarity::IR_GradeB:  // Grade B (Named) - 4-5 affixes
			OutMinPrefixes = 2;
			OutMaxPrefixes = 3;
			OutMinSuffixes = 2;
			OutMaxSuffixes = 3;
			break;
			
		case EItemRarity::IR_GradeA:  // Grade A (Legendary) - 5-6 affixes
			OutMinPrefixes = 2;
			OutMaxPrefixes = 3;
			OutMinSuffixes = 3;
			OutMaxSuffixes = 3;
			break;
			
		case EItemRarity::IR_GradeS:  // Grade S (Mythic) - 6 affixes (max)
			OutMinPrefixes = 3;
			OutMaxPrefixes = 3;
			OutMinSuffixes = 3;
			OutMaxSuffixes = 3;
			break;
			
		case EItemRarity::IR_GradeSS:  // Grade SS (EX-Rank) - Fixed unique affixes
			OutMinPrefixes = 0;
			OutMaxPrefixes = 0;
			OutMinSuffixes = 0;
			OutMaxSuffixes = 0;
			break;
			
		default:
			OutMinPrefixes = 0;
			OutMaxPrefixes = 0;
			OutMinSuffixes = 0;
			OutMaxSuffixes = 0;
			break;
	}
}

// ═══════════════════════════════════════════════
// DATATABLE ACCESS
// ═══════════════════════════════════════════════

UDataTable* FAffixGenerator::GetAffixDataTable() const
{
	if (CachedAffixTable)
	{
		return CachedAffixTable;
	}
	
	// Load affix DataTable from configured path
	CachedAffixTable = Cast<UDataTable>(AffixDataTablePath.TryLoad());
	
	if (!CachedAffixTable)
	{
		UE_LOG(LogTemp, Error, TEXT("AffixGenerator: Failed to load affix DataTable from path: %s"),
			*AffixDataTablePath.ToString());
		UE_LOG(LogTemp, Error, TEXT("AffixGenerator: Make sure DT_Affixes exists and uses FPHAttributeData as row type"));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("AffixGenerator: Loaded affix DataTable: %s"),
			*CachedAffixTable->GetName());
	}
	
	return CachedAffixTable;
}