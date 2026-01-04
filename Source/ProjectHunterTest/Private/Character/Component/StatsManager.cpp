// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/Component/StatsManager.h"
#include "AbilitySystem/HunterAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "Item/ItemInstance.h"
#include "Item/Library/ItemStructs.h"
#include "Item/Library/AffixEnums.h"

class UBaseStatsData;
class IAbilitySystemInterface;

UStatsManager::UStatsManager()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false); // Stats are replicated through AttributeSet
}

void UStatsManager::BeginPlay()
{
	Super::BeginPlay();

	// Cache references on begin play
	CachedASC = GetAbilitySystemComponent();
	CachedAttributeSet = GetAttributeSet();
	
	if (!CachedASC)
	{
		UE_LOG(LogTemp, Warning, TEXT("StatsManager: No AbilitySystemComponent found on %s"), *GetOwner()->GetName());
	}
	
	if (!CachedAttributeSet)
	{
		UE_LOG(LogTemp, Warning, TEXT("StatsManager: No HunterAttributeSet found on %s"), *GetOwner()->GetName());
	}

	if (StatsData)
	{
		InitializeFromDataAsset(StatsData);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("StatsManager: No StatsData found on %s"), *GetOwner()->GetName());
	}
}

/* ═══════════════════════════════════════════════════════════════════════ */
/* EQUIPMENT INTEGRATION */
/* ═══════════════════════════════════════════════════════════════════════ */

void UStatsManager::ApplyEquipmentStats(UItemInstance* Item)
{
	if (!Item)
	{
		UE_LOG(LogTemp, Warning, TEXT("StatsManager::ApplyEquipmentStats: Invalid item"));
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		UE_LOG(LogTemp, Error, TEXT("StatsManager::ApplyEquipmentStats: No AbilitySystemComponent"));
		return;
	}

	if (!GetOwner()->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("StatsManager::ApplyEquipmentStats: Must be called on server"));
		return;
	}

	// Check if already applied
	if (ActiveEquipmentEffects.Contains(Item->UniqueID))
	{
		UE_LOG(LogTemp, Warning, TEXT("StatsManager: Equipment stats already applied for %s"), *Item->GetName());
		return;
	}

	// Get all stats from item (Prefixes + Suffixes + Implicits + Crafted)
	TArray<FPHAttributeData> AllStats = Item->Stats.GetAllStats();
	
	if (AllStats.Num() == 0)
	{
		UE_LOG(LogTemp, Verbose, TEXT("StatsManager: Item %s has no stats to apply"), *Item->GetName());
		return;
	}

	// Create gameplay effect for this item
	FGameplayEffectSpecHandle EffectSpec = CreateEquipmentEffect(Item, AllStats);
	if (!EffectSpec.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("StatsManager: Failed to create equipment effect for %s"), *Item->GetName());
		return;
	}

	// Apply effect and store handle
	FActiveGameplayEffectHandle EffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data.Get());
	
	if (EffectHandle.IsValid())
	{
		ActiveEquipmentEffects.Add(Item->UniqueID, EffectHandle);
		
		UE_LOG(LogTemp, Log, TEXT("StatsManager: Applied %d stats from %s (GUID: %s)"), 
			AllStats.Num(), *Item->GetName(), *Item->UniqueID.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("StatsManager: Failed to apply equipment effect for %s"), *Item->GetName());
	}
}

void UStatsManager::RemoveEquipmentStats(UItemInstance* Item)
{
	if (!Item)
	{
		UE_LOG(LogTemp, Warning, TEXT("StatsManager::RemoveEquipmentStats: Invalid item"));
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		UE_LOG(LogTemp, Error, TEXT("StatsManager::RemoveEquipmentStats: No AbilitySystemComponent"));
		return;
	}

	if (!GetOwner()->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("StatsManager::RemoveEquipmentStats: Must be called on server"));
		return;
	}

	// Find effect handle
	FActiveGameplayEffectHandle* EffectHandle = ActiveEquipmentEffects.Find(Item->UniqueID);
	if (!EffectHandle || !EffectHandle->IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("StatsManager: No active equipment effect found for %s"), *Item->GetName());
		return;
	}

	// Remove effect
	ASC->RemoveActiveGameplayEffect(*EffectHandle);
	ActiveEquipmentEffects.Remove(Item->UniqueID);

	UE_LOG(LogTemp, Log, TEXT("StatsManager: Removed equipment stats for %s (GUID: %s)"), 
		*Item->GetName(), *Item->UniqueID.ToString());
}

