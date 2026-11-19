#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FXRHudWidget.generated.h"

class UBorder;
class UImage;
class UTextBlock;
class UVerticalBox;

/**
 * Race HUD (docs/plan.md §4.4), built entirely in C++ (no Blueprint widget asset).
 * Screen-space overlay in PC mode; the same widget on the track's lap tower in mixed reality.
 * Reads the race from AFXRRaceGameMode every frame.
 */
UCLASS()
class FORMULAXR_API UFXRHudWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY()
	TObjectPtr<UTextBlock> Status;

	UPROPERTY()
	TObjectPtr<UTextBlock> Clock;

	UPROPERTY()
	TObjectPtr<UImage> Lights[5];

	UPROPERTY()
	TObjectPtr<UTextBlock> CarTitle[2];

	UPROPERTY()
	TObjectPtr<UTextBlock> CarLap[2];

	UPROPERTY()
	TObjectPtr<UTextBlock> CarTimes[2];

	UPROPERTY()
	TObjectPtr<UTextBlock> CarState[2];

	UPROPERTY()
	TObjectPtr<UBorder> ResultsPanel;

	UPROPERTY()
	TObjectPtr<UTextBlock> Results;

	UPROPERTY()
	TObjectPtr<UTextBlock> Hint;

	/** "Connect a controller" when no gamepad is attached (the game needs one). */
	UPROPERTY()
	TObjectPtr<UBorder> ControllerPrompt;

	void BuildTree();
	UTextBlock* MakeText(int32 Size, const FLinearColor& Color, bool bBold = false);
	UBorder* MakePanel(UVerticalBox*& OutContent);
};
