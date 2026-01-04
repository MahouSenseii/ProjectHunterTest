// Copyright © 2025 MahouSensei
// Author: Quentin Davis

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "GameplayEffectTypes.h"
#include "Components/ActorComponent.h"
#include "HunterBaseComponent.generated.h"


class UHunterAttributeSet;
class UAbilitySystemComponent;


DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnAttributeChanged, FGameplayAttribute, Attribute, float, NewValue, float, OldValue);


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PROJECTHUNTERTEST_API UHunterBaseComponent : public UActorComponent
{
	GENERATED_BODY()


	/** Functions **/
public:	
	// Sets default values for this component's properties
	UHunterBaseComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual auto EndPlay(const EEndPlayReason::Type EndPlayReason) -> void override;

	virtual void OnAscInitialized() {}

	UFUNCTION(BlueprintCallable, Category = "Hunter|ASC")
	void RefreshASCCache();

	// Called when ASC is successfully initialized
	virtual void OnASCInitialized() {}

	UFUNCTION(BlueprintPure, Category = "Hunter|ASC")
	UAbilitySystemComponent* GetCachedASC() const { return CachedASC;}

	UFUNCTION(BlueprintPure, Category = "Hunter|ASC")
	bool HasValidASC() const { return CachedASC != nullptr; }


	//Helper
	// Will return actor as a Type
	template<typename T> T* GetOwnerAs() const { return Cast<T>(GetOwner());}

		
private:

	// Initialize ASC cache and bind to attribute changes
	void InitializeASCCache();

	// Callback when any attribute changes
	void HandleAttributeChanged(const FOnAttributeChangeData& Data);
	void LogDebug(const FString& Message, bool bWarning = false) const;


	// Track bound attributes to unbind on cleanup
	TArray<FDelegateHandle> AttributeChangeDelegateHandles;

	/** Variables **/
public:

	// Delegate that fires when any attribute changes
	UPROPERTY(BlueprintAssignable, Category = "Hunter|Attributes")
	FOnAttributeChanged OnAttributeChanged;
	
	// Cached ASC reference only updated when an owner changes or ASC is granted/removed
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> CachedASC;

	
	// Cached attribute set reference for faster access
	UPROPERTY()
	TObjectPtr<const UHunterAttributeSet> CachedAttributeSet;
	
protected:

private:
	
};
