#include "../FXRFigure8Path.h"
#include "../FXRSlotCarModel.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	constexpr float Dt = 1.0f / 240.0f;

	/** Full-throttle top speed on a straight: motor and drag balance. */
	float EquilibriumSpeed(const FFXRSlotCarParams& P)
	{
		return P.TopSpeed * P.MaxAccel / (P.MaxAccel + P.Drag * P.TopSpeed);
	}

	/** Runs at constant throttle and curvature; returns true if it de-slots within Seconds. */
	bool Run(FFXRSlotCarModel& Model, const FFXRSlotCarParams& P, float Throttle, double Curvature, float Seconds)
	{
		for (float T = 0.0f; T < Seconds; T += Dt)
		{
			if (Model.Step(P, Throttle, Curvature, Dt))
			{
				return true;
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFXRCarStraightLine, "FormulaXR.Car.StraightLine", TestFlags)
bool FFXRCarStraightLine::RunTest(const FString& Parameters)
{
	const FFXRSlotCarParams P;
	FFXRSlotCarModel Model;
	float MaxSpeed = 0.0f;
	for (float T = 0.0f; T < 6.0f; T += Dt)
	{
		Model.Step(P, 1.0f, 0.0, Dt);
		MaxSpeed = FMath::Max(MaxSpeed, Model.Speed);
	}
	TestTrue(TEXT("Never above TopSpeed"), MaxSpeed <= P.TopSpeed);
	TestNearlyEqual(TEXT("Settles at full-throttle equilibrium"), Model.Speed, EquilibriumSpeed(P), EquilibriumSpeed(P) * 0.01f);
	TestEqual(TEXT("No slide on a straight"), Model.SlideMeter, 0.0f);

	// Off the trigger: the motor brake stops the car quickly.
	Run(Model, P, 0.0f, 0.0, 2.0f);
	TestTrue(FString::Printf(TEXT("Stops within 2 s off throttle (%.2f cm/s left)"), Model.Speed), Model.Speed < 5.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFXRCarCornerLimit, "FormulaXR.Car.CornerLimit", TestFlags)
bool FFXRCarCornerLimit::RunTest(const FString& Parameters)
{
	const FFXRSlotCarParams P;
	const float V = EquilibriumSpeed(P);

	// Curvature where full throttle sits just under / over the grip limit.
	const double JustUnder = 0.95 * P.GripLimit / (double(V) * V);
	const double JustOver = 1.10 * P.GripLimit / (double(V) * V);

	FFXRSlotCarModel Model;
	Model.Speed = V;
	TestFalse(TEXT("Holds a corner just under the limit for 10 s"), Run(Model, P, 1.0f, JustUnder, 10.0f));
	TestEqual(TEXT("Slide meter stays empty under the limit"), Model.SlideMeter, 0.0f);

	Model = FFXRSlotCarModel();
	Model.Speed = V;
	TestTrue(TEXT("De-slots 10% over the limit"), Run(Model, P, 1.0f, JustOver, 10.0f));

	// 10% over fills the meter slowly, so the player gets seconds of warning.
	Model = FFXRSlotCarModel();
	Model.Speed = V;
	float Time = 0.0f;
	while (!Model.Step(P, 1.0f, JustOver, Dt) && Time < 10.0f)
	{
		Time += Dt;
	}
	TestTrue(FString::Printf(TEXT("Slide warning before de-slot (%.2f s)"), Time), Time > 2.0f);

	// Flat out into the inside loop of the default track (~340 cm/s at 41.1 cm radius):
	// there must be time to react and lift (> 0.3 s), but it must not hold for the whole loop (~0.55 s).
	Model = FFXRSlotCarModel();
	Model.Speed = 340.0f;
	Time = 0.0f;
	while (!Model.Step(P, 1.0f, 1.0 / 41.1, Dt) && Time < 10.0f)
	{
		Time += Dt;
	}
	TestTrue(FString::Printf(TEXT("Flat out: reaction time before de-slot (%.2f s)"), Time), Time > 0.3f);
	TestTrue(FString::Printf(TEXT("Flat out: still de-slots inside one loop (%.2f s)"), Time), Time < 0.55f);

	// Lifting as soon as the slide shows saves it.
	Model = FFXRSlotCarModel();
	Model.Speed = 340.0f;
	Run(Model, P, 1.0f, 1.0 / 41.1, 0.25f);
	TestFalse(TEXT("Lifting after 0.25 s of slide saves the car"), Run(Model, P, 0.0f, 1.0 / 41.1, 2.0f));

	// Much too fast still goes quickly.
	Model = FFXRSlotCarModel();
	Model.Speed = 2.0f * P.CornerSpeedLimit(JustUnder);
	TestTrue(TEXT("Twice the corner speed de-slots within 0.5 s"), Run(Model, P, 1.0f, JustUnder, 0.5f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFXRCarSlideRecovers, "FormulaXR.Car.SlideRecovers", TestFlags)
bool FFXRCarSlideRecovers::RunTest(const FString& Parameters)
{
	const FFXRSlotCarParams P;
	FFXRSlotCarModel Model;
	Model.Speed = 300.0f;
	const double Corner = 1.0 / 41.1;   // inside lane of the default track
	// Brief overshoot, then lift: the meter rises, the car survives, the meter drains.
	Run(Model, P, 1.0f, Corner, 0.15f);
	const float Peak = Model.SlideMeter;
	TestTrue(TEXT("Meter rose"), Peak > 0.0f && Peak < 1.0f);
	TestFalse(TEXT("Lifting saves it"), Run(Model, P, 0.0f, Corner, 2.0f));
	TestEqual(TEXT("Meter drained"), Model.SlideMeter, 0.0f);
	TestTrue(TEXT("Tail slides to the outside: nose points into a left turn"), [&]()
	{
		FFXRSlotCarModel M;
		M.Speed = 300.0f;
		Run(M, P, 1.0f, Corner, 0.1f);
		return M.SlideAngle(P) > 0.0f;
	}());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFXRCarThrottleShape, "FormulaXR.Car.ThrottleShape", TestFlags)
bool FFXRCarThrottleShape::RunTest(const FString& Parameters)
{
	const FFXRSlotCarParams P;
	TestEqual(TEXT("Dead zone"), P.ShapeThrottle(0.02f), 0.0f);
	TestEqual(TEXT("Full"), P.ShapeThrottle(1.0f), 1.0f);
	TestTrue(TEXT("Finer low end"), P.ShapeThrottle(0.5f) < 0.5f);
	TestTrue(TEXT("Monotonic"), P.ShapeThrottle(0.6f) > P.ShapeThrottle(0.5f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFXRCarLapOnTrack, "FormulaXR.Car.LapOnTrack", TestFlags)
bool FFXRCarLapOnTrack::RunTest(const FString& Parameters)
{
	// Default track, both lanes: a careful constant throttle laps forever; flat out de-slots in the first loop.
	const FFXRSlotCarParams P;
	for (const double Offset : { -3.9, 3.9 })
	{
		FFXRFigure8Path::FParams Params;
		Params.LaneOffset = Offset;
		const FFXRFigure8Path Lane(Params);

		FFXRSlotCarModel Careful;
		bool bDeslot = false;
		for (float T = 0.0f; T < 30.0f && !bDeslot; T += 1.0f / 60.0f)
		{
			bDeslot = Careful.Advance(P, Lane, 0.55f, 1.0f / 60.0f);
		}
		TestFalse(FString::Printf(TEXT("Lane offset %.1f: 55%% throttle never de-slots"), Offset), bDeslot);
		TestTrue(FString::Printf(TEXT("Lane offset %.1f: 55%% throttle completes laps (%.1f)"), Offset, Careful.TotalDistance / Lane.GetLength()),
			Careful.TotalDistance > 3.0 * Lane.GetLength());

		FFXRSlotCarModel FlatOut;
		bDeslot = false;
		for (float T = 0.0f; T < 10.0f && !bDeslot; T += 1.0f / 60.0f)
		{
			bDeslot = FlatOut.Advance(P, Lane, 1.0f, 1.0f / 60.0f);
		}
		TestTrue(FString::Printf(TEXT("Lane offset %.1f: flat out de-slots"), Offset), bDeslot);
		TestTrue(FString::Printf(TEXT("Lane offset %.1f: ... within the first lap"), Offset), FlatOut.TotalDistance < Lane.GetLength());
	}
	return true;
}

#endif