void UStatsManager::RefreshEquipmentStats()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC || !GetOwner()->HasAuthority())
	{
		return;
	}

	int32 NumEffects = ActiveEquipmentEffects.Num();
	
	// Remove all equipment effects
	for (const TPair<FGuid, FActiveGameplayEffectHandle>& Pair : ActiveEquipmentEffects)
	{
		if (Pair.Value.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(Pair.Value);
		}
	}
	
	ActiveEquipmentEffects.Empty();

	UE_LOG(LogTemp, Log, TEXT("StatsManager: Refreshed equipment stats (removed %d effects)"), NumEffects);
}

bool UStatsManager::HasEquipmentStatsApplied(UItemInstance* Item) const
{
	if (!Item)
	{
		return false;
	}

	return ActiveEquipmentEffects.Contains(Item->UniqueID);
}

FGameplayEffectSpecHandle UStatsManager::CreateEquipmentEffect(UItemInstance* Item, const TArray<FPHAttributeData>& Stats)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC || !Item)
	{
		return FGameplayEffectSpecHandle();
	}

	// Create a dynamic gameplay effect
	UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage(), FName(TEXT("EquipmentEffect")));
	Effect->DurationPolicy = EGameplayEffectDurationType::Infinite; // Lasts until removed
	Effect->StackingType = EGameplayEffectStackingType::None;

	// Add modifiers for each stat on the item (BEFORE creating spec)
	int32 ModifiersAdded = 0;
	for (const FPHAttributeData& Stat : Stats)
	{
		// Skip unidentified affixes
		if (!Stat.bIsIdentified)
		{
			continue;
		}

		// Get attribute (prefer ModifiedAttribute, fall back to AttributeName)
		FGameplayAttribute Attribute = Stat.ModifiedAttribute;
		if (!Attribute.IsValid() && Stat.AttributeName != NAME_None)
		{
			Attribute = UHunterAttributeSet::FindAttributeByName(Stat.AttributeName);
		}

		if (!Attribute.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("StatsManager: Invalid attribute for stat (AttributeName: %s)"), 
				*Stat.AttributeName.ToString());
			continue;
		}

		// Apply stat based on ModifyType
		if (ApplyStatModifier(Effect, Stat, Attribute))
		{
			ModifiersAdded++;
		}
	}

	if (ModifiersAdded == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("StatsManager: No valid modifiers added for %s"), *Item->GetName());
	}

	// Create effect spec AFTER adding modifiers
	FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
	EffectContext.AddSourceObject(Item); // Track which item created this effect
	
	// Create spec from effect instance (not class)
	FGameplayEffectSpec* EffectSpec = new FGameplayEffectSpec(Effect, EffectContext, 1.0f);
	return FGameplayEffectSpecHandle(EffectSpec);
}

bool UStatsManager::ApplyStatModifier(UGameplayEffect* Effect, const FPHAttributeData& Stat, const FGameplayAttribute& Attribute)
{
	if (!Effect || !Attribute.IsValid() || Stat.RolledStatValue == 0.0f)
	{
		return false;
	}

	// Determine modifier operation based on ModifyType
	EGameplayModOp::Type ModOp;
	float FinalValue = Stat.RolledStatValue;

	switch (Stat.ModifyType)
	{
	case EModifyType::MT_Add:
		// Flat addition (e.g., +50 Strength)
		ModOp = EGameplayModOp::Additive;
		break;

	case EModifyType::MT_Multiply:
	case EModifyType::MT_Increased:
		
		// Percentage (e.g., +75% increased damage)
		// GAS handles this as multiplicative
		ModOp = EGameplayModOp::Multiplicitive;
		// Convert to multiplier (75% = 1.75)
		FinalValue = 1.0f + (Stat.RolledStatValue / 100.0f);
		break;

	case EModifyType::MT_More:
		// More multiplier (e.g., 50% more damage)
		ModOp = EGameplayModOp::Multiplicitive;
		FinalValue = 1.0f + (Stat.RolledStatValue / 100.0f);
		break;

	case EModifyType::MT_Reduced:
	case EModifyType::MT_Less:
		// Negative multiplier (e.g., 25% reduced)
		ModOp = EGameplayModOp::Multiplicitive;
		FinalValue = 1.0f - (Stat.RolledStatValue / 100.0f);
		break;

	case EModifyType::MT_Override:
		// Override value completely
		ModOp = EGameplayModOp::Override;
		break;

	default:
		// Unsupported modifier type for equipment stats
		UE_LOG(LogTemp, Warning, TEXT("StatsManager: Unsupported ModifyType %d for attribute %s"), 
			static_cast<int32>(Stat.ModifyType), *Attribute.GetName());
		return false;
	}

	// Create modifier
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = Attribute;
	Modifier.ModifierOp = ModOp;
	Modifier.ModifierMagnitude = FScalableFloat(FinalValue);

	// Add to effect
	Effect->Modifiers.Add(Modifier);

	UE_LOG(LogTemp, VeryVerbose, TEXT("StatsManager: Added modifier: %s (%s) = %.2f [Op: %d]"), 
		*Attribute.GetName(), *Stat.AttributeName.ToString(), FinalValue, static_cast<int32>(ModOp));

	return true;
}

