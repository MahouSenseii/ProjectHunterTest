// Interactable/Widget/InteractableWidget.cpp

#include "Interactable/Widget/InteractableWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Overlay.h"
#include "Components/ProgressBar.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

void UInteractableWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Create dynamic material instance
	if (SquareBorderMaterial && Img_FillBorder)
	{
		BorderMaterialInstance = UMaterialInstanceDynamic::Create(SquareBorderMaterial, this);
		Img_FillBorder->SetBrushFromMaterial(BorderMaterialInstance);
	}

	// Hide progress bar by default
	if (ProgressBar)
	{
		ProgressBar->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Initial update
	UpdateInputMode();
}

void UInteractableWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Animate border
	if (BorderMaterialInstance)
	{
		AnimTime += InDeltaTime * BorderAnimSpeed;
		
		// Set material parameter for animation (you can customize this)
		BorderMaterialInstance->SetScalarParameterValue(FName("AnimTime"), AnimTime);
		BorderMaterialInstance->SetScalarParameterValue(FName("Progress"), FMath::Sin(AnimTime) * 0.5f + 0.5f);
	}

	// Check for input mode changes
	bool bWasUsingGamepad = bIsUsingGamepad;
	bIsUsingGamepad = IsUsingGamepad();
	
	if (bWasUsingGamepad != bIsUsingGamepad)
	{
		UpdateInputMode();
	}
}

void UInteractableWidget::SetInteractionData(FName ActionName, FText Description)
{
	CurrentActionName = ActionName;

	// Set description text
	if (InteractionDescription)
	{
		InteractionDescription->SetText(Description);
	}

	// Update icon
	UpdateKeyIcon();
}

void UInteractableWidget::SetProgress(float Progress, bool bIsDepleting)
{
	if (!ProgressBar)
	{
		return; // Progress bar not bound in Blueprint
	}

	// Clamp progress
	CurrentProgress = FMath::Clamp(Progress, 0.0f, 1.0f);
	bProgressIsDepleting = bIsDepleting;

	// Update progress bar
	ProgressBar->SetPercent(CurrentProgress);

	// Update color
	UpdateProgressColor();
}

void UInteractableWidget::SetProgressBarVisible(bool bVisible)
{
	if (!ProgressBar)
	{
		return; // Progress bar not bound in Blueprint
	}

	ProgressBar->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UInteractableWidget::UpdateInputMode()
{
	bIsUsingGamepad = IsUsingGamepad();

	// Update border material
	UpdateBorderMaterial();

	// Update key icon
	UpdateKeyIcon();
}

void UInteractableWidget::SetVisible(bool bVisible)
{
	SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

bool UInteractableWidget::IsUsingGamepad() const
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC)
	{
		return false;
	}

	// Check last input device
	// Note: This is a simple check - you might need more robust detection
	return PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftX) != 0.0f ||
	       PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftY) != 0.0f ||
	       PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Bottom);
}

void UInteractableWidget::UpdateBorderMaterial()
{
	if (!Img_FillBorder)
	{
		return;
	}

	// Switch between square (keyboard) and circle (gamepad)
	UMaterialInterface* TargetMaterial = bIsUsingGamepad ? CircleBorderMaterial : SquareBorderMaterial;
	
	if (TargetMaterial)
	{
		BorderMaterialInstance = UMaterialInstanceDynamic::Create(TargetMaterial, this);
		Img_FillBorder->SetBrushFromMaterial(BorderMaterialInstance);
	}
}

void UInteractableWidget::UpdateKeyIcon()
{
	if (!Img_Key)
	{
		return;
	}

	// Get appropriate texture
	UTexture2D* IconTexture = nullptr;
	
	if (bIsUsingGamepad)
	{
		// Use gamepad icon
		if (UTexture2D** Found = GamepadIcons.Find(CurrentActionName))
		{
			IconTexture = *Found;
		}
	}
	else
	{
		// Use keyboard icon
		if (UTexture2D** Found = KeyboardIcons.Find(CurrentActionName))
		{
			IconTexture = *Found;
		}
	}

	// Set icon texture
	if (IconTexture)
	{
		Img_Key->SetBrushFromTexture(IconTexture);
	}
}

void UInteractableWidget::UpdateProgressColor()
{
	if (!ProgressBar)
	{
		return;
	}

	// Choose color based on depleting state
	FLinearColor TargetColor = bProgressIsDepleting ? ProgressDepletingColor : ProgressFillColor;

	// Set progress bar color
	ProgressBar->SetFillColorAndOpacity(TargetColor);
}