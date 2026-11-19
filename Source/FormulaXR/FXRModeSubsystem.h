#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FXRModeSubsystem.generated.h"

/** Actors and components with this tag exist only in PC mode (room, table, track base board). */
#define FXR_PC_ONLY_TAG TEXT("PCOnly")

/**
 * Which mode the game runs in. launch.bat passes -nohmd (PC), launch-vr.bat passes -vr (mixed reality);
 * the game is in mixed reality exactly when a headset is enabled.
 */
UCLASS()
class FORMULAXR_API UFXRModeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "FormulaXR")
	bool IsMixedReality() const;

	/** Hides everything tagged PCOnly in mixed reality; shows it in PC mode. */
	void ApplyModeVisibility(UWorld* World) const { SetPCOnlyHidden(World, IsMixedReality()); }

	/** Hides or shows everything tagged PCOnly (the -FXRMRPreview mode hides it on a PC). */
	void SetPCOnlyHidden(UWorld* World, bool bHide) const;

	/**
	 * -FXRMRPreview: the mixed reality look on the monitor (no PC room, soft shadows, lap tower) over
	 * a grey stand-in floor, so MR visuals can be checked with screenshots without a headset.
	 */
	static bool IsMixedRealityPreview();
};
