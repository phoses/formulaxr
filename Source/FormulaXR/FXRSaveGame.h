#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "FXRSaveGame.generated.h"

/** Local records and settings (docs/plan.md §5.2), in Saved/SaveGames/FormulaXR.sav. */
UCLASS()
class FORMULAXR_API UFXRSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** Best total race time (seconds) for each race length in laps. */
	UPROPERTY()
	TMap<int32, float> BestRaceTimes;

	/** Best single lap per lane (seconds, 0 = none), from races and practice. */
	UPROPERTY()
	float BestLap[2];   // UObjects are zero-initialised

	/** The player's track height adjustment (right stick), cm. */
	UPROPERTY()
	float TrackHeightOffset = 0.0f;

	/** Loads the save, or a fresh one if there is none. Never null. */
	static UFXRSaveGame* Load();

	/** Loads the latest save, applies Change, and writes it back (so writers never undo each other). */
	static void Update(TFunctionRef<void(UFXRSaveGame&)> Change);
};
