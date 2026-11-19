#include "FXRSlotCarModel.h"

#include "FXRFigure8Path.h"

float FFXRSlotCarParams::ShapeThrottle(float Trigger) const
{
	const float Normalised = FMath::Clamp((Trigger - TriggerDeadZone) / FMath::Max(1.0f - TriggerDeadZone, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
	return FMath::Pow(Normalised, ThrottleExponent);
}

float FFXRSlotCarParams::CornerSpeedLimit(double Curvature) const
{
	const double K = FMath::Abs(Curvature);
	return K < 1e-9 ? TNumericLimits<float>::Max() : float(FMath::Sqrt(GripLimit / K));
}

void FFXRSlotCarModel::Reset(double InDistance)
{
	Distance = InDistance;
	TotalDistance = InDistance;
	Speed = 0.0f;
	SlideMeter = 0.0f;
	Curvature = 0.0;
	LateralAccel = 0.0f;
	TimeAccumulator = 0.0f;
}

bool FFXRSlotCarModel::Step(const FFXRSlotCarParams& Params, float Throttle, double InCurvature, float Dt)
{
	Throttle = FMath::Clamp(Throttle, 0.0f, 1.0f);
	const float Motor = Throttle * Params.MaxAccel * (1.0f - Speed / Params.TopSpeed);
	const float Brake = (1.0f - Throttle) * Params.MotorBrake * Speed;
	const float Accel = Motor - Brake - Params.Drag * Speed;
	Speed = FMath::Max(0.0f, Speed + Accel * Dt);

	Curvature = InCurvature;
	LateralAccel = float(Speed * Speed * FMath::Abs(InCurvature));
	const float Overshoot = LateralAccel / Params.GripLimit - 1.0f;
	SlideMeter = Overshoot > 0.0f
		? SlideMeter + Overshoot * Params.SlideFillRate * Dt
		: FMath::Max(0.0f, SlideMeter - Params.SlideDrainRate * Dt);
	if (SlideMeter >= 1.0f)
	{
		SlideMeter = 1.0f;
		return true;
	}
	return false;
}

bool FFXRSlotCarModel::Advance(const FFXRSlotCarParams& Params, const FFXRFigure8Path& Lane, float Throttle, float DeltaTime)
{
	const float SubStep = 1.0f / FMath::Max(Params.PhysicsRate, 1.0f);
	// Long hitches (loading, breakpoints) would fire hundreds of steps; cap at a quarter second.
	TimeAccumulator = FMath::Min(TimeAccumulator + DeltaTime, 0.25f);
	while (TimeAccumulator >= SubStep)
	{
		TimeAccumulator -= SubStep;
		// Curvature where the car is now; it then moves at the new speed.
		const bool bDeslot = Step(Params, Throttle, Lane.Sample(Distance).Curvature, SubStep);
		const double Moved = double(Speed) * SubStep;
		TotalDistance += Moved;
		Distance = Lane.Wrap(Distance + Moved);
		if (bDeslot)
		{
			TimeAccumulator = 0.0f;
			return true;
		}
	}
	return false;
}

float FFXRSlotCarModel::SlideAngle(const FFXRSlotCarParams& Params) const
{
	// The tail swings to the outside of the turn, so the nose points further into it.
	return float(FMath::Sign(Curvature)) * SlideMeter * Params.MaxSlideAngle;
}