/* ═══════════════════════════════════════════════════════════════════════ */
/* INTERNAL HELPERS */
/* ═══════════════════════════════════════════════════════════════════════ */

UHunterAttributeSet* UStatsManager::GetAttributeSet() const
{
	if (CachedAttributeSet)
	{
		return CachedAttributeSet;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (ASC)
	{
		return const_cast<UHunterAttributeSet*>(ASC->GetSet<UHunterAttributeSet>());
	}

	return nullptr;
}

UAbilitySystemComponent* UStatsManager::GetAbilitySystemComponent() const
{
	if (CachedASC)
	{
		return CachedASC;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	// Try to get ASC from owner (should implement IAbilitySystemInterface)
	if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Owner))
	{
		return ASI->GetAbilitySystemComponent();
	}

	return nullptr;
}

/* ═══════════════════════════════════════════════════════════════════════ */
/* PRIMARY ATTRIBUTES (7) */
/* ═══════════════════════════════════════════════════════════════════════ */

float UStatsManager::GetStrength() const
{
	UHunterAttributeSet* AttrSet = GetAttributeSet();
	return AttrSet ? AttrSet->GetStrength() : 0.0f;
}

float UStatsManager::GetIntelligence() const
{
	UHunterAttributeSet* AttrSet = GetAttributeSet();
	return AttrSet ? AttrSet->GetIntelligence() : 0.0f;
}

float UStatsManager::GetDexterity() const
{
	UHunterAttributeSet* AttrSet = GetAttributeSet();
	return AttrSet ? AttrSet->GetDexterity() : 0.0f;
}

float UStatsManager::GetEndurance() const
{
	UHunterAttributeSet* AttrSet = GetAttributeSet();
	return AttrSet ? AttrSet->GetEndurance() : 0.0f;
}

float UStatsManager::GetAffliction() const
{
	UHunterAttributeSet* AttrSet = GetAttributeSet();
	return AttrSet ? AttrSet->GetAffliction() : 0.0f;
}

float UStatsManager::GetLuck() const
{
	UHunterAttributeSet* AttrSet = GetAttributeSet();
	return AttrSet ? AttrSet->GetLuck() : 0.0f;
}

float UStatsManager::GetCovenant() const
{
	UHunterAttributeSet* AttrSet = GetAttributeSet();
	return AttrSet ? AttrSet->GetCovenant() : 0.0f;
}

float UStatsManager::GetPrimaryAttribute(FName AttributeName) const
{
	if (AttributeName == "Strength") return GetStrength();
	if (AttributeName == "Intelligence") return GetIntelligence();
	if (AttributeName == "Dexterity") return GetDexterity();
	if (AttributeName == "Endurance") return GetEndurance();
	if (AttributeName == "Affliction") return GetAffliction();
	if (AttributeName == "Luck") return GetLuck();
	if (AttributeName == "Covenant") return GetCovenant();
	
	return 0.0f;
}

