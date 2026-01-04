#pragma once

#include "CoreMinimal.h"
#include "CombatEnumLibrary.generated.h"

/**
 * differentiate between neutral, hostile, and friendly characters. (for targeting)
 */
UENUM(BlueprintType)
enum class ECombatAlignment : uint8
{
	None UMETA(DisplayName = "None"),       // Neutral or player character
	Enemy UMETA(DisplayName = "Enemy"),    // Hostile character
	Ally UMETA(DisplayName = "Ally")       // Friendly character
};

/**
 * Attack types: add more here for different types of attacks 
 */
UENUM(BlueprintType)
enum class EAttackType : uint8
{
	AT_None       UMETA(DisplayName = "None"),
	AT_Melee      UMETA(DisplayName = "Melee"),
	AT_Ranged     UMETA(DisplayName = "Ranged"),
	AT_Spell      UMETA(DisplayName = "Spell")
};

/**
 * Combat status for tracking whether a character is engaged in combat
 */
UENUM(BlueprintType)
enum class ECombatStatus : uint8
{
	OutOfCombat UMETA(DisplayName = "Out of Combat"),
	InCombat UMETA(DisplayName = "In Combat"),
	EnteringCombat UMETA(DisplayName = "Entering Combat"),  // Transition state
	LeavingCombat UMETA(DisplayName = "Leaving Combat")     // Transition state
};