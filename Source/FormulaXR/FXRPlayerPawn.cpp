#include "FXRPlayerPawn.h"

#include "Camera/CameraComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FXRModeSubsystem.h"
#include "FXRSaveGame.h"
#include "FXRTrack.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#if WITH_META_XR
#include "OculusXRPassthroughSubsystem.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogFXRPawn, Log, All);

namespace
{
	/** Distance from the headset to the track centre after re-centering, cm. */
	constexpr float RecenterDistance = 100.0f;
	/** PC camera relative to the track centre: a 3/4 view from the long side framing the whole figure 8. */
	const FVector PCCameraOffset(0.0f, -205.0f, 150.0f);

	/** -FXRCam=x,y,z overrides the PC camera offset (cm, track space), e.g. for close-up screenshots. */
	FVector GetPCCameraOffset()
	{
		FString Value;
		if (FParse::Value(FCommandLine::Get(), TEXT("FXRCam="), Value, false))
		{
			TArray<FString> Parts;
			Value.ParseIntoArray(Parts, TEXT(","));
			if (Parts.Num() == 3)
			{
				return FVector(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]));
			}
		}
		return PCCameraOffset;
	}

	/** -FXRCamTarget=x,y,z: where the PC camera looks (cm, track space); the track centre by default. */
	FVector GetPCCameraTarget()
	{
		FString Value;
		TArray<FString> Parts;
		if (FParse::Value(FCommandLine::Get(), TEXT("FXRCamTarget="), Value, false) && Value.ParseIntoArray(Parts, TEXT(",")) == 3)
		{
			return FVector(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]));
		}
		return FVector::ZeroVector;
	}
}

AFXRPlayerPawn::AFXRPlayerPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	TrackingOrigin = CreateDefaultSubobject<USceneComponent>(TEXT("TrackingOrigin"));
	RootComponent = TrackingOrigin;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(TrackingOrigin);
	Camera->bLockToHmd = false;
}

void AFXRPlayerPawn::BeginPlay()
{
	Super::BeginPlay();
	const UFXRModeSubsystem* Mode = GetGameInstance()->GetSubsystem<UFXRModeSubsystem>();
	bMixedReality = Mode && Mode->IsMixedReality();
	if (Mode)
	{
		Mode->ApplyModeVisibility(GetWorld());
	}
	UE_LOG(LogFXRPawn, Log, TEXT("FormulaXR mode: %s"), bMixedReality ? TEXT("mixed reality") : TEXT("PC"));
	// The track height the player set last time (right stick).
	if (AFXRTrack* T = FindTrack())
	{
		T->AddHeightOffset(UFXRSaveGame::Load()->TrackHeightOffset - T->GetHeightOffset());
	}
	if (bMixedReality)
	{
		SetupMixedReality();
	}
	else
	{
		SetupPC();
	}
}

AFXRTrack* AFXRPlayerPawn::FindTrack()
{
	if (!Track.IsValid())
	{
		for (TActorIterator<AFXRTrack> It(GetWorld()); It; ++It)
		{
			Track = *It;
			break;
		}
	}
	return Track.Get();
}

void AFXRPlayerPawn::SetupPC()
{
	Camera->bLockToHmd = false;
	Camera->SetFieldOfView(60.0f);
	if (AFXRTrack* T = FindTrack())
	{
		const FVector Target = T->GetActorLocation() + GetPCCameraTarget();
		const FVector Eye = T->GetActorLocation() + GetPCCameraOffset();
		SetActorLocationAndRotation(Eye, UKismetMathLibrary::FindLookAtRotation(Eye, Target));
	}
}

void AFXRPlayerPawn::SetupMixedReality()
{
	// The pawn stays at the world origin; tracking space is the room with its origin on the floor.
	SetActorLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
	Camera->bLockToHmd = true;
	UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(EHMDTrackingOrigin::LocalFloor);
	ShowPassthrough(true);
	if (AFXRTrack* T = FindTrack())
	{
		T->ShowLapTower();
	}
}

void AFXRPlayerPawn::EndPlay(const EEndPlayReason::Type Reason)
{
	if (const AFXRTrack* T = Track.Get())
	{
		const float Offset = T->GetHeightOffset();
		UFXRSaveGame::Update([Offset](UFXRSaveGame& Save) { Save.TrackHeightOffset = Offset; });
	}
	Super::EndPlay(Reason);
}

void AFXRPlayerPawn::ShowPassthrough(bool bVisible)
{
#if WITH_META_XR
	UOculusXRPassthroughSubsystem* Subsystem = UOculusXRPassthroughSubsystem::GetPassthroughSubsystem(GetWorld());
	if (!Subsystem)
	{
		UE_LOG(LogFXRPawn, Warning, TEXT("Meta XR passthrough unavailable; running in full VR."));
		return;
	}
	// Same approach as the proven handletest setup: a persistent passthrough layer under the
	// scene, visible wherever the rendered alpha is 0 (r.PostProcessing.PropagateAlpha).
	if (UOculusXRPersistentPassthroughInstance* Layer = Subsystem->GetPersistentPassthrough())
	{
		Layer->SetVisible(bVisible);
	}
	else
	{
		FOculusXRPersistentPassthroughParameters Parameters;
		Parameters.bVisible = bVisible;
		Subsystem->InitializePersistentPassthrough(Parameters, FOculusXRPassthrough_LayerResumed_Single());
	}
	UE_LOG(LogFXRPawn, Log, TEXT("Passthrough %s"), bVisible ? TEXT("on") : TEXT("off"));
#else
	UE_LOG(LogFXRPawn, Warning, TEXT("Built without Meta XR; running in full VR."));
#endif
}

void AFXRPlayerPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bMixedReality && !bPlaced && UHeadMountedDisplayFunctionLibrary::HasValidTrackingPosition())
	{
		// The headset pose is not valid for the first frames; place the track once it is.
		RecenterTrack();
	}
}

void AFXRPlayerPawn::AdjustTrackHeight(float DeltaCm)
{
	if (AFXRTrack* T = FindTrack())
	{
		T->AddHeightOffset(DeltaCm);
		if (!bMixedReality)
		{
			SetupPC();   // the PC camera follows the track
		}
	}
}

void AFXRPlayerPawn::RecenterTrack()
{
	AFXRTrack* T = FindTrack();
	if (!bMixedReality || !T)
	{
		return;
	}
	const FVector Head = Camera->GetComponentLocation();
	const float HeadYaw = Camera->GetComponentRotation().Yaw;
	const FVector Forward = FRotator(0.0f, HeadYaw, 0.0f).Vector();
	const FVector Centre = Head + Forward * RecenterDistance;
	// The start line (track +X) runs across the player's view, left to right.
	T->PlaceAt(FVector2D(Centre.X, Centre.Y), HeadYaw + 90.0f);
	bPlaced = true;
	UE_LOG(LogFXRPawn, Log, TEXT("Track placed at (%.0f, %.0f), yaw %.0f"), Centre.X, Centre.Y, HeadYaw + 90.0f);
}
