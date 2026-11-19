#include "FXRSlotCar.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "FXRCarPreset.h"
#include "FXRSlotCarTuning.h"
#include "FXRTrack.h"
#include "HAL/IConsoleManager.h"
#include "KismetProceduralMeshLibrary.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogFXRCar, Log, All);

static TAutoConsoleVariable<int32> CVarShowCarDebug(
	TEXT("fxr.ShowCarDebug"), 0, TEXT("1: show each car's speed and slide meter on screen (the HUD shows laps and times)."));

namespace
{
	// Track layer the cars ride on (top of the slot strip), cm.
	constexpr float RideHeight = 0.06f;
	enum { FrontAxle = 0, RearAxle = 1 };
}

AFXRSlotCar::AFXRSlotCar()
{
	PrimaryActorTick.bCanEverTick = true;

	Chassis = CreateDefaultSubobject<UBoxComponent>(TEXT("Chassis"));
	Chassis->SetMobility(EComponentMobility::Movable);
	Chassis->SetCollisionProfileName(TEXT("PhysicsActor"));
	Chassis->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootComponent = Chassis;

	auto MakeMesh = [this](const TCHAR* Name)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(Chassis);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return Part;
	};
	Body = MakeMesh(TEXT("Body"));
	StaticWheels = MakeMesh(TEXT("StaticWheels"));

	const TCHAR* PivotNames[2] = { TEXT("FrontAxle"), TEXT("RearAxle") };
	const TCHAR* WheelNames[2] = { TEXT("FrontWheels"), TEXT("RearWheels") };
	for (int32 Axle = 0; Axle < 2; ++Axle)
	{
		AxlePivots[Axle] = CreateDefaultSubobject<USceneComponent>(PivotNames[Axle]);
		AxlePivots[Axle]->SetupAttachment(Chassis);
		AxleWheels[Axle] = CreateDefaultSubobject<UProceduralMeshComponent>(WheelNames[Axle]);
		AxleWheels[Axle]->SetupAttachment(AxlePivots[Axle]);
		AxleWheels[Axle]->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

const FFXRSlotCarParams& AFXRSlotCar::GetParams() const
{
	static const FFXRSlotCarParams Defaults;
	return Tuning ? Tuning->Params : Defaults;
}

void AFXRSlotCar::Setup(AFXRTrack* InTrack, int32 InLane, UFXRCarPreset* InPreset)
{
	Track = InTrack;
	Lane = InLane;
	Preset = InPreset;
	ApplyPreset();
}

void AFXRSlotCar::ApplyPreset()
{
	if (!Preset || !Preset->BodyMesh)
	{
		UE_LOG(LogFXRCar, Warning, TEXT("Car %d has no body mesh (run Scripts\\import_toy_car.ps1)"), Lane + 1);
		return;
	}
	const FRotator MeshRotation(0.0f, Preset->YawCorrection, 0.0f);
	const FVector MeshScale(Preset->Scale);

	// The chassis box is the car's bounds at slot scale, centred on the actor.
	FBox MeshBounds = Preset->BodyMesh->GetBoundingBox();
	if (Preset->WheelsMesh)
	{
		MeshBounds += Preset->WheelsMesh->GetBoundingBox();
	}
	const FBox CarBounds = MeshBounds.TransformBy(FTransform(MeshRotation, FVector::ZeroVector, MeshScale));
	Chassis->SetBoxExtent(CarBounds.GetExtent());
	HalfLength = CarBounds.GetExtent().X;
	HalfHeight = CarBounds.GetExtent().Z;
	const FTransform MeshToChassis(MeshRotation, -CarBounds.GetCenter(), MeshScale);

	Body->SetStaticMesh(Preset->BodyMesh);
	Body->SetRelativeTransform(MeshToChassis);
	Body->SetMaterial(0, Preset->Paint);

	const bool bSpinning = Preset->WheelsMesh && BuildSpinningWheels(MeshToChassis);
	StaticWheels->SetStaticMesh(bSpinning ? nullptr : Preset->WheelsMesh.Get());
	StaticWheels->SetRelativeTransform(MeshToChassis);
	StaticWheels->SetMaterial(0, Preset->Paint);
	StaticWheels->SetVisibility(!bSpinning);
	UE_LOG(LogFXRCar, Log, TEXT("Car %d: %.1f x %.1f x %.1f cm, wheels %s (radius %.2f cm)"), Lane + 1,
		2 * CarBounds.GetExtent().X, 2 * CarBounds.GetExtent().Y, 2 * CarBounds.GetExtent().Z,
		bSpinning ? TEXT("spinning") : TEXT("static"), WheelRadius);
}

bool AFXRSlotCar::BuildSpinningWheels(const FTransform& MeshToChassis)
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FProcMeshTangent> Tangents;
	// Needs CPU access on the mesh in packaged builds (set by the import script).
	UKismetProceduralMeshLibrary::GetSectionFromStaticMesh(Preset->WheelsMesh, 0, 0, Vertices, Triangles, Normals, UVs, Tangents);
	if (Vertices.Num() == 0 || Triangles.Num() == 0)
	{
		return false;
	}

	// The car's forward axis in mesh space: front wheels are ahead of the middle, rear behind.
	const FVector MeshForward = MeshToChassis.GetRotation().Inverse().RotateVector(FVector::ForwardVector);
	struct FPart
	{
		TArray<FVector> Vertices, Normals;
		TArray<FVector2D> UVs;
		TArray<FProcMeshTangent> Tangents;
		TArray<int32> Triangles;
		TMap<int32, int32> Remap;
		FBox Bounds = FBox(ForceInit);
	} Parts[2];
	for (int32 Index = 0; Index + 2 < Triangles.Num(); Index += 3)
	{
		const FVector Centroid = (Vertices[Triangles[Index]] + Vertices[Triangles[Index + 1]] + Vertices[Triangles[Index + 2]]) / 3.0;
		FPart& Part = Parts[FVector::DotProduct(Centroid, MeshForward) >= 0.0 ? FrontAxle : RearAxle];
		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			const int32 Source = Triangles[Index + Corner];
			int32* Existing = Part.Remap.Find(Source);
			if (!Existing)
			{
				const int32 New = Part.Vertices.Add(Vertices[Source]);
				Part.Normals.Add(Normals.IsValidIndex(Source) ? Normals[Source] : FVector::UpVector);
				Part.UVs.Add(UVs.IsValidIndex(Source) ? UVs[Source] : FVector2D::ZeroVector);
				Part.Tangents.Add(Tangents.IsValidIndex(Source) ? Tangents[Source] : FProcMeshTangent());
				Part.Bounds += Vertices[Source];
				Existing = &Part.Remap.Add(Source, New);
			}
			Part.Triangles.Add(*Existing);
		}
	}
	if (Parts[FrontAxle].Triangles.Num() == 0 || Parts[RearAxle].Triangles.Num() == 0)
	{
		return false;
	}

	for (int32 Axle = 0; Axle < 2; ++Axle)
	{
		FPart& Part = Parts[Axle];
		// Both wheels of a pair share the axle through the middle of their combined bounds.
		const FVector AxleCentre = Part.Bounds.GetCenter();
		for (FVector& Vertex : Part.Vertices)
		{
			Vertex -= AxleCentre;
		}
		AxleWheels[Axle]->CreateMeshSection(0, Part.Vertices, Part.Triangles, Part.Normals, Part.UVs,
			TArray<FColor>(), Part.Tangents, false);
		AxleWheels[Axle]->SetMaterial(0, Preset->Paint);
		// The pivot sits on the axle with no rotation, so spinning it is a pitch about the car's side axis.
		AxlePivots[Axle]->SetRelativeLocation(MeshToChassis.TransformPosition(AxleCentre));
		AxleWheels[Axle]->SetRelativeTransform(FTransform(MeshToChassis.GetRotation(), FVector::ZeroVector, MeshToChassis.GetScale3D()));
	}
	// Wheel radius from the front pair's height.
	WheelRadius = Parts[FrontAxle].Bounds.GetExtent().Z * Preset->Scale;
	return WheelRadius > KINDA_SMALL_NUMBER;
}

void AFXRSlotCar::SpinWheels(float DeltaSeconds)
{
	if (WheelRadius <= 0.0f)
	{
		return;
	}
	// Rolling forward moves the top of the wheel forward (+X). Find which pitch direction does that
	// rather than relying on the engine's rotation sign convention.
	static const float ForwardRoll = FRotator(-1.0f, 0.0f, 0.0f).RotateVector(FVector::UpVector).X > 0.0 ? -1.0f : 1.0f;
	WheelAngle = FMath::Fmod(WheelAngle + ForwardRoll * FMath::RadiansToDegrees(Model.Speed / WheelRadius) * DeltaSeconds, 360.0f);
	for (USceneComponent* Pivot : AxlePivots)
	{
		Pivot->SetRelativeRotation(FRotator(WheelAngle, 0.0f, 0.0f));
	}
}

void AFXRSlotCar::PlaceOnLane(double Distance)
{
	StopPhysics();
	Model.Reset(Distance);
	State = EFXRSlotCarState::OnTrack;
	MarshalTimer = 0.0f;
	RejoinedAt = -1.0;
	UpdateTransformOnTrack();
}

bool AFXRSlotCar::IsRejoining() const
{
	constexpr double ClearDistance = 25.0;
	return State == EFXRSlotCarState::OnTrack && RejoinedAt >= 0.0 && Model.TotalDistance - RejoinedAt < ClearDistance;
}

