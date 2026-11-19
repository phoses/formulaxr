#pragma once

#include "CoreMinimal.h"
#include "FXRSlotCarModel.generated.h"

class FFXRFigure8Path;

/** Every number that shapes how a slot car drives (docs/plan.md §4.2). Units: cm, s. */
USTRUCT(BlueprintType)
struct FORMULAXR_API FFXRSlotCarParams
{
	GENERATED_BODY()

	/** Speed the motor tops out at, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motor")
	float TopSpeed = 400.0f;

	/** Acceleration from standstill at full throttle, cm/s^2. Falls to 0 at TopSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motor")
	float MaxAccel = 900.0f;

	/** Braking when the trigger is released (slot motors brake with power off), 1/s, times speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motor")
	float MotorBrake = 4.0f;

	/** Rolling and air drag, 1/s, times speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motor")
	float Drag = 0.15f;

	/** Sideways acceleration the guide magnet holds, cm/s^2 (~1.7 g). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip")
	float GripLimit = 1650.0f;

	/**
	 * Slide meter fill per second per unit of overshoot (lateral / grip - 1). Reaching 1 de-slots.
	 * 3.0 gives ~0.45 s of visible slide flat out into a loop (time to lift), while 10-30% too
	 * fast slides through a whole loop and recovers. (First playtest: 6.0 felt sudden and uncatchable.)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip")
	float SlideFillRate = 3.0f;

	/** Slide meter drain per second once grip returns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip")
	float SlideDrainRate = 2.5f;

	/** How far the tail swings out at a full slide meter, degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip")
	float MaxSlideAngle = 25.0f;

	/** Trigger travel ignored at the bottom, 0..1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Controls")
	float TriggerDeadZone = 0.03f;

	/** Throttle = trigger ^ exponent: above 1 gives finer control at low throttle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Controls")
	float ThrottleExponent = 1.4f;

	/** Seconds a de-slotted car is off before the marshal puts it back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race")
	float MarshalDelay = 2.0f;

	/** Fixed physics sub-steps per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation")
	float PhysicsRate = 240.0f;

	/** Trigger position (0..1) to throttle, with dead zone and response curve. */
	float ShapeThrottle(float Trigger) const;

	/** Highest speed a corner of this curvature holds indefinitely, cm/s. */
	float CornerSpeedLimit(double Curvature) const;
};

/**
 * One slot car's motion along its lane: speed, distance and how close it is to de-slotting.
 * Pure maths so it can be unit tested without a world.
 */
struct FORMULAXR_API FFXRSlotCarModel
{
	/** Distance along the lane, wrapped to one lap, cm. */
	double Distance = 0.0;
	/** Distance including completed laps (for lap counting), cm. */
	double TotalDistance = 0.0;
	/** cm/s, never negative (slot cars don't reverse here). */
	float Speed = 0.0f;
	/** 0 = gripping, 1 = de-slots. */
	float SlideMeter = 0.0f;
	/** Curvature and sideways acceleration at the current position. */
	double Curvature = 0.0;
	float LateralAccel = 0.0f;

	/** Leftover time below one sub-step, carried to the next Advance. */
	float TimeAccumulator = 0.0f;

	void Reset(double InDistance);

	/** One fixed step with a known curvature. Returns true when the car de-slots. */
	bool Step(const FFXRSlotCarParams& Params, float Throttle, double InCurvature, float Dt);

	/** Advances DeltaTime along Lane in fixed sub-steps. Returns true when the car de-slots (and stops there). */
	bool Advance(const FFXRSlotCarParams& Params, const FFXRFigure8Path& Lane, float Throttle, float DeltaTime);

	/** Body yaw relative to the lane tangent from the tail slide, degrees (+ = nose to the left). */
	float SlideAngle(const FFXRSlotCarParams& Params) const;
};
