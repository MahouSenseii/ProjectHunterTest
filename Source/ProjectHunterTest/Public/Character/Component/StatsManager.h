#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/BaseStatsData.h"
#include "GameplayEffectTypes.h"
#include "StatsManager.generated.h"

// Forward declarations
class UAbilitySystemComponent;
class UHunterAttributeSet;
class UItemInstance;
class UGameplayEffect;
struct FPHAttributeData;
struct FGameplayAttribute;

/**
 * Stats Manager Component
 * Handles all stat queries, attribute access, and stat-related calculations
 * Provides clean interface for accessing character stats
 * 
 * SEPARATION OF CONCERNS:
 * - HunterBaseCharacter: Core combat, death, abilities
 * - StatsManager: Stats queries, attribute access, calculations
 * - ProgressionManager: XP, leveling, stat point spending
 * - EquipmentManager: Items, gear management (calls StatsManager to apply/remove stats)
 * 
 * EQUIPMENT INTEGRATION:
 * - EquipmentManager calls ApplyEquipmentStats/RemoveEquipmentStats
 * - StatsManager applies stats via GAS (Gameplay Ability System)
 * - Tracks active equipment effects by item GUID
 * - Works with FPHAttributeData system (Prefixes/Suffixes/Implicits/Crafted)
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECTHUNTERTEST_API UStatsManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UStatsManager();

	virtual void BeginPlay() override;

	/* ═══════════════════════════════════════════════════════════════════════ */
	/* EQUIPMENT INTEGRATION (Required by EquipmentManager) */
	/* ═══════════════════════════════════════════════════════════════════════ */

	/**
	 * Apply stats from an equipped item via GAS
	 * Called by EquipmentManager when item is equipped
	 * Works with FPHAttributeData arrays (Prefixes/Suffixes/Implicits/Crafted)
	 * @param Item - Item that was equipped
	 */
	UFUNCTION(BlueprintCallable, Category = "Stats|Equipment")
	void ApplyEquipmentStats(UItemInstance* Item);

	/**
	 * Remove stats from an unequipped item via GAS
	 * Called by EquipmentManager when item is unequipped
	 * @param Item - Item that was unequipped
	 */
	UFUNCTION(BlueprintCallable, Category = "Stats|Equipment")
	void RemoveEquipmentStats(UItemInstance* Item);

	/**
	 * Refresh all equipment stats (useful after stat recalculation)
	 * Removes all equipment effects and reapplies them
	 */
	UFUNCTION(BlueprintCallable, Category = "Stats|Equipment")
	void RefreshEquipmentStats();

	/**
	 * Check if item's stats are currently applied
	 */
	UFUNCTION(BlueprintPure, Category = "Stats|Equipment")
	bool HasEquipmentStatsApplied(UItemInstance* Item) const;

	/* ═══════════════════════════════════════════════════════════════════════ */
	/* PRIMARY ATTRIBUTES */
	/* ═══════════════════════════════════════════════════════════════════════ */

	/** Get Strength attribute value */
	UFUNCTION(BlueprintPure, Category = "Stats|Primary")
	float GetStrength() const;

	/** Get Intelligence attribute value */
	UFUNCTION(BlueprintPure, Category = "Stats|Primary")
	float GetIntelligence() const;

	/** Get Dexterity attribute value */
	UFUNCTION(BlueprintPure, Category = "Stats|Primary")
	float GetDexterity() const;

	/** Get Endurance attribute value */
	UFUNCTION(BlueprintPure, Category = "Stats|Primary")
	float GetEndurance() const;

	/** Get Affliction attribute value */
	UFUNCTION(BlueprintPure, Category = "Stats|Primary")
	float GetAffliction() const;

	/** Get Luck attribute value */
	UFUNCTION(BlueprintPure, Category = "Stats|Primary")
	float GetLuck() const;

	/** Get Covenant attribute value */
	UFUNCTION(BlueprintPure, Category = "Stats|Primary")
	float GetCovenant() const;

	/** Get any primary attribute by name */
	UFUNCTION(BlueprintPure, Category = "Stats|Primary")
	float GetPrimaryAttribute(FName AttributeName) const;

	/** Get all primary attributes as map */
	UFUNCTION(BlueprintPure, Category = "Stats|Primary")
	TMap<FName, float> GetAllPrimaryAttributes() const;

	/* ═══════════════════════════════════════════════════════════════════════ */
	/* VITAL ATTRIBUTES (Health, Mana, Stamina, etc.) */
	/* ═══════════════════════════════════════════════════════════════════════ */

	/** Get current health */
	UFUNCTION(BlueprintPure, Category = "Stats|Vitals")
	float GetHealth() const;

	/** Get max health */
	UFUNCTION(BlueprintPure, Category = "Stats|Vitals")
	float GetMaxHealth() const;

	/** Get health percent (0.0 - 1.0) */
	UFUNCTION(BlueprintPure, Category = "Stats|Vitals")
	float GetHealthPercent() const;

	/** Get current mana */
	UFUNCTION(BlueprintPure, Category = "Stats|Vitals")
	float GetMana() const;

	/** Get max mana */
	UFUNCTION(BlueprintPure, Category = "Stats|Vitals")
	float GetMaxMana() const;

	/** Get mana percent (0.0 - 1.0) */
	UFUNCTION(BlueprintPure, Category = "Stats|Vitals")
	float GetManaPercent() const;

	/** Get current stamina */
	UFUNCTION(BlueprintPure, Category = "Stats|Vitals")
	float GetStamina() const;

	/** Get max stamina */
	UFUNCTION(BlueprintPure, Category = "Stats|Vitals")
	float GetMaxStamina() const;

	/** Get stamina percent (0.0 - 1.0) */
	UFUNCTION(BlueprintPure, Category = "Stats|Vitals")
	float GetStaminaPercent() const;

	/** Get current arcane shield */
	UFUNCTION(BlueprintPure, Category = "Stats|Vitals")
	float GetArcaneShield() const;

	/** Get max arcane shield */
	UFUNCTION(BlueprintPure, Category = "Stats|Vitals")
	float GetMaxArcaneShield() const;

	/* ═══════════════════════════════════════════════════════════════════════ */
	/* COMBAT STATS */
	/* ═══════════════════════════════════════════════════════════════════════ */

	/** Get physical damage (min-max range) */
	UFUNCTION(BlueprintPure, Category = "Stats|Combat")
	void GetPhysicalDamageRange(float& OutMin, float& OutMax) const;

	/** Get elemental damage (fire, ice, lightning) */
	UFUNCTION(BlueprintPure, Category = "Stats|Combat")
	void GetElementalDamageRange(float& OutFireMin, float& OutFireMax,
	                              float& OutIceMin, float& OutIceMax,
	                              float& OutLightningMin, float& OutLightningMax) const;

	/** Get critical strike chance */
	UFUNCTION(BlueprintPure, Category = "Stats|Combat")
	float GetCriticalStrikeChance() const;

	/** Get critical strike multiplier */
	UFUNCTION(BlueprintPure, Category = "Stats|Combat")
	float GetCriticalStrikeMultiplier() const;

	/** Get attack speed multiplier */
	UFUNCTION(BlueprintPure, Category = "Stats|Combat")
	float GetAttackSpeed() const;

	/** Get cast speed multiplier */
	UFUNCTION(BlueprintPure, Category = "Stats|Combat")
	float GetCastSpeed() const;

	/* ═══════════════════════════════════════════════════════════════════════ */
	/* DEFENSE STATS */
	/* ═══════════════════════════════════════════════════════════════════════ */

	/** Get armor value */
	UFUNCTION(BlueprintPure, Category = "Stats|Defense")
	float GetArmor() const;

	/** Get block strength */
	UFUNCTION(BlueprintPure, Category = "Stats|Defense")
	float GetBlockStrength() const;

	/* ═══════════════════════════════════════════════════════════════════════ */
	/* RESISTANCES (Flat + Percent for each element) */
	/* ═══════════════════════════════════════════════════════════════════════ */

	/** Get fire resistance (flat bonus) */
	UFUNCTION(BlueprintPure, Category = "Stats|Resistance")
	float GetFireResistanceFlat() const;

	/** Get fire resistance (percent bonus) */
	UFUNCTION(BlueprintPure, Category = "Stats|Resistance")
	float GetFireResistancePercent() const;

	/** Get ice resistance (flat bonus) */
	UFUNCTION(BlueprintPure, Category = "Stats|Resistance")
	float GetIceResistanceFlat() const;

	/** Get ice resistance (percent bonus) */
	UFUNCTION(BlueprintPure, Category = "Stats|Resistance")
	float GetIceResistancePercent() const;

	/** Get lightning resistance (flat bonus) */
	UFUNCTION(BlueprintPure, Category = "Stats|Resistance")
	float GetLightningResistanceFlat() const;

	/** Get lightning resistance (percent bonus) */
	UFUNCTION(BlueprintPure, Category = "Stats|Resistance")
	float GetLightningResistancePercent() const;

	/** Get light resistance (flat bonus) */
	UFUNCTION(BlueprintPure, Category = "Stats|Resistance")
	float GetLightResistanceFlat() const;

	/** Get light resistance (percent bonus) */
	UFUNCTION(BlueprintPure, Category = "Stats|Resistance")
	float GetLightResistancePercent() const;

	/** Get corruption resistance (flat bonus) */
	UFUNCTION(BlueprintPure, Category = "Stats|Resistance")
	float GetCorruptionResistanceFlat() const;

	/** Get corruption resistance (percent bonus) */
	UFUNCTION(BlueprintPure, Category = "Stats|Resistance")
	float GetCorruptionResistancePercent() const;

	/* ═══════════════════════════════════════════════════════════════════════ */
	/* MOVEMENT */
	/* ═══════════════════════════════════════════════════════════════════════ */

	/** Get movement speed */
	UFUNCTION(BlueprintPure, Category = "Stats|Movement")
	float GetMovementSpeed() const;

	/* ═══════════════════════════════════════════════════════════════════════ */
	/* GENERIC ATTRIBUTE ACCESS */
	/* ═══════════════════════════════════════════════════════════════════════ */

	/**
	 * Get any attribute value by name
	 * @param AttributeName - Name of the attribute (e.g., "Strength", "Health", "CritChance")
	 * @return Attribute value, or 0 if not found
	 */
	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetAttributeByName(FName AttributeName) const;

	/**
	 * Get all attributes as a map
	 * WARNING: Expensive operation, use sparingly
	 */
	UFUNCTION(BlueprintPure, Category = "Stats")
	TMap<FName, float> GetAllAttributes() const;

	/* ═══════════════════════════════════════════════════════════════════════ */
	/* STAT CALCULATIONS */
	/* ═══════════════════════════════════════════════════════════════════════ */

	/**
	 * Calculate effective damage reduction from armor
	 * @param IncomingDamage - Raw damage before armor
	 * @return Damage after armor reduction
	 */
	UFUNCTION(BlueprintPure, Category = "Stats|Calculations")
	float CalculateArmorReduction(float IncomingDamage) const;

	/**
	 * Calculate effective health (Health + Armor + Resistances)
	 * Useful for AI threat assessment
	 */
	UFUNCTION(BlueprintPure, Category = "Stats|Calculations")
	float CalculateEffectiveHealth() const;

	/**
	 * Calculate total DPS (Damage Per Second)
	 * Includes physical, elemental, crit, and attack speed
	 */
	UFUNCTION(BlueprintPure, Category = "Stats|Calculations")
	float CalculateTotalDPS() const;

	/**
	 * Get character "power level" (for matchmaking, difficulty scaling)
	 * Combines level, stats, equipment quality
	 */
	UFUNCTION(BlueprintPure, Category = "Stats|Calculations")
	float GetPowerLevel() const;

	/* ═══════════════════════════════════════════════════════════════════════ */
	/* STAT COMPARISONS */
	/* ═══════════════════════════════════════════════════════════════════════ */

	/**
	 * Compare stats with another character
	 * Returns percentage difference (1.0 = equal, 1.5 = 50% stronger)
	 */
	UFUNCTION(BlueprintPure, Category = "Stats|Comparison")
	float CompareStatsWithCharacter(AActor* OtherCharacter) const;

	/**
	 * Check if character meets stat requirements
	 * @param Requirements - Map of attribute names to required values
	 */
	UFUNCTION(BlueprintPure, Category = "Stats|Comparison")
	bool MeetsStatRequirements(const TMap<FName, float>& Requirements) const;

	/** Initialize stats from data asset */
	void InitializeFromDataAsset(UBaseStatsData* InStatsData);

	/** Initialize stats from map */
	void InitializeFromMap(const TMap<FName, float>& StatsMap) const;

	/** Set single stat value */
	void SetStatValue(FName AttributeName, float Value) const;

