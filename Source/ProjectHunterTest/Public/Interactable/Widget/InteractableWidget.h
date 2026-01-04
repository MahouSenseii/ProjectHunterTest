// Interactable/Widget/InteractableWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InteractableWidget.generated.h"

class UImage;
class UTextBlock;
class UOverlay;
class UProgressBar;
class UMaterialInstanceDynamic;

/**
 * Universal interaction prompt widget
 * Shows "Press [E] to Interact" or "Press [A] to Pick Up"
 * Automatically switches between keyboard/gamepad icons
 * Supports progress bar for hold interactions
 */
UCLASS()
class PROJECTHUNTERTEST_API UInteractableWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ═══════════════════════════════════════════════
	// WIDGET BINDINGS (Set in Blueprint)
	// ═══════════════════════════════════════════════
	
	/** Background image (the cyan square/circle) */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* Img_Background;

	/** Fill/border image (the animated border) */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* Img_FillBorder;

	/** Key icon (E, F, or gamepad button) */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* Img_Key;

	/** Interaction text ("Interact", "Pick Up", etc.) */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* InteractionDescription;

	/** Root overlay */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UOverlay* RootOverlay;

	/** Progress bar for hold interactions (OPTIONAL - can be null) */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UProgressBar* ProgressBar;

	// ═══════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════
	
	/** Material for square border (keyboard) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Materials")
	UMaterialInterface* SquareBorderMaterial;

	/** Material for circle border (gamepad) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Materials")
	UMaterialInterface* CircleBorderMaterial;

	/** Keyboard key textures */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Icons")
	TMap<FName, UTexture2D*> KeyboardIcons;  // "Interact" -> E_Key.png, "Pickup" -> F_Key.png

	/** Gamepad button textures */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Icons")
	TMap<FName, UTexture2D*> GamepadIcons;  // "Interact" -> A_Button.png, "Pickup" -> X_Button.png

	/** Animation speed for border fill */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Animation")
	float BorderAnimSpeed = 1.0f;

	/** Widget scale */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Display")
	float WidgetScale = 1.0f;

	/** Progress bar fill color (normal) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Progress")
	FLinearColor ProgressFillColor = FLinearColor(0.0f, 1.0f, 0.5f, 1.0f); // Green

	/** Progress bar fill color (depleting) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Progress")
	FLinearColor ProgressDepletingColor = FLinearColor(1.0f, 0.3f, 0.0f, 1.0f); // Orange

	// ═══════════════════════════════════════════════
	// PUBLIC METHODS
	// ═══════════════════════════════════════════════
	
	/**
	 * Set interaction data
	 * @param ActionName - Action name (e.g., "Interact", "Pickup")
	 * @param Description - Display text (e.g., "Press To Interact", "Pick Up!")
	 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetInteractionData(FName ActionName, FText Description);

	/**
	 * Update progress bar value [0.0 - 1.0]
	 * @param Progress - Progress value (0.0 = empty, 1.0 = full)
	 * @param bIsDepleting - Is progress depleting? (changes color)
	 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetProgress(float Progress, bool bIsDepleting = false);

	/**
	 * Show/hide progress bar
	 * @param bVisible - Should progress bar be visible?
	 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetProgressBarVisible(bool bVisible);

	/**
	 * Update for input device change
	 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void UpdateInputMode();

	/**
	 * Show/hide entire widget
	 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetVisible(bool bVisible);

private:
	/** Current input mode */
	bool bIsUsingGamepad = false;

	/** Dynamic material instance for border */
	UPROPERTY()
	UMaterialInstanceDynamic* BorderMaterialInstance;

	/** Current action name */
	FName CurrentActionName;

	/** Animation time */
	float AnimTime = 0.0f;

	/** Current progress value */
	float CurrentProgress = 0.0f;

	/** Is progress depleting? */
	bool bProgressIsDepleting = false;

	/** Check if using gamepad */
	bool IsUsingGamepad() const;

	/** Update border material */
	void UpdateBorderMaterial();

	/** Update key icon */
	void UpdateKeyIcon();

	/** Update progress bar color */
	void UpdateProgressColor();
};