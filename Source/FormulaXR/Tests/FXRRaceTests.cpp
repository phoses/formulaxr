#include "../FXRRace.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	/** A race already at lights out. */
	FFXRRace StartedRace(int32 Laps)
	{
		FFXRRace Race;
		Race.Reset(EFXRRaceMode::Race, Laps, 1.0);
		Race.Tick(FFXRRace::PreRaceTime);
		while (Race.Phase != EFXRRacePhase::Racing)
		{
			Race.Tick(0.01);
		}
		return Race;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFXRRaceStartLights, "FormulaXR.Race.StartLights", TestFlags)
bool FFXRRaceStartLights::RunTest(const FString& Parameters)
{
	FFXRRace Race;
	Race.Reset(EFXRRaceMode::Race, 10, 0.8);
	TestTrue(TEXT("Starts in pre-race"), Race.Phase == EFXRRacePhase::PreRace);
	TestFalse(TEXT("No driving before the start"), Race.CanDrive());
	TestTrue(TEXT("Pre-race -> countdown"), Race.Tick(FFXRRace::PreRaceTime) && Race.Phase == EFXRRacePhase::Countdown);

	TestEqual(TEXT("First light at once"), Race.LightsLit(), 1);
	Race.Tick(2.0);
	TestEqual(TEXT("Third light at 2 s"), Race.LightsLit(), 3);
	Race.Tick(2.0);
	TestEqual(TEXT("All five at 4 s"), Race.LightsLit(), 5);
	TestFalse(TEXT("Still no driving with lights on"), Race.CanDrive());
	Race.Tick(0.7);
	TestTrue(TEXT("Lights held for the delay"), Race.Phase == EFXRRacePhase::Countdown);
	TestTrue(TEXT("Lights out after the delay"), Race.Tick(0.2) && Race.Phase == EFXRRacePhase::Racing);
	TestEqual(TEXT("Lights off"), Race.LightsLit(), 0);
	TestTrue(TEXT("Driving after lights out"), Race.CanDrive());
	TestEqual(TEXT("Clock starts at zero"), Race.Clock, 0.0);
	Race.Tick(1.5);
	TestNearlyEqual(TEXT("Clock runs"), Race.Clock, 1.5, 1e-9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFXRRaceLapsAndFinish, "FormulaXR.Race.LapsAndFinish", TestFlags)
bool FFXRRaceLapsAndFinish::RunTest(const FString& Parameters)
{
	FFXRRace Race = StartedRace(3);

	// The grid is behind the line: the first crossing only starts lap 1.
	Race.Tick(0.3);
	TestFalse(TEXT("First crossing is not a lap"), Race.OnLineCrossed(0));
	TestEqual(TEXT("No laps yet"), Race.Cars[0].LapsCompleted, 0);

	Race.Tick(4.0);
	TestTrue(TEXT("Second crossing completes lap 1"), Race.OnLineCrossed(0));
	TestNearlyEqual(TEXT("Lap 1 time"), Race.Cars[0].LastLapTime, 4.0, 1e-9);
	Race.Tick(3.5);
	Race.OnLineCrossed(0);
	TestNearlyEqual(TEXT("Best lap"), Race.Cars[0].BestLapTime, 3.5, 1e-9);
	Race.Tick(3.8);
	Race.OnLineCrossed(0);
	TestTrue(TEXT("Car 1 finished after 3 laps"), Race.Cars[0].bFinished);
	TestNearlyEqual(TEXT("Car 1 finish time"), Race.Cars[0].FinishTime, 0.3 + 4.0 + 3.5 + 3.8, 1e-9);
	TestTrue(TEXT("Race goes on until both finish"), Race.Phase == EFXRRacePhase::Racing);
	TestFalse(TEXT("Cool-down laps don't count"), Race.OnLineCrossed(0));
	TestEqual(TEXT("Still 3 laps"), Race.Cars[0].LapsCompleted, 3);

	// Car 2 with a de-slot.
	Race.OnLineCrossed(1);
	Race.OnDeslot(1);
	for (int32 Lap = 0; Lap < 3; ++Lap)
	{
		Race.Tick(5.0);
		Race.OnLineCrossed(1);
	}
	TestTrue(TEXT("Both finished -> race finished"), Race.Phase == EFXRRacePhase::Finished);
	TestEqual(TEXT("De-slot counted"), Race.Cars[1].Deslots, 1);
	TestNearlyEqual(TEXT("Total = last car's finish"), Race.RaceTime(), Race.Cars[1].FinishTime, 1e-9);
	const double Total = Race.RaceTime();
	Race.Tick(10.0);
	TestNearlyEqual(TEXT("Clock stops at the finish"), Race.RaceTime(), Total, 1e-9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFXRRacePractice, "FormulaXR.Race.Practice", TestFlags)
bool FFXRRacePractice::RunTest(const FString& Parameters)
{
	FFXRRace Race;
	Race.Reset(EFXRRaceMode::Practice, 3, 1.0);
	TestTrue(TEXT("Practice starts driving at once"), Race.CanDrive());
	Race.OnLineCrossed(0);
	for (int32 Lap = 0; Lap < 6; ++Lap)
	{
		Race.Tick(4.0);
		Race.OnLineCrossed(0);
	}
	TestEqual(TEXT("Practice counts laps past the race length"), Race.Cars[0].LapsCompleted, 6);
	TestFalse(TEXT("Practice never finishes"), Race.Cars[0].bFinished);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFXRRaceNoCheating, "FormulaXR.Race.NoLapsBeforeStart", TestFlags)
bool FFXRRaceNoCheating::RunTest(const FString& Parameters)
{
	FFXRRace Race;
	Race.Reset(EFXRRaceMode::Race, 3, 1.0);
	Race.Tick(FFXRRace::PreRaceTime + 1.0);
	TestFalse(TEXT("Crossing during the countdown is ignored"), Race.OnLineCrossed(0));
	Race.OnDeslot(0);
	TestEqual(TEXT("No crossings recorded"), Race.Cars[0].LineCrossings, 0);
	TestEqual(TEXT("No de-slots recorded"), Race.Cars[0].Deslots, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFXRRaceCoversPoint, "FormulaXR.Race.CoversPoint", TestFlags)
bool FFXRRaceCoversPoint::RunTest(const FString& Parameters)
{
	const double Lap = 629.0, Behind = 12.0, Ahead = 5.0;
	TestTrue(TEXT("Point just ahead of the pin"), FFXRRace::CoversPoint(Lap, 100.0, Behind, Ahead, 104.0));
	TestTrue(TEXT("Point under the tail"), FFXRRace::CoversPoint(Lap, 100.0, Behind, Ahead, 89.0));
	TestFalse(TEXT("Point beyond the nose"), FFXRRace::CoversPoint(Lap, 100.0, Behind, Ahead, 106.0));
	TestFalse(TEXT("Point behind the tail"), FFXRRace::CoversPoint(Lap, 100.0, Behind, Ahead, 87.0));
	// Across the lap boundary both ways.
	TestTrue(TEXT("Car at lap start, point at lap end"), FFXRRace::CoversPoint(Lap, 3.0, Behind, Ahead, 625.0));
	TestTrue(TEXT("Car at lap end, point at lap start"), FFXRRace::CoversPoint(Lap, 627.0, Behind, Ahead, 1.0));
	TestFalse(TEXT("Far side of the track"), FFXRRace::CoversPoint(Lap, 0.0, Behind, Ahead, 314.0));
	return true;
}

#endif
