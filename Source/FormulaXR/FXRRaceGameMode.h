#pragma once

#include "CoreMinimal.h"
#include "FXRRace.h"
#include "GameFramework/GameModeBase.h"
#include "FXRRaceGameMode.generated.h"

class AFXRSlotCar;
class AFXRTrack;
class USoundBase;

/**
 * Builds the scene at startup (lights, PC-only room, the track, two cars) and runs the race
 * (docs/plan.md §4.3): start lights, laps, crossover crashes, finish, saved records.
 *
 * Command line (automated checks):
 *   -FXRShot=<file.png> [-FXRShotDelay=<s>]   one screenshot, then quit
 *   -FXRLaps=<n>                               race length (default RaceLaps)
 *   -FXRLightsOut=<s>                          fixed hold after the fifth light (default random)
 *   -FXRPractice                               start in practice mode
 */
UCLASS()
class FORMULAXR_API AFXRRaceGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AFXRRaceGameMode();
	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Laps per car in a race. */
	UPROPERTY(EditAnywhere, Category = "Race")
	int32 RaceLaps = 10;

	const FFXRRace& GetRace() const { return Race; }
	AFXRSlotCar* GetCar(int32 Lane) const;

	/** Back to the grid and start again (Menu button). */
	void RestartRace();
	/** Race <-> Practice, then restart (View button). */
	void ToggleMode();

	/** The saved record for this race length before the current race (0 = none). */
	float GetRecordBefore() const { return RecordBefore; }
	bool IsNewRecord() const { return bNewRecord; }
	/** Best lap per lane across all sessions, including this one (0 = none). */
	float GetBestLapEver(int32 Lane) const;

private:
	FFXRRace Race;
	TArray<TWeakObjectPtr<AFXRSlotCar>> Cars;
	TWeakObjectPtr<AFXRTrack> Track;
	EFXRRaceMode NextMode = EFXRRaceMode::Race;

	float RecordBefore = 0.0f;
	bool bNewRecord = false;
	float SavedBestLap[2] = { 0.0f, 0.0f };

	FString ShotPath;
	FTimerHandle ShotTimer;

	/** Race sounds (2D): a beep per start light, a higher one at lights out, a chime at the finish. */
	UPROPERTY()
	TObjectPtr<USoundBase> LightBeep;
	UPROPERTY()
	TObjectPtr<USoundBase> GoBeep;
	UPROPERTY()
	TObjectPtr<USoundBase> FinishChime;
	int32 LastLightsLit = 0;

	void PlayRaceSound(USoundBase* Sound) const;

	/** -FXRPerfLog: averages of frame, game, render and GPU time, logged every 2 s. */
	bool bPerfLog = false;
	struct FPerfWindow { double Frame = 0, Game = 0, Render = 0, Gpu = 0, MaxGpu = 0; int32 Frames = 0; double Elapsed = 0; } Perf;
	void UpdatePerfLog(float DeltaSeconds);

	void SpawnStage();
	void StartSession();
	void HandleLineCrossed(AFXRSlotCar* Car);
	void HandleDeslot(AFXRSlotCar* Car);
	void CheckCrossover();
	void OnRaceFinished();
	void TakeShot();
};