TMap<FName, float> UStatsManager::GetAllPrimaryAttributes() const
{
	TMap<FName, float> Attributes;

	Attributes.Add("Strength", GetStrength());
	Attributes.Add("Intelligence", GetIntelligence());
	Attributes.Add("Dexterity", GetDexterity());
	Attributes.Add("Endurance", GetEndurance());
	Attributes.Add("Affliction", GetAffliction());
	Attributes.Add("Luck", GetLuck());
	Attributes.Add("Covenant", GetCovenant());

	return Attributes;
}

/* ═══════════════════════════════════════════════════════════════════════ */
/* VITAL ATTRIBUTES (Most Used) */
/* ═══════════════════════════════════════════════════════════════════════ */

float UStatsManager::GetHealth() const
{
	UHunterAttributeSet* AttrSet = GetAttributeSet();
	return AttrSet ? AttrSet->GetHealth() : 0.0f;
}

float UStatsManager::GetMaxHealth() const
{
	UHunterAttributeSet* AttrSet = GetAttributeSet();
	return AttrSet ? AttrSet->GetMaxHealth() : 1.0f;
}

float UStatsManager::GetHealthPercent() const
{
	float MaxHP = GetMaxHealth();
	if (MaxHP > 0.0f)
	{
		return GetHealth() / MaxHP;
	}
	return 0.0f;
}

float UStatsManager::GetMana() const
{
	UHunterAttributeSet* AttrSet = GetAttributeSet();
	return AttrSet ? AttrSet->GetMana() : 0.0f;
}

float UStatsManager::GetMaxMana() const
{
	UHunterAttributeSet* AttrSet = GetAttributeSet();
	return AttrSet ? AttrSet->GetMaxMana() : 1.0f;
}

float UStatsManager::GetManaPercent() const
{
	float MaxMP = GetMaxMana();
	if (MaxMP > 0.0f)
	{
		return GetMana() / MaxMP;
	}
	return 0.0f;
}

float UStatsManager::GetStamina() const
{
	UHunterAttributeSet* AttrSet = GetAttributeSet();
	return AttrSet ? AttrSet->GetStamina() : 0.0f;
}

float UStatsManager::GetMaxStamina() const
{
	UHunterAttributeSet* AttrSet = GetAttributeSet();
	return AttrSet ? AttrSet->GetMaxStamina() : 1.0f;
}

float UStatsManager::GetStaminaPercent() const
{
	float MaxStam = GetMaxStamina();
	if (MaxStam > 0.0f)
	{
		return GetStamina() / MaxStam;
	}
	return 0.0f;
}

float UStatsManager::GetArcaneShield() const
{
	UHunterAttributeSet* AttrSet = GetAttributeSet();
	return AttrSet ? AttrSet->GetArcaneShield() : 0.0f;
}

float UStatsManager::GetMaxArcaneShield() const
{
	UHunterAttributeSet* AttrSet = GetAttributeSet();
	return AttrSet ? AttrSet->GetMaxArcaneShield() : 0.0f;
}

/* ═══════════════════════════════════════════════════════════════════════ */
/* COMBAT STATS                                                            */
/* ═══════════════════════════════════════════════════════════════════════ */

void UStatsManager::GetPhysicalDamageRange(float& OutMin, float& OutMax) const
{
	UHunterAttributeSet* AttrSet = GetAttributeSet();
	if (AttrSet)
	{
		OutMin = AttrSet->GetMinPhysicalDamage();
		OutMax = AttrSet->GetMaxPhysicalDamage();
	}
	else
	{
		OutMin = 0.0f;
		OutMax = 0.0f;
	}
}

void UStatsManager::GetElementalDamageRange(float& OutFireMin, float& OutFireMax,
                                             float& OutIceMin, float& OutIceMax,
                                             float& OutLightningMin, float& OutLightningMax) const
{
	UHunterAttributeSet* AttrSet = GetAttributeSet();
	if (AttrSet)
	{
		OutFireMin = AttrSet->GetMinFireDamage();
		OutFireMax = AttrSet->GetMaxFireDamage();
		OutIceMin = AttrSet->GetMinIceDamage();
		OutIceMax = AttrSet->GetMaxIceDamage();
		OutLightningMin = AttrSet->GetMinLightningDamage();
		OutLightningMax = AttrSet->GetMaxLightningDamage();
	}
	else
	{
		OutFireMin = OutFireMax = 0.0f;
		OutIceMin = OutIceMax = 0.0f;
		OutLightningMin = OutLightningMax = 0.0f;
	}
}