protected:
	/* ═══════════════════════════════════════════════════════════════════════ */
	/* INTERNAL HELPERS */
	/* ═══════════════════════════════════════════════════════════════════════ */

	/** Get AttributeSet from owner */
	UHunterAttributeSet* GetAttributeSet() const;

	/** Get AbilitySystemComponent from owner */
	UAbilitySystemComponent* GetAbilitySystemComponent() const;

	/**
	 * Create a gameplay effect for equipment stats
	 * @param Item - Item to create effect for
	 * @param Stats - Array of FPHAttributeData from item
	 * @return Gameplay effect spec handle
	 */
	FGameplayEffectSpecHandle CreateEquipmentEffect(UItemInstance* Item, const TArray<FPHAttributeData>& Stats);

	/**
	 * Apply a single stat modifier to effect
	 * @param Effect - UGameplayEffect to modify
	 * @param Stat - FPHAttributeData with modifier info
	 * @param Attribute - Target gameplay attribute
	 * @return True if modifier was added successfully
	 */
	bool ApplyStatModifier(UGameplayEffect* Effect, const FPHAttributeData& Stat, const FGameplayAttribute& Attribute);

	/** Cached references */
	UPROPERTY()
	TObjectPtr<UHunterAttributeSet> CachedAttributeSet;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Cached References")
	UBaseStatsData* StatsData;

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> CachedASC;

	/**
	 * Active equipment effects
	 * Maps item GUID to gameplay effect handle
	 * Used to remove effects when equipment is unequipped
	 */
	UPROPERTY()
	TMap<FGuid, FActiveGameplayEffectHandle> ActiveEquipmentEffects;
};