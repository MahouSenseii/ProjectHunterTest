// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/HunterBaseCharacter.h"
#include "AbilitySystem/HunterAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Character/Component/CharacterProgressionManager.h"
#include "Character/Component/StatsManager.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "Character/Component/EquipmentManager.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"



AHunterBaseCharacter::AHunterBaseCharacter(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	// Create Ability System Component
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// Create Attribute Set
	AttributeSet = CreateDefaultSubobject<UHunterAttributeSet>(TEXT("AttributeSet"));

	// Create Progression Manager
	ProgressionManager = CreateDefaultSubobject<UCharacterProgressionManager>(TEXT("ProgressionManager"));

	// Create Equipment Component
	EquipmentManager = CreateDefaultSubobject<UEquipmentManager>(TEXT("EquipmentComponent"));

	// Create Stats Manager
	StatsManager = CreateDefaultSubobject<UStatsManager>(TEXT("StatsManager"));
}

void AHunterBaseCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHunterBaseCharacter, CharacterName);
	DOREPLIFETIME(AHunterBaseCharacter, bIsDead);
	DOREPLIFETIME(AHunterBaseCharacter, TeamID);
}


void AHunterBaseCharacter::BeginPlay()
{
	Super::BeginPlay();

	
}

void AHunterBaseCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Initialize ASC on server
	if (AbilitySystemComponent && !bAbilitySystemInitialized)
	{
		InitializeAbilitySystem();
	}

	
	
}

void AHunterBaseCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// Initialize ASC on client
	if (AbilitySystemComponent && !bAbilitySystemInitialized)
	{
		InitializeAbilitySystem();
	}
}

/* ═══════════════════════════════════════════════════════════════════════ */
/* INITIALIZATION */
/* ═══════════════════════════════════════════════════════════════════════ */

void AHunterBaseCharacter::InitializeAbilitySystem()
{
	if (bAbilitySystemInitialized)
	{
		return;
	}

	if (!AbilitySystemComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("InitializeAbilitySystem: AbilitySystemComponent is null!"));
		return;
	}

	// Initialize ASC with self as owner and avatar
	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	// Initialize attributes with base values
	InitializeAttributes();

	// Grant default abilities
	if (HasAuthority())
	{
		GiveDefaultAbilities();
		ApplyStartupEffects();
	}

	// Bind to attribute changes
	BindAttributeDelegates();

	bAbilitySystemInitialized = true;

	// Call virtual function for subclass initialization
	OnAbilitySystemInitialized();

	UE_LOG(LogTemp, Log, TEXT("Ability System initialized for %s"), *GetName());
}

void AHunterBaseCharacter::InitializeAttributes()
{
	if (!AttributeSet)
	{
		return;
	}
	
	// Base attributes will be set by:
	// 1. StartupEffects (for base values)
	// 2. ProgressionManager (for stat points)
	// 3. EquipmentComponent (for item stats)
}

void AHunterBaseCharacter::BindAttributeDelegates()
{
	if (!AbilitySystemComponent || !AttributeSet)
	{
		return;
	}

	// Bind to health changes
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		AttributeSet->GetHealthAttribute()).AddUObject(this, &AHunterBaseCharacter::HandleHealthChanged);
}

void AHunterBaseCharacter::OnAbilitySystemInitialized()
{
	// Override in subclasses for custom initialization
}

/* ═══════════════════════════════════════════════════════════════════════ */
/* PROGRESSION */
/* ═══════════════════════════════════════════════════════════════════════ */

int32 AHunterBaseCharacter::GetCharacterLevel() const
{
	if (ProgressionManager)
	{
		return ProgressionManager->Level;
	}
	return 1;
}

int64 AHunterBaseCharacter::GetXPReward() const
{
	return GetCharacterLevel() * 100;
}

void AHunterBaseCharacter::AwardExperienceFromKill(AHunterBaseCharacter* KilledCharacter)
{
	if (!ProgressionManager || !KilledCharacter)
	{
		return;
	}

	// Delegate to ProgressionManager
	ProgressionManager->AwardExperienceFromKill(KilledCharacter);
}

/* ═══════════════════════════════════════════════════════════════════════ */
/* COMBAT & HEALTH */
/* ═══════════════════════════════════════════════════════════════════════ */

float AHunterBaseCharacter::GetHealth() const
{
	if (StatsManager)
	{
		return StatsManager->GetHealth();
	}
	return 0.0f;
}

float AHunterBaseCharacter::GetMaxHealth() const
{
	if (StatsManager)
	{
		return StatsManager->GetMaxHealth();
	}
	return 1.0f;
}

float AHunterBaseCharacter::GetHealthPercent() const
{
	if (StatsManager)
	{
		return StatsManager->GetHealthPercent();
	}
	return 0.0f;
}




void AHunterBaseCharacter::OnDeath_Implementation(AController* Killer, AActor* DamageCauser)
{
	if (bIsDead)
	{
		return; // Already dead
	}

	bIsDead = true;

	UE_LOG(LogTemp, Log, TEXT("%s died"), *GetName());

	// Play death animation
	PlayDeathAnimation();

	// Disable collision
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Disable movement
	GetCharacterMovement()->DisableMovement();

	// Award XP to killer (if killer has ProgressionManager)
	if (Killer)
	{
		APawn* KillerPawn = Killer->GetPawn();
		AHunterBaseCharacter* KillerCharacter = Cast<AHunterBaseCharacter>(KillerPawn);
		
		if (KillerCharacter)
		{
			KillerCharacter->AwardExperienceFromKill(this);
		}
	}

	// Override in subclasses for additional death behavior
	// (drop loot, play effects, respawn timer, etc.)
}

void AHunterBaseCharacter::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
}

void AHunterBaseCharacter::PlayDeathAnimation()
{
	if (DeathMontage)
	{
		PlayAnimMontage(DeathMontage);
	}
}

void AHunterBaseCharacter::PlayHitReactAnimation()
{
	if (HitReactMontage)
	{
		PlayAnimMontage(HitReactMontage);
	}
}

/* ═══════════════════════════════════════════════════════════════════════ */
/* ABILITIES */
/* ═══════════════════════════════════════════════════════════════════════ */

void AHunterBaseCharacter::GiveDefaultAbilities()
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	// Grant all default abilities
	for (TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
	{
		if (AbilityClass)
		{
			FGameplayAbilitySpec AbilitySpec(AbilityClass, 1, INDEX_NONE, this);
			FGameplayAbilitySpecHandle Handle = AbilitySystemComponent->GiveAbility(AbilitySpec);
			GrantedAbilityHandles.Add(Handle);
		}
	}
}

void AHunterBaseCharacter::ApplyStartupEffects()
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	// Apply all startup effects
	for (TSubclassOf<UGameplayEffect>& EffectClass : StartupEffects)
	{
		if (EffectClass)
		{
			FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(
				EffectClass, 1, EffectContext);
			
			if (SpecHandle.IsValid())
			{
				AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
			}
		}
	}
}

void AHunterBaseCharacter::RemoveAllAbilities()
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	// Remove all granted abilities
	for (FGameplayAbilitySpecHandle& Handle : GrantedAbilityHandles)
	{
		AbilitySystemComponent->ClearAbility(Handle);
	}

	GrantedAbilityHandles.Empty();
}

/* ═══════════════════════════════════════════════════════════════════════ */
/* TEAM & TARGETING */
/* ═══════════════════════════════════════════════════════════════════════ */

bool AHunterBaseCharacter::IsSameTeam(const AHunterBaseCharacter* OtherCharacter) const
{
	if (!OtherCharacter)
	{
		return false;
	}

	return TeamID == OtherCharacter->TeamID;
}

bool AHunterBaseCharacter::IsHostile(const AHunterBaseCharacter* OtherCharacter) const
{
	return !IsSameTeam(OtherCharacter);
}