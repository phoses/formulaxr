#pragma once

#include "CoreMinimal.h"

enum class EFXRRaceMode : uint8
{
	/** Both cars must complete TargetLaps; the clock stops when the second car finishes. */
	Race,
	/** No countdown or finish: free running with lap times. */
	Practice,
};

enum class EFXRRacePhase : uint8
{
	PreRace,
	Countdown,
	Racing,
	Finished,
};

/** One car's progress through a race. Times are race-clock seconds. */
struct FFXRCarRaceState
{
	/** Start/finish line crossings since the start; the first one starts lap 1 (the grid is behind the line). */
	int32 LineCrossings = 0;
	int32 LapsCompleted = 0;
	double LapStartTime = 0.0;
	double LastLapTime = 0.0;
	/** 0 until a lap is completed. */
	double BestLapTime = 0.0;
	int32 Deslots = 0;
	bool bFinished = false;
	double FinishTime = 0.0;

	double CurrentLapTime(double Clock) const { return LineCrossings > 0 && !bFinished ? Clock - LapStartTime : 0.0; }
};

/**
 * Race rules (docs/plan.md §4.3), with no engine dependencies so they can be unit tested.
 * PreRace -> Countdown (a light every second, five lights, then out after LightsOutDelay) -> Racing -> Finished.
 */
struct FORMULAXR_API FFXRRace
{
	static constexpr int32 NumCars = 2;
	static constexpr int32 NumLights = 5;
	static constexpr double LightInterval = 1.0;
	static constexpr double PreRaceTime = 1.5;

	EFXRRaceMode Mode = EFXRRaceMode::Race;
	EFXRRacePhase Phase = EFXRRacePhase::PreRace;
	int32 TargetLaps = 10;
	/** Seconds since lights out (Practice: since the start). Stops when the race finishes. */
	double Clock = 0.0;
	/** Seconds in the current phase. */
	double PhaseTime = 0.0;
	/** Hold after the fifth light before lights out (randomised by the game so it can't be anticipated). */
	double LightsOutDelay = 1.0;
	FFXRCarRaceState Cars[NumCars];

	void Reset(EFXRRaceMode InMode, int32 InTargetLaps, double InLightsOutDelay);

	/** Advances time. Returns true when the phase changed. */
	bool Tick(double DeltaTime);

	/** Lights showing red: 0..5 during the countdown, 0 otherwise. */
	int32 LightsLit() const;

	/** Cars respond to their triggers only after lights out. */
	bool CanDrive() const { return Phase == EFXRRacePhase::Racing || Phase == EFXRRacePhase::Finished; }

	/** A car crossed the start/finish line. Returns true if that completed a lap. */
	bool OnLineCrossed(int32 Car);

	void OnDeslot(int32 Car);

	/** Total time: the last car's finish time once finished, the running clock before. */
	double RaceTime() const;

	/**
	 * Does a car whose guide pin is at CarDistance, with its body from Behind to Ahead of the pin,
	 * cover the point at Point on a lap of LapLength? (Used for crossover crashes.)
	 */
	static bool CoversPoint(double LapLength, double CarDistance, double Behind, double Ahead, double Point);
};
