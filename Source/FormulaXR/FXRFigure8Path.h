#pragma once

#include "CoreMinimal.h"

/** A point on a path: position and unit tangent in the track plane (cm), signed curvature (1/cm, + = left turn). */
struct FFXRPathSample
{
	FVector2D Location = FVector2D::ZeroVector;
	FVector2D Tangent = FVector2D(1.0, 0.0);
	double Curvature = 0.0;

	/** Unit normal to the left of the direction of travel. */
	FVector2D LeftNormal() const { return FVector2D(-Tangent.Y, Tangent.X); }
};

/** Where two paths cross: distance along each, and the crossing point. */
struct FFXRPathCrossing
{
	double DistanceA = 0.0;
	double DistanceB = 0.0;
	FVector2D Location = FVector2D::ZeroVector;
};

/**
 * Analytic figure-8 path (docs/plan.md §4.1), exact rather than spline-approximated.
 *
 * Centreline: two loops of radius R centred at (+-D, 0), joined by the two tangent straights
 * that cross at the origin. Distance 0 is the origin, heading up and to the right along
 * straight A. The right loop is driven clockwise, the left loop counter-clockwise.
 *
 * A lane is the centreline offset by LaneOffset to the left of travel. On the clockwise loop
 * the left side is the outside (radius R + offset), on the other loop it is the inside
 * (R - offset), so every lane has exactly the same length.
 *
 * Segments: 0 half of straight A, 1 right loop, 2 straight B, 3 left loop, 4 other half of straight A.
 */
class FORMULAXR_API FFXRFigure8Path
{
public:
	struct FParams
	{
		double LoopRadius = 45.0;
		double LoopCentreDistance = 72.0;
		double LaneOffset = 0.0;
	};

	FFXRFigure8Path() : FFXRFigure8Path(FParams()) {}
	explicit FFXRFigure8Path(const FParams& InParams);

	double GetLength() const { return Length; }
	const FParams& GetParams() const { return Params; }

	/** Distance wraps around the lap, so any value is valid. */
	FFXRPathSample Sample(double Distance) const;

	/** Wraps a distance into [0, GetLength()). */
	double Wrap(double Distance) const;

	/** Distance range [Start, End) of a segment (see class comment for the order). */
	void GetSegmentRange(int32 Segment, double& OutStart, double& OutEnd) const;

	/** Is this distance on one of the two loops (curved), rather than a straight? */
	bool IsOnLoop(double Distance) const;

	/** Half-angle between the straights and the X axis, radians. */
	double GetStraightAngle() const { return StraightAngle; }

	/** All points where a straight of A crosses a straight of B. */
	static TArray<FFXRPathCrossing> FindCrossings(const FFXRFigure8Path& A, const FFXRFigure8Path& B);

	static constexpr int32 NumSegments = 5;

private:
	struct FSegment
	{
		FVector2D Start;
		double StartHeading = 0.0;
		double Length = 0.0;
		double Curvature = 0.0;
		double StartDistance = 0.0;
	};

	static FFXRPathSample Evaluate(const FSegment& Segment, double LocalDistance);

	FParams Params;
	double StraightAngle = 0.0;
	double Length = 0.0;
	TArray<FSegment, TFixedAllocator<NumSegments>> Segments;
};
