#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "FXRPlayerController.generated.h"

class AFXRSlotCar;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * Controller input (docs/plan.md §4.5). Mapping contexts are built in code at startup, so there
 * are no binary input assets. Left trigger drives car 1, right trigger car 2; right stick moves
 * the track up and down; right stick press re-centers it in mixed reality.
 *
 * -FXRAutoThrottle=<car1>,<car2> holds both triggers at fixed positions (automated checks).
 */
UCLASS()
class FORMULAXR_API AFXRPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void PlayerTick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	UPROPERTY()
	TObjectPtr<UInputMappingContext> CommonContext;

	UPROPERTY()
	TObjectPtr<UInputMappingContext> MixedRealityContext;

	UPROPERTY()
	TObjectPtr<UInputAction> ThrottleActions[2];

	UPROPERTY()
	TObjectPtr<UInputAction> TrackHeightAction;

	UPROPERTY()
	TObjectPtr<UInputAction> RecenterAction;

	UPROPERTY()
	TObjectPtr<UInputAction> RestartAction;

	UPROPERTY()
	TObjectPtr<UInputAction> ToggleModeAction;

	TWeakObjectPtr<AFXRSlotCar> Cars[2];
	bool bAutoThrottle = false;
	float AutoThrottle[2] = { 0.0f, 0.0f };

	void CreateInputAssets();
	AFXRSlotCar* FindCar(int32 Lane);
	void OnTrackHeight(const FInputActionValue& Value);
	void OnRecenter(const FInputActionValue& Value);
	void OnRestart(const FInputActionValue& Value);
	void OnToggleMode(const FInputActionValue& Value);
	void OnQuit();
};