float UStatsManager::GetCriticalStrikeChance() const
{
	UHunterAttributeSet* AttrSet = GetAttributeSet();
	return AttrSet ? AttrSet->GetCritChance() : 0.0f;
}

float UStatsManager::GetCriticalStrikeMultiplier() const
{
	UHunterAttributeSet* AttrSet = GetAttributeSet();
	return AttrSet ? AttrSet->GetCritMultiplier() : 150.0f;
}

float UStatsManager::GetAttackSpeed() const
{
	UHunterAttributeSet* AttrSet = GetAttributeSet();
	return AttrSet ? AttrSet->GetAttackSpeed() : 100.0f;
}

float UStatsManager::GetCastSpeed() const
{
	UHunterAttributeSet* AttrSet = GetAttributeSet();
	return AttrSet ? AttrSet->GetCastSpeed() : 100.0f;
}

/* ═══════════════════════════════════════════════════════════════════════ */
/* DEFENSE STATS                                                           */
/* ═══════════════════════════════════════════════════════════════════════ */

float UStatsManager::GetArmor() const
{
	UHunterAttributeSet* AttrSet = GetAttributeSet();
	return AttrSet ? AttrSet->GetArmour() : 0.0f;  // Note: British spelling "Armour" in AttributeSet
}

float UStatsManager::GetBlockStrength() const
{
	UHunterAttributeSet* AttrSet = GetAttributeSet();
	return AttrSet ? AttrSet->GetBlockStrength() : 0.0f;
}

/* ═══════════════════════════════════════════════════════════════════════ */
/* RESISTANCES (Flat + Percent)                                            */
/* ═══════════════════════════════════════════════════════════════════════ */

float UStatsManager::GetFireResistanceFlat() const
{
	UHunterAttributeSet* AttrSet = GetAttributeSet();
	return AttrSet ? AttrSet->GetFireResistanceFlatBonus() : 0.0f;
}

float UStatsManager::GetFireResistancePercent() const
{
	UHunterAttributeSet* AttrSet = GetAttributeSet();
	return AttrSet ? AttrSet->GetFireResistancePercentBonus() : 0.0f;
}

float UStatsManager::GetIceResistanceFlat() const
{
	UHunterAttributeSet* AttrSet = GetAttributeSet();
	return AttrSet ? AttrSet->GetIceResistanceFlatBonus() : 0.0f;
}

float UStatsManager::GetIceResistancePercent() const
{
	UHunterAttributeSet* AttrSet = GetAttributeSet();
	return AttrSet ? AttrSet->GetIceResistancePercentBonus() : 0.0f;
}

float UStatsManager::GetLightningResistanceFlat() const
{
	UHunterAttributeSet* AttrSet = GetAttributeSet();
	return AttrSet ? AttrSet->GetLightningResistanceFlatBonus() : 0.0f;
}

float UStatsManager::GetLightningResistancePercent() const
{
	UHunterAttributeSet* AttrSet = GetAttributeSet();
	return AttrSet ? AttrSet->GetLightningResistancePercentBonus() : 0.0f;
}

float UStatsManager::GetLightResistanceFlat() const
{
	UHunterAttributeSet* AttrSet = GetAttributeSet();
	return AttrSet ? AttrSet->GetLightResistanceFlatBonus() : 0.0f;
}

float UStatsManager::GetLightResistancePercent() const
{
	UHunterAttributeSet* AttrSet = GetAttributeSet();
	return AttrSet ? AttrSet->GetLightResistancePercentBonus() : 0.0f;
}

float UStatsManager::GetCorruptionResistanceFlat() const
{
	UHunterAttributeSet* AttrSet = GetAttributeSet();
	return AttrSet ? AttrSet->GetCorruptionResistanceFlatBonus() : 0.0f;
}

float UStatsManager::GetCorruptionResistancePercent() const
{
	UHunterAttributeSet* AttrSet = GetAttributeSet();
	return AttrSet ? AttrSet->GetCorruptionResistancePercentBonus() : 0.0f;
}

/* ═══════════════════════════════════════════════════════════════════════ */
/* MOVEMENT                                                                */
/* ═══════════════════════════════════════════════════════════════════════ */

float UStatsManager::GetMovementSpeed() const
{
	UHunterAttributeSet* AttrSet = GetAttributeSet();
	return AttrSet ? AttrSet->GetMovementSpeed() : 100.0f;
}

/* ═══════════════════════════════════════════════════════════════════════ */
/* GENERIC ATTRIBUTE ACCESS                                                */
/* ═══════════════════════════════════════════════════════════════════════ */

float UStatsManager::GetAttributeByName(FName AttributeName) const
{
	UHunterAttributeSet* AttrSet = GetAttributeSet();
	if (!AttrSet)
	{
		return 0.0f;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		return 0.0f;
	}

	FGameplayAttribute Attribute = UHunterAttributeSet::FindAttributeByName(AttributeName);
	
	if (Attribute.IsValid())
	{
		return ASC->GetNumericAttribute(Attribute);
	}

	return 0.0f;
}

TMap<FName, float> UStatsManager::GetAllAttributes() const
{
	TMap<FName, float> Attributes;
	UHunterAttributeSet* AttrSet = GetAttributeSet();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();

	if (!AttrSet || !ASC)
	{
		return Attributes;
	}

	TArray<FGameplayAttribute> AllAttributes;
	AttrSet->GetAllAttributes(AllAttributes);

	for (const FGameplayAttribute& Attribute : AllAttributes)
	{
		FString AttributeName = Attribute.GetName();
		float Value = ASC->GetNumericAttribute(Attribute);
		Attributes.Add(FName(*AttributeName), Value);
	}

	return Attributes;
}

/* ═══════════════════════════════════════════════════════════════════════ */
/* STAT CALCULATIONS                                                       */
/* ═══════════════════════════════════════════════════════════════════════ */

float UStatsManager::CalculateArmorReduction(float IncomingDamage) const
{
	float Armor = GetArmor();
	
	if (Armor <= 0.0f || IncomingDamage <= 0.0f)
	{
		return IncomingDamage;
	}

	float Reduction = Armor / (Armor + 10.0f * IncomingDamage);
	float ReducedDamage = IncomingDamage * (1.0f - Reduction);

	return FMath::Max(ReducedDamage, 0.0f);
}

float UStatsManager::CalculateEffectiveHealth() const
{
	float HP = GetMaxHealth();
	float Armor = GetArmor();

	float ArmorMultiplier = 1.0f + (Armor / 100.0f);

	return HP * ArmorMultiplier;
}

float UStatsManager::CalculateTotalDPS() const
{
	float MinPhys, MaxPhys;
	GetPhysicalDamageRange(MinPhys, MaxPhys);
	
	float MinFire, MaxFire, MinIce, MaxIce, MinLight, MaxLight;
	GetElementalDamageRange(MinFire, MaxFire, MinIce, MaxIce, MinLight, MaxLight);

	float AvgPhysDamage = (MinPhys + MaxPhys) / 2.0f;
	float AvgFireDamage = (MinFire + MaxFire) / 2.0f;
	float AvgIceDamage = (MinIce + MaxIce) / 2.0f;
	float AvgLightDamage = (MinLight + MaxLight) / 2.0f;
	
	float TotalAvgDamage = AvgPhysDamage + AvgFireDamage + AvgIceDamage + AvgLightDamage;

	float CritChance = GetCriticalStrikeChance() / 100.0f;
	float CritMult = GetCriticalStrikeMultiplier() / 100.0f;
	
	float CritMultiplier = 1.0f + (CritChance * (CritMult - 1.0f));
	TotalAvgDamage *= CritMultiplier;

	float AttackSpeed = GetAttackSpeed() / 100.0f;
	float BaseAttacksPerSecond = 1.0f;
	float ActualAttacksPerSecond = BaseAttacksPerSecond * AttackSpeed;

	return TotalAvgDamage * ActualAttacksPerSecond;
}

float UStatsManager::GetPowerLevel() const
{
	float OffensivePower = CalculateTotalDPS();
	float DefensivePower = CalculateEffectiveHealth() / 100.0f;

	float TotalPrimaryStats = GetStrength() + GetIntelligence() + GetDexterity() + 
	                          GetEndurance() + GetAffliction() + GetLuck() + GetCovenant();

	return (OffensivePower + DefensivePower + TotalPrimaryStats) / 10.0f;
}

/* ═══════════════════════════════════════════════════════════════════════ */
/* STAT COMPARISONS                                                        */
/* ═══════════════════════════════════════════════════════════════════════ */

float UStatsManager::CompareStatsWithCharacter(AActor* OtherActor) const
{
	if (!OtherActor)
	{
		return 1.0f;
	}

	UStatsManager* OtherStats = OtherActor->FindComponentByClass<UStatsManager>();
	if (!OtherStats)
	{
		return 1.0f;
	}

	float MyPower = GetPowerLevel();
	float TheirPower = OtherStats->GetPowerLevel();

	if (TheirPower <= 0.0f)
	{
		return 1.0f;
	}

	return MyPower / TheirPower;
}

bool UStatsManager::MeetsStatRequirements(const TMap<FName, float>& Requirements) const
{
	for (const TPair<FName, float>& Requirement : Requirements)
	{
		float CurrentValue = GetAttributeByName(Requirement.Key);
		
		if (CurrentValue < Requirement.Value)
		{
			return false;
		}
	}

	return true;
}

/* ═══════════════════════════════════════════════════════════════════════ */
/* STAT INITIALIZATION                                                     */
/* ═══════════════════════════════════════════════════════════════════════ */

void UStatsManager::InitializeFromDataAsset(UBaseStatsData* InStatsData)
{
	if (!StatsData)
	{
		UE_LOG(LogTemp, Warning, TEXT("InitializeFromDataAsset: StatsData is null"));
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		UE_LOG(LogTemp, Error, TEXT("InitializeFromDataAsset: No AbilitySystemComponent found"));
		return;
	}

	if (!GetOwner()->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("InitializeFromDataAsset: Must be called on server"));
		return;
	}

	TMap<FName, float> StatsMap = StatsData->GetAllStatsAsMap();

	for (const TPair<FName, float>& Pair : StatsMap)
	{
		SetStatValue(Pair.Key, Pair.Value);
	}

	FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
	EffectContext.AddSourceObject(GetOwner());

	for (TSubclassOf<UGameplayEffect> EffectClass : StatsData->InitializationEffects)
	{
		if (EffectClass)
		{
			FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(EffectClass, 1.0f, EffectContext);
			if (SpecHandle.IsValid())
			{
				ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Stats initialized from %s"), *StatsData->StatSetName.ToString());
}

void UStatsManager::InitializeFromMap(const TMap<FName, float>& StatsMap) const
{
	if (!GetOwner()->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("InitializeFromMap: Must be called on server"));
		return;
	}

	for (const TPair<FName, float>& Pair : StatsMap)
	{
		SetStatValue(Pair.Key, Pair.Value);
	}

	UE_LOG(LogTemp, Log, TEXT("Stats initialized from map (%d attributes)"), StatsMap.Num());
}

void UStatsManager::SetStatValue(FName AttributeName, float Value) const
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	UHunterAttributeSet* AttrSet = GetAttributeSet();

	if (!ASC || !AttrSet)
	{
		UE_LOG(LogTemp, Error, TEXT("SetStatValue: ASC or AttributeSet is null"));
		return;
	}

	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	FGameplayAttribute Attribute = UHunterAttributeSet::FindAttributeByName(AttributeName);
	
	if (!Attribute.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("SetStatValue: Unknown attribute '%s'"), *AttributeName.ToString());
		return;
	}

	ASC->SetNumericAttributeBase(Attribute, Value);

	// Auto-fill current values for vitals
	if (AttributeName == "MaxHealth")
	{
		FGameplayAttribute HealthAttr = UHunterAttributeSet::GetHealthAttribute();
		ASC->SetNumericAttributeBase(HealthAttr, Value);
	}
	else if (AttributeName == "MaxMana")
	{
		FGameplayAttribute ManaAttr = UHunterAttributeSet::GetManaAttribute();
		ASC->SetNumericAttributeBase(ManaAttr, Value);
	}
	else if (AttributeName == "MaxStamina")
	{
		FGameplayAttribute StaminaAttr = UHunterAttributeSet::GetStaminaAttribute();
		ASC->SetNumericAttributeBase(StaminaAttr, Value);
	}
	else if (AttributeName == "MaxArcaneShield")
	{
		FGameplayAttribute ShieldAttr = UHunterAttributeSet::GetArcaneShieldAttribute();
		ASC->SetNumericAttributeBase(ShieldAttr, Value);
	}
}