#include "FXRCarPreset.h"

#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

UFXRCarPreset* UFXRCarPreset::LoadOrDefault(UObject* Outer, const TCHAR* PresetPath, const TCHAR* PaintPath)
{
	if (UFXRCarPreset* Preset = LoadObject<UFXRCarPreset>(nullptr, PresetPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		return Preset;
	}
	UFXRCarPreset* Preset = NewObject<UFXRCarPreset>(Outer);
	Preset->BodyMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Fab/ToyRacingCar/SM_ToyCar_Body.SM_ToyCar_Body"));
	Preset->WheelsMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Fab/ToyRacingCar/SM_ToyCar_Wheels.SM_ToyCar_Wheels"));
	Preset->Paint = LoadObject<UMaterialInterface>(nullptr, PaintPath);
	return Preset;
}
