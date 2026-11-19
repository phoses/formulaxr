#include "FXRFigure8Path.h"

FFXRFigure8Path::FFXRFigure8Path(const FParams& InParams)
	: Params(InParams)
{
	const double R = Params.LoopRadius;
	const double D = Params.LoopCentreDistance;
	const double Offset = Params.LaneOffset;
	check(D > R && R > FMath::Abs(Offset));

	// The straights pass through the origin tangent to both loops: sin(angle) = R / D.
	StraightAngle = FMath::Asin(R / D);
	const double HalfStraight = D * FMath::Cos(StraightAngle);
	const double Sweep = UE_DOUBLE_PI + 2.0 * StraightAngle;   // each loop turns through this
	const double RightRadius = R + Offset;   // clockwise loop: left of travel is the outside
	const double LeftRadius = R - Offset;    // counter-clockwise loop: left of travel is the inside

	const struct { double Length; double Curvature; } Definitions[NumSegments] = {
		{ HalfStraight, 0.0 },
		{ RightRadius * Sweep, -1.0 / RightRadius },
		{ 2.0 * HalfStraight, 0.0 },
		{ LeftRadius * Sweep, 1.0 / LeftRadius },
		{ HalfStraight, 0.0 },
	};

	const FVector2D StartTangent(FMath::Cos(StraightAngle), FMath::Sin(StraightAngle));
	FVector2D Position = FVector2D(-StartTangent.Y, StartTangent.X) * Offset;
	double Heading = StraightAngle;
	double Distance = 0.0;
	for (const auto& Definition : Definitions)
	{
		FSegment Segment;
		Segment.Start = Position;
		Segment.StartHeading = Heading;
		Segment.Length = Definition.Length;
		Segment.Curvature = Definition.Curvature;
		Segment.StartDistance = Distance;
		Segments.Add(Segment);

		Position = Evaluate(Segment, Segment.Length).Location;
		Heading += Segment.Curvature * Segment.Length;
		Distance += Segment.Length;
	}
	Length = Distance;
}

FFXRPathSample FFXRFigure8Path::Evaluate(const FSegment& Segment, double LocalDistance)
{
	FFXRPathSample Result;
	const double Heading = Segment.StartHeading + Segment.Curvature * LocalDistance;
	Result.Tangent = FVector2D(FMath::Cos(Heading), FMath::Sin(Heading));
	Result.Curvature = Segment.Curvature;
	if (FMath::Abs(Segment.Curvature) < 1e-12)
	{
		Result.Location = Segment.Start + FVector2D(FMath::Cos(Segment.StartHeading), FMath::Sin(Segment.StartHeading)) * LocalDistance;
	}
	else
	{
		// Integral of (cos h, sin h) for a heading that changes linearly with distance.
		const double K = Segment.Curvature;
		Result.Location = Segment.Start + FVector2D(
			(FMath::Sin(Heading) - FMath::Sin(Segment.StartHeading)) / K,
			(FMath::Cos(Segment.StartHeading) - FMath::Cos(Heading)) / K);
	}
	return Result;
}

double FFXRFigure8Path::Wrap(double Distance) const
{
	double Wrapped = FMath::Fmod(Distance, Length);
	if (Wrapped < 0.0)
	{
		Wrapped += Length;
	}
	return Wrapped;
}

FFXRPathSample FFXRFigure8Path::Sample(double Distance) const
{
	const double S = Wrap(Distance);
	for (int32 Index = NumSegments - 1; Index >= 0; --Index)
	{
		const FSegment& Segment = Segments[Index];
		if (S >= Segment.StartDistance)
		{
			return Evaluate(Segment, FMath::Min(S - Segment.StartDistance, Segment.Length));
		}
	}
	return Evaluate(Segments[0], 0.0);
}

void FFXRFigure8Path::GetSegmentRange(int32 Segment, double& OutStart, double& OutEnd) const
{
	check(Segments.IsValidIndex(Segment));
	OutStart = Segments[Segment].StartDistance;
	OutEnd = OutStart + Segments[Segment].Length;
}

bool FFXRFigure8Path::IsOnLoop(double Distance) const
{
	const double S = Wrap(Distance);
	for (int32 Index = NumSegments - 1; Index >= 0; --Index)
	{
		if (S >= Segments[Index].StartDistance)
		{
			return Segments[Index].Curvature != 0.0;
		}
	}
	return false;
}

TArray<FFXRPathCrossing> FFXRFigure8Path::FindCrossings(const FFXRFigure8Path& A, const FFXRFigure8Path& B)
{
	auto Cross = [](const FVector2D& U, const FVector2D& V) { return U.X * V.Y - U.Y * V.X; };
	TArray<FFXRPathCrossing> Result;
	for (const FSegment& SegA : A.Segments)
	{
		if (SegA.Curvature != 0.0)
		{
			continue;
		}
		const FVector2D DirA = FVector2D(FMath::Cos(SegA.StartHeading), FMath::Sin(SegA.StartHeading)) * SegA.Length;
		for (const FSegment& SegB : B.Segments)
		{
			if (SegB.Curvature != 0.0)
			{
				continue;
			}
			const FVector2D DirB = FVector2D(FMath::Cos(SegB.StartHeading), FMath::Sin(SegB.StartHeading)) * SegB.Length;
			const double Denominator = Cross(DirA, DirB);
			if (FMath::Abs(Denominator) < 1e-9)
			{
				continue;   // parallel: the same straight of two lanes never meets
			}
			const FVector2D Between = SegB.Start - SegA.Start;
			const double T = Cross(Between, DirB) / Denominator;
			const double U = Cross(Between, DirA) / Denominator;
			if (T < 0.0 || T > 1.0 || U < 0.0 || U > 1.0)
			{
				continue;
			}
			FFXRPathCrossing Crossing;
			Crossing.DistanceA = A.Wrap(SegA.StartDistance + T * SegA.Length);
			Crossing.DistanceB = B.Wrap(SegB.StartDistance + U * SegB.Length);
			Crossing.Location = SegA.Start + DirA * T;
			// Straight A is split at distance 0, so a crossing exactly there is found twice.
			const bool bDuplicate = Result.ContainsByPredicate([&](const FFXRPathCrossing& Other)
			{
				return FVector2D::Distance(Other.Location, Crossing.Location) < 0.01;
			});
			if (!bDuplicate)
			{
				Result.Add(Crossing);
			}
		}
	}
	return Result;
}
