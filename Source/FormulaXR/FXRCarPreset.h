#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FXRCarPreset.generated.h"

class UMaterialInterface;
class UStaticMesh;

/**
 * What a car looks like and how its model sits on the slot (docs/plan.md §6.3).
 * DA_Car1_Red and DA_Car2_Blue (Scripts/create_car_presets.py) use the Fab toy car; any other
 * car mesh works by changing these fields. The collision box comes from the mesh bounds.
 */
UCLASS(BlueprintType)
class FORMULAXR_API UFXRCarPreset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Car")
	TObjectPtr<UStaticMesh> BodyMesh;

	/** All four wheels in one mesh; split into a front and a rear pair at runtime so each axle spins. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Car")
	TObjectPtr<UStaticMesh> WheelsMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Car")
	TObjectPtr<UMaterialInterface> Paint;

	/** Uniform scale to slot size. Toy car: 106.6 cm wide authored, 6.5 cm at 0.061. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Car")
	float Scale = 0.061f;

	/** Turns the mesh so its nose points along +X. The toy car's nose points along +Y as imported. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Car")
	float YawCorrection = -90.0f;

	/** Guide pin distance ahead of the car centre, cm at slot scale (just ahead of the front axle). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Car")
	float GuideAhead = 3.6f;

	/** The asset at PresetPath, or a transient preset of the toy car with PaintPath if it doesn't exist. */
	static UFXRCarPreset* LoadOrDefault(UObject* Outer, const TCHAR* PresetPath, const TCHAR* PaintPath);
};
