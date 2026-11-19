#include "FXRRace.h"

void FFXRRace::Reset(EFXRRaceMode InMode, int32 InTargetLaps, double InLightsOutDelay)
{
	Mode = InMode;
	TargetLaps = FMath::Max(1, InTargetLaps);
	LightsOutDelay = FMath::Max(0.0, InLightsOutDelay);
	Clock = 0.0;
	PhaseTime = 0.0;
	for (FFXRCarRaceState& Car : Cars)
	{
		Car = FFXRCarRaceState();
	}
	// Practice skips the start procedure.
	Phase = Mode == EFXRRaceMode::Practice ? EFXRRacePhase::Racing : EFXRRacePhase::PreRace;
}

bool FFXRRace::Tick(double DeltaTime)
{
	PhaseTime += DeltaTime;
	switch (Phase)
	{
	case EFXRRacePhase::PreRace:
		if (PhaseTime >= PreRaceTime)
		{
			Phase = EFXRRacePhase::Countdown;
			PhaseTime = 0.0;
			return true;
		}
		break;
	case EFXRRacePhase::Countdown:
		// Lights come on at 0, 1, 2, 3, 4 s; all five are held for LightsOutDelay.
		if (PhaseTime >= (NumLights - 1) * LightInterval + LightsOutDelay)
		{
			Phase = EFXRRacePhase::Racing;
			PhaseTime = 0.0;
			Clock = 0.0;
			return true;
		}
		break;
	case EFXRRacePhase::Racing:
		Clock += DeltaTime;
		break;
	case EFXRRacePhase::Finished:
		break;
	}
	return false;
}

int32 FFXRRace::LightsLit() const
{
	if (Phase != EFXRRacePhase::Countdown)
	{
		return 0;
	}
	return FMath::Min(NumLights, FMath::FloorToInt32(PhaseTime / LightInterval) + 1);
}

bool FFXRRace::OnLineCrossed(int32 CarIndex)
{
	if (!CanDrive() || CarIndex < 0 || CarIndex >= NumCars)
	{
		return false;
	}
	FFXRCarRaceState& Car = Cars[CarIndex];
	if (Car.bFinished)
	{
		return false;   // cool-down laps after the flag don't count
	}
	++Car.LineCrossings;
	if (Car.LineCrossings == 1)
	{
		Car.LapStartTime = Clock;
		return false;
	}
	const double Lap = Clock - Car.LapStartTime;
	Car.LastLapTime = Lap;
	Car.BestLapTime = Car.BestLapTime > 0.0 ? FMath::Min(Car.BestLapTime, Lap) : Lap;
	++Car.LapsCompleted;
	Car.LapStartTime = Clock;

	if (Mode == EFXRRaceMode::Race && Car.LapsCompleted >= TargetLaps)
	{
		Car.bFinished = true;
		Car.FinishTime = Clock;
		bool bAllFinished = true;
		for (const FFXRCarRaceState& Other : Cars)
		{
			bAllFinished &= Other.bFinished;
		}
		if (bAllFinished)
		{
			Phase = EFXRRacePhase::Finished;
			PhaseTime = 0.0;
		}
	}
	return true;
}

void FFXRRace::OnDeslot(int32 CarIndex)
{
	if (CarIndex >= 0 && CarIndex < NumCars && CanDrive() && !Cars[CarIndex].bFinished)
	{
		++Cars[CarIndex].Deslots;
	}
}

double FFXRRace::RaceTime() const
{
	if (Phase == EFXRRacePhase::Finished)
	{
		double Latest = 0.0;
		for (const FFXRCarRaceState& Car : Cars)
		{
			Latest = FMath::Max(Latest, Car.FinishTime);
		}
		return Latest;
	}
	return Clock;
}

bool FFXRRace::CoversPoint(double LapLength, double CarDistance, double Behind, double Ahead, double Point)
{
	// Signed distance from the car to the point, the short way round the lap.
	double Delta = FMath::Fmod(Point - CarDistance, LapLength);
	if (Delta < 0.0)
	{
		Delta += LapLength;
	}
	if (Delta > 0.5 * LapLength)
	{
		Delta -= LapLength;
	}
	return Delta >= -Behind && Delta <= Ahead;
}
