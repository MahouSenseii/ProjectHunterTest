// Copyright © 2025 MahouSensei
// Author: Quentin Davis

#include "HunterBaseComponent.h"
#include "AbilitySystemInterface.h" 
#include "AbilitySystemComponent.h"

// Sets default values for this component's properties
UHunterBaseComponent::UHunterBaseComponent()
{
	// Set to false to save performance will use custom tick if needed 
	// also no need to base to use tick 
	PrimaryComponentTick.bCanEverTick = false;
}


// Called when the game starts
void UHunterBaseComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeASCCache();
}

void UHunterBaseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (CachedASC)
	{
		for (const FDelegateHandle& Handle : AttributeChangeDelegateHandles)
		{
			CachedASC->GetGameplayAttributeValueChangeDelegate(FGameplayAttribute()).Remove(Handle);
		}
		AttributeChangeDelegateHandles.Empty();
	}

	CachedASC = nullptr;
	//CachedAttributeSet = nullptr;

	Super::EndPlay(EndPlayReason);
}

void UHunterBaseComponent::RefreshASCCache()
{
	//Clean up 
	if (CachedASC)
	{
		for (const FDelegateHandle& Handle : AttributeChangeDelegateHandles)
		{
			CachedASC->GetGameplayAttributeValueChangeDelegate(FGameplayAttribute()).Remove(Handle);
		}
		AttributeChangeDelegateHandles.Empty();
	}

	// Re-initialize
	InitializeASCCache();
}


void UHunterBaseComponent::InitializeASCCache()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		LogDebug("No owner found during ASC initialization", true);
		return;
	}

	// Try to get ASC from owner
	if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Owner))
	{
		CachedASC = ASI->GetAbilitySystemComponent();
	}

	if (!CachedASC)
	{
		LogDebug("Owner does not have an AbilitySystemComponent", true);
		return;
	}

	/*// Cache the attribute set
	CachedAttributeSet = CachedASC->GetSet<UPHAttributeSet>();

	if (!CachedAttributeSet)
	{
		LogDebug("Could not find PHAttributeSet on ASC", true);
	}*/

	// Bind to ALL attribute changes (single callback handles everything)
	FDelegateHandle Handle = CachedASC->GetGameplayAttributeValueChangeDelegate(FGameplayAttribute())
		.AddUObject(this, &UHunterBaseComponent::HandleAttributeChanged);
    
	AttributeChangeDelegateHandles.Add(Handle);

	// Notify derived classes
	OnASCInitialized();

	LogDebug("ASC successfully cached and initialized");
}

void UHunterBaseComponent::HandleAttributeChanged(const FOnAttributeChangeData& Data)
{
	OnAttributeChanged.Broadcast(Data.Attribute, Data.NewValue, Data.OldValue);
}


void UHunterBaseComponent::LogDebug(const FString& Message, const bool bWarning) const
{
	const FString ComponentName = GetName();
	const FString OwnerName = GetOwner() ? GetOwner()->GetName() : TEXT("NoOwner");
	const FString FullMessage = FString::Printf(TEXT("[%s on %s] %s"), *ComponentName, *OwnerName, *Message);

	if (bWarning)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s"), *FullMessage);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("%s"), *FullMessage);
	}
}
