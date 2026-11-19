#pragma once

#include "CoreMinimal.h"
#include "FXRSlotCarModel.h"
#include "GameFramework/Actor.h"
#include "FXRSlotCar.generated.h"

class AFXRTrack;
class UBoxComponent;
class UFXRCarPreset;
class UFXRSlotCarTuning;
class UAudioComponent;
class UMaterialInstanceDynamic;
class USoundAttenuation;
class USoundBase;
class UProceduralMeshComponent;
class UStaticMeshComponent;

class AFXRSlotCar;
DECLARE_MULTICAST_DELEGATE_OneParam(FFXRSlotCarEvent, AFXRSlotCar*);

UENUM()
enum class EFXRSlotCarState : uint8
{
	OnTrack,
	/** Flying or lying off the track as a physics body until the marshal puts it back. */
	Deslotted,
};

/**
 * One slot car on one lane (docs/plan.md §4.2). On the track it follows FFXRSlotCarModel;
 * when it de-slots it becomes a physics body that leaves along the tangent, and after
 * MarshalDelay it is put back where it left, at standstill.
 */
UCLASS()
class FORMULAXR_API AFXRSlotCar : public AActor
{
	GENERATED_BODY()

public:
	AFXRSlotCar();

	/** Called once after spawning: which track and lane, and what the car looks like. */
	void Setup(AFXRTrack* InTrack, int32 InLane, UFXRCarPreset* InPreset);

	/** Raw trigger position, 0..1. Shaped by the tuning's dead zone and curve. */
	void SetTrigger(float Trigger) { TriggerInput = Trigger; }

	/** Parks the car standstill at a distance along its lane (also ends a de-slot early). */
	void PlaceOnLane(double Distance);

	/** Before lights out the motor gets no power, whatever the trigger says. */
	void SetDriveEnabled(bool bEnabled) { bDriveEnabled = bEnabled; }

	/** Knocks the car out of its slot (crossover crash). Does nothing if it is already off. */
	void ForceDeslot();

	/** The car body along the lane, measured from the guide pin: cm behind and ahead of it. */
	void GetBodyRange(double& OutBehind, double& OutAhead) const;

	/**
	 * Just put back by the marshal and not yet driven clear (25 cm). Such a car doesn't collide at the
	 * crossover; otherwise two cars put back where they crashed would crash again at once.
	 */
	bool IsRejoining() const;

	/**
	 * Mixed reality: a soft shadow under the car on the track plane, which also covers the real table
	 * when the car flies off. Replaces the car's own shadow so both look the same.
	 */
	void EnableSoftShadow();

	/** Fired when the guide pin crosses the start/finish line. */
	FFXRSlotCarEvent OnLineCrossed;
	/** Fired when the car leaves its slot. */
	FFXRSlotCarEvent OnDeslotted;

	int32 GetLane() const { return Lane; }
	EFXRSlotCarState GetState() const { return State; }
	const FFXRSlotCarModel& GetModel() const { return Model; }
	const FFXRSlotCarParams& GetParams() const;

	virtual void Tick(float DeltaSeconds) override;

	/** Optional tuning asset; the FFXRSlotCarParams defaults apply when empty. */
	UPROPERTY(EditAnywhere, Category = "Slot Car")
	TObjectPtr<UFXRSlotCarTuning> Tuning;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBoxComponent> Chassis;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Body;

	/** All four wheels as one static mesh: shown only if the wheels can't be split for spinning. */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> StaticWheels;

	/** Front and rear axle: a pivot on the axle that spins, holding that pair of wheels. */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> AxlePivots[2];

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProceduralMeshComponent> AxleWheels[2];

	UPROPERTY()
	TObjectPtr<UFXRCarPreset> Preset;

	/** Motor whine: a looping sound whose pitch and volume follow speed, placed at the car (3D). */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UAudioComponent> MotorAudio;

	UPROPERTY()
	TObjectPtr<USoundBase> DeslotSound;

	/** Small-scale 3D falloff shared by the car's sounds (built in code, no asset). */
	UPROPERTY()
	TObjectPtr<USoundAttenuation> Attenuation;

	float MotorPitch = 0.55f;
	float MotorVolume = 0.0f;

	/** Mixed reality soft shadow; null until EnableSoftShadow. */
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> SoftShadow;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> SoftShadowMaterial;

	TWeakObjectPtr<AFXRTrack> Track;
	int32 Lane = 0;
	FFXRSlotCarModel Model;
	EFXRSlotCarState State = EFXRSlotCarState::OnTrack;
	float TriggerInput = 0.0f;
	float MarshalTimer = 0.0f;
	bool bDriveEnabled = true;
	/** TotalDistance when the marshal last put the car back; below 0 = not rejoining. */
	double RejoinedAt = -1.0;

	/** Half the car's length and its ride height (chassis centre above the track), from the mesh, cm. */
	float HalfLength = 5.6f;
	float HalfWidth = 3.25f;
	float HalfHeight = 2.17f;
	/** Rolling radius at slot scale, cm; 0 when the wheels don't spin. */
	float WheelRadius = 0.0f;
	float WheelAngle = 0.0f;

	void ApplyPreset();
	bool BuildSpinningWheels(const FTransform& MeshToChassis);
	void SpinWheels(float DeltaSeconds);
	void UpdateSoftShadow();
	void SetupAudio();
	void UpdateMotorAudio(float DeltaSeconds);
	void UpdateTransformOnTrack();
	void Deslot();
	void Marshal();
	void StopPhysics();
	void CheckLineCrossing(double PreviousTotal);
	void ShowDebug() const;
};
