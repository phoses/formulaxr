#include "../FXRFigure8Path.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	FFXRFigure8Path MakeLane(double Offset, double R = 45.0, double D = 72.0)
	{
		FFXRFigure8Path::FParams Params;
		Params.LoopRadius = R;
		Params.LoopCentreDistance = D;
		Params.LaneOffset = Offset;
		return FFXRFigure8Path(Params);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFXRFigure8LanesEqualLength, "FormulaXR.Track.LanesEqualLength", TestFlags)
bool FFXRFigure8LanesEqualLength::RunTest(const FString& Parameters)
{
	// Default track plus a smaller and a wider variant: equal length must not depend on the numbers.
	for (const FVector3d& Case : { FVector3d(45.0, 72.0, 3.9), FVector3d(30.0, 40.0, 5.0), FVector3d(60.0, 150.0, 3.9) })
	{
		const FFXRFigure8Path Centre = MakeLane(0.0, Case.X, Case.Y);
		const FFXRFigure8Path Lane0 = MakeLane(-Case.Z, Case.X, Case.Y);
		const FFXRFigure8Path Lane1 = MakeLane(Case.Z, Case.X, Case.Y);
		TestNearlyEqual(TEXT("Lane 0 length = centreline length"), Lane0.GetLength(), Centre.GetLength(), 1e-6);
		TestNearlyEqual(TEXT("Lane 1 length = centreline length"), Lane1.GetLength(), Centre.GetLength(), 1e-6);
	}
	// Closed-form lap: 4 D cos(a) + 2 R (pi + 2a), sin(a) = R / D.
	const double A = FMath::Asin(45.0 / 72.0);
	TestNearlyEqual(TEXT("Lap matches closed form"), MakeLane(0.0).GetLength(), 4.0 * 72.0 * FMath::Cos(A) + 2.0 * 45.0 * (UE_DOUBLE_PI + 2.0 * A), 1e-6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFXRFigure8Closes, "FormulaXR.Track.PathCloses", TestFlags)
bool FFXRFigure8Closes::RunTest(const FString& Parameters)
{
	for (const double Offset : { -3.9, 0.0, 3.9 })
	{
		const FFXRFigure8Path Path = MakeLane(Offset);
		const FFXRPathSample Start = Path.Sample(0.0);
		const FFXRPathSample End = Path.Sample(Path.GetLength() - 1e-9);
		TestTrue(FString::Printf(TEXT("Offset %.1f: end meets start"), Offset), FVector2D::Distance(Start.Location, End.Location) < 1e-4);
		TestTrue(FString::Printf(TEXT("Offset %.1f: end heading matches start"), Offset), FVector2D::DotProduct(Start.Tangent, End.Tangent) > 1.0 - 1e-9);

		// No jumps anywhere: consecutive 1 cm samples are about 1 cm apart.
		double MaxStep = 0.0;
		for (double S = 0.0; S < Path.GetLength(); S += 1.0)
		{
			MaxStep = FMath::Max(MaxStep, FVector2D::Distance(Path.Sample(S).Location, Path.Sample(S + 1.0).Location));
		}
		TestTrue(FString::Printf(TEXT("Offset %.1f: continuous (max 1 cm step %.4f)"), Offset, MaxStep), MaxStep <= 1.0 + 1e-6);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFXRFigure8Curvature, "FormulaXR.Track.Curvature", TestFlags)
bool FFXRFigure8Curvature::RunTest(const FString& Parameters)
{
	const double Offset = 3.9;
	const FFXRFigure8Path Lane = MakeLane(Offset);
	double S0, S1;
	Lane.GetSegmentRange(1, S0, S1);
	TestNearlyEqual(TEXT("Right loop: clockwise, outside radius"), Lane.Sample(0.5 * (S0 + S1)).Curvature, -1.0 / (45.0 + Offset), 1e-9);
	Lane.GetSegmentRange(3, S0, S1);
	TestNearlyEqual(TEXT("Left loop: counter-clockwise, inside radius"), Lane.Sample(0.5 * (S0 + S1)).Curvature, 1.0 / (45.0 - Offset), 1e-9);
	Lane.GetSegmentRange(2, S0, S1);
	TestEqual(TEXT("Straight B is straight"), Lane.Sample(0.5 * (S0 + S1)).Curvature, 0.0);
	TestFalse(TEXT("Distance 0 is on a straight"), Lane.IsOnLoop(0.0));

	// The loop really is a circle of that radius around (+-D, 0).
	Lane.GetSegmentRange(1, S0, S1);
	for (double S = S0; S < S1; S += 5.0)
	{
		if (!TestNearlyEqual(TEXT("Right loop radius"), FVector2D::Distance(Lane.Sample(S).Location, FVector2D(72.0, 0.0)), 45.0 + Offset, 1e-6))
		{
			break;
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFXRFigure8Crossings, "FormulaXR.Track.LaneCrossings", TestFlags)
bool FFXRFigure8Crossings::RunTest(const FString& Parameters)
{
	const FFXRFigure8Path Lane0 = MakeLane(-3.9);
	const FFXRFigure8Path Lane1 = MakeLane(3.9);
	const TArray<FFXRPathCrossing> Crossings = FFXRFigure8Path::FindCrossings(Lane0, Lane1);
	TestEqual(TEXT("Lanes 0 and 1 cross twice"), Crossings.Num(), 2);
	for (const FFXRPathCrossing& Crossing : Crossings)
	{
		TestTrue(TEXT("Crossing is in the crossover"), Crossing.Location.Size() < 10.0);
		TestTrue(TEXT("Crossing point is on lane 0"), FVector2D::Distance(Lane0.Sample(Crossing.DistanceA).Location, Crossing.Location) < 1e-6);
		TestTrue(TEXT("Crossing point is on lane 1"), FVector2D::Distance(Lane1.Sample(Crossing.DistanceB).Location, Crossing.Location) < 1e-6);
	}
	// A lane also crosses itself once (straight A over straight B).
	TestEqual(TEXT("Lane crosses itself once"), FFXRFigure8Path::FindCrossings(Lane0, Lane0).Num(), 1);
	return true;
}

#endif