void AFXRSlotCar::ForceDeslot()
{
	if (State == EFXRSlotCarState::OnTrack)
	{
		Deslot();
	}
}

void AFXRSlotCar::GetBodyRange(double& OutBehind, double& OutAhead) const
{
	const float GuideAhead = Preset ? Preset->GuideAhead : 3.6f;
	OutBehind = HalfLength + GuideAhead;
	OutAhead = FMath::Max(0.0f, HalfLength - GuideAhead);
}

void AFXRSlotCar::StopPhysics()
{
	if (Chassis->IsSimulatingPhysics())
	{
		Chassis->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Chassis->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		Chassis->SetSimulatePhysics(false);
	}
	Chassis->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AFXRSlotCar::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AFXRTrack* T = Track.Get();
	if (!T)
	{
		return;
	}

	if (State == EFXRSlotCarState::OnTrack)
	{
		const FFXRSlotCarParams& Params = GetParams();
		const double PreviousTotal = Model.TotalDistance;
		const float Throttle = bDriveEnabled ? Params.ShapeThrottle(TriggerInput) : 0.0f;
		const bool bDeslot = Model.Advance(Params, T->GetLane(Lane), Throttle, DeltaSeconds);
		CheckLineCrossing(PreviousTotal);
		SpinWheels(DeltaSeconds);
		UpdateTransformOnTrack();
		if (bDeslot)
		{
			Deslot();
		}
	}
	else
	{
		MarshalTimer -= DeltaSeconds;
		if (MarshalTimer <= 0.0f)
		{
			Marshal();
		}
	}

	if (CVarShowCarDebug.GetValueOnGameThread() != 0)
	{
		ShowDebug();
	}
}

void AFXRSlotCar::UpdateTransformOnTrack()
{
	AFXRTrack* T = Track.Get();
	if (!T)
	{
		return;
	}
	// The guide pin follows the slot; the body pivots around it as the tail slides out.
	const float GuideAhead = Preset ? Preset->GuideAhead : 3.6f;
	const FFXRPathSample Guide = T->GetLane(Lane).Sample(Model.Distance);
	const double Heading = FMath::Atan2(Guide.Tangent.Y, Guide.Tangent.X) + FMath::DegreesToRadians(Model.SlideAngle(GetParams()));
	const FVector2D Forward(FMath::Cos(Heading), FMath::Sin(Heading));
	const FVector2D Centre = Guide.Location - Forward * GuideAhead;
	const FVector Location = T->TrackToWorld(Centre, RideHeight + HalfHeight);
	const FRotator Rotation(0.0f, T->GetActorRotation().Yaw + FMath::RadiansToDegrees(Heading), 0.0f);
	SetActorLocationAndRotation(Location, Rotation);
}

void AFXRSlotCar::Deslot()
{
	State = EFXRSlotCarState::Deslotted;
	MarshalTimer = GetParams().MarshalDelay;

	// The guide leaves the slot: the car carries on along its heading and tumbles.
	const FVector Velocity = GetActorForwardVector() * Model.Speed + FVector(0.0f, 0.0f, 0.15f * Model.Speed);
	const float Turn = Model.Curvature >= 0.0 ? 1.0f : -1.0f;
	Chassis->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Chassis->SetSimulatePhysics(true);
	// A light plastic car tumbles to a stop within about half a metre rather than skating off the table.
	Chassis->SetLinearDamping(2.5f);
	Chassis->SetAngularDamping(1.5f);
	Chassis->SetPhysicsLinearVelocity(Velocity);
	// Yaw spin towards the outside of the turn plus a roll, scaled with speed.
	const float Spin = Model.Speed * 1.2f;
	Chassis->SetPhysicsAngularVelocityInDegrees(GetActorTransform().TransformVectorNoScale(FVector(Turn * Spin, 0.0f, -Turn * Spin)));
	UE_LOG(LogFXRCar, Log, TEXT("Car %d de-slotted at %.0f cm/s (lateral %.0f cm/s^2, distance %.0f)"),
		Lane + 1, Model.Speed, Model.LateralAccel, Model.Distance);
	Model.Speed = 0.0f;
	OnDeslotted.Broadcast(this);
}

void AFXRSlotCar::Marshal()
{
	if (const AFXRTrack* T = Track.Get())
	{
		const FVector Local = T->GetActorTransform().InverseTransformPosition(GetActorLocation());
		UE_LOG(LogFXRCar, Log, TEXT("Car %d landed at track (%.0f, %.0f, %.0f)"), Lane + 1, Local.X, Local.Y, Local.Z);
	}
	StopPhysics();
	// Back in the slot where it left, standing still; laps already driven still count.
	const double Total = Model.TotalDistance;
	Model.Reset(Model.Distance);
	Model.TotalDistance = Total;
	RejoinedAt = Total;
	State = EFXRSlotCarState::OnTrack;
	UpdateTransformOnTrack();
	UE_LOG(LogFXRCar, Log, TEXT("Car %d back on track"), Lane + 1);
}

void AFXRSlotCar::CheckLineCrossing(double PreviousTotal)
{
	AFXRTrack* T = Track.Get();
	const double LapLength = T->GetLane(Lane).GetLength();
	const double Line = T->StartLineDistance;
	const int64 Before = FMath::FloorToInt64((PreviousTotal - Line) / LapLength);
	const int64 After = FMath::FloorToInt64((Model.TotalDistance - Line) / LapLength);
	for (int64 Crossing = Before; Crossing < After; ++Crossing)
	{
		OnLineCrossed.Broadcast(this);
	}
}

void AFXRSlotCar::ShowDebug() const
{
	if (!GEngine)
	{
		return;
	}
	const FString Text = State == EFXRSlotCarState::OnTrack
		? FString::Printf(TEXT("Car %d  %3.0f cm/s  slide %3.0f%%"), Lane + 1, Model.Speed, Model.SlideMeter * 100.0f)
		: FString::Printf(TEXT("Car %d  OFF - marshal in %.1f s"), Lane + 1, FMath::Max(MarshalTimer, 0.0f));
	GEngine->AddOnScreenDebugMessage(int32(GetUniqueID()), 0.0f, Lane == 0 ? FColor(255, 90, 80) : FColor(90, 150, 255), Text);
}
