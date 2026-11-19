#include "FXRRaceGameMode.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextureCube.h"
#include "Engine/World.h"
#include "FXRCarPreset.h"
#include "Kismet/GameplayStatics.h"
#include "RenderCore.h"
#include "RHI.h"
#include "Sound/SoundBase.h"
#include "FXRModeSubsystem.h"
#include "FXRPlayerController.h"
#include "FXRPlayerPawn.h"
#include "FXRSaveGame.h"
#include "FXRSlotCar.h"
#include "FXRTrack.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogFXRGame, Log, All);

AFXRRaceGameMode::AFXRRaceGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = AFXRPlayerPawn::StaticClass();
	PlayerControllerClass = AFXRPlayerController::StaticClass();
}

void AFXRRaceGameMode::StartPlay()
{
	// Spawned before Super::StartPlay so everything is in the world when BeginPlay runs.
	SpawnStage();
	Super::StartPlay();

	LightBeep = LoadObject<USoundBase>(nullptr, TEXT("/Game/FormulaXR/Audio/SW_LightBeep.SW_LightBeep"));
	GoBeep = LoadObject<USoundBase>(nullptr, TEXT("/Game/FormulaXR/Audio/SW_GoBeep.SW_GoBeep"));
	FinishChime = LoadObject<USoundBase>(nullptr, TEXT("/Game/FormulaXR/Audio/SW_Finish.SW_Finish"));

	FParse::Value(FCommandLine::Get(), TEXT("FXRLaps="), RaceLaps);
	bPerfLog = FParse::Param(FCommandLine::Get(), TEXT("FXRPerfLog"));
	NextMode = FParse::Param(FCommandLine::Get(), TEXT("FXRPractice")) ? EFXRRaceMode::Practice : EFXRRaceMode::Race;
	StartSession();

	if (FParse::Value(FCommandLine::Get(), TEXT("FXRShot="), ShotPath))
	{
		float Delay = 6.0f;
		FParse::Value(FCommandLine::Get(), TEXT("FXRShotDelay="), Delay);
		GetWorldTimerManager().SetTimer(ShotTimer, this, &AFXRRaceGameMode::TakeShot, Delay, false);
	}
}

namespace
{
	/** A PC-only box of the room, SizeCm big, centred at Centre, painted with one of the room materials. */
	AStaticMeshActor* SpawnBox(UWorld* World, const FVector& Centre, const FVector& SizeCm, const TCHAR* MaterialName)
	{
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(Centre, FRotator::ZeroRotator, Params);
		UStaticMeshComponent* Mesh = Actor->GetStaticMeshComponent();
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetStaticMesh(Cube);
		const FString MaterialPath = FString::Printf(TEXT("/Game/FormulaXR/Materials/%s.%s"), MaterialName, MaterialName);
		if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath))
		{
			Mesh->SetMaterial(0, Material);
		}
		Actor->SetActorScale3D(SizeCm / 100.0f);   // engine cube is 100 cm
		Actor->Tags.Add(FXR_PC_ONLY_TAG);
		return Actor;
	}

	/**
	 * PC mode's minimal setting: a floor and a table whose top is at the track's default height.
	 * Kept deliberately simple: mixed reality is the primary target and hides all of this.
	 */
	void SpawnRoom(UWorld* World)
	{
		SpawnBox(World, FVector(0, 0, -1.0f), FVector(1000, 1000, 2), TEXT("MI_RoomFloor"));
		const float TableTop = AFXRTrack::DefaultHeight - 1.5f;   // the track base board sits on top
		SpawnBox(World, FVector(0, 0, TableTop - 2.0f), FVector(320, 160, 4), TEXT("MI_TableTop"));
		for (const FVector2D Leg : { FVector2D(150, 70), FVector2D(-150, 70), FVector2D(150, -70), FVector2D(-150, -70) })
		{
			SpawnBox(World, FVector(Leg.X, Leg.Y, (TableTop - 4.0f) / 2.0f), FVector(5, 5, TableTop - 4.0f), TEXT("MI_TableLeg"));
		}
	}
}

void AFXRRaceGameMode::SpawnStage()
{
	UWorld* World = GetWorld();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Lights work in both modes: they light the track and cars over passthrough too.
	ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(FVector(0, 0, 300), FRotator(-50.0f, 30.0f, 0.0f), Params);
	Sun->GetLightComponent()->SetMobility(EComponentMobility::Movable);
	Sun->GetLightComponent()->SetIntensity(4.0f);

	ASkyLight* Sky = World->SpawnActor<ASkyLight>(FVector(0, 0, 200), FRotator::ZeroRotator, Params);
	USkyLightComponent* SkyComponent = Sky->GetLightComponent();
	SkyComponent->SetMobility(EComponentMobility::Movable);
	SkyComponent->SourceType = ESkyLightSourceType::SLS_SpecifiedCubemap;
	SkyComponent->SetCubemap(LoadObject<UTextureCube>(nullptr, TEXT("/Engine/MapTemplates/Sky/SunsetAmbientCubemap.SunsetAmbientCubemap")));
	SkyComponent->SetIntensity(1.0f);
	SkyComponent->RecaptureSky();

	SpawnRoom(World);

	AFXRTrack* NewTrack = World->SpawnActor<AFXRTrack>(FVector(0, 0, AFXRTrack::DefaultHeight), FRotator::ZeroRotator, Params);
	NewTrack->PlaceAt(FVector2D::ZeroVector, 0.0f);
	NewTrack->EnsureBuilt();
	Track = NewTrack;

	// Car 1 (red, left trigger) on lane 0, car 2 (blue, right trigger) on lane 1.
	const TCHAR* Presets[AFXRTrack::NumLanes] = {
		TEXT("/Game/FormulaXR/Cars/DA_Car1_Red.DA_Car1_Red"),
		TEXT("/Game/FormulaXR/Cars/DA_Car2_Blue.DA_Car2_Blue"),
	};
	const TCHAR* Paints[AFXRTrack::NumLanes] = {
		TEXT("/Game/Fab/ToyRacingCar/MI_ToyCar_Red.MI_ToyCar_Red"),
		TEXT("/Game/Fab/ToyRacingCar/MI_ToyCar_Blue.MI_ToyCar_Blue"),
	};
	for (int32 Lane = 0; Lane < AFXRTrack::NumLanes; ++Lane)
	{
		AFXRSlotCar* Car = World->SpawnActor<AFXRSlotCar>(NewTrack->GetActorLocation(), FRotator::ZeroRotator, Params);
		Car->Setup(NewTrack, Lane, UFXRCarPreset::LoadOrDefault(this, Presets[Lane], Paints[Lane]));
		Car->OnLineCrossed.AddUObject(this, &AFXRRaceGameMode::HandleLineCrossed);
		Car->OnDeslotted.AddUObject(this, &AFXRRaceGameMode::HandleDeslot);
		Cars.Add(Car);
	}
}

AFXRSlotCar* AFXRRaceGameMode::GetCar(int32 Lane) const
{
	return Cars.IsValidIndex(Lane) ? Cars[Lane].Get() : nullptr;
}

void AFXRRaceGameMode::StartSession()
{
	// Lights out after a random hold, so the start can't be anticipated.
	double LightsOut = FMath::FRandRange(0.4, 1.6);
	FParse::Value(FCommandLine::Get(), TEXT("FXRLightsOut="), LightsOut);
	Race.Reset(NextMode, RaceLaps, LightsOut);
	bNewRecord = false;

	const UFXRSaveGame* Save = UFXRSaveGame::Load();
	const float* Record = Save->BestRaceTimes.Find(Race.TargetLaps);
	RecordBefore = Record ? *Record : 0.0f;
	SavedBestLap[0] = Save->BestLap[0];
	SavedBestLap[1] = Save->BestLap[1];

	const AFXRTrack* T = Track.Get();
	for (int32 Lane = 0; Lane < Cars.Num(); ++Lane)
	{
		if (AFXRSlotCar* Car = Cars[Lane].Get())
		{
			Car->PlaceOnLane(T ? T->GetGridDistance() : 0.0);
			Car->SetDriveEnabled(Race.CanDrive());
		}
	}
	UE_LOG(LogFXRGame, Log, TEXT("%s started (%d laps, lights out %.2f s after the fifth light)"),
		Race.Mode == EFXRRaceMode::Race ? TEXT("Race") : TEXT("Practice"), Race.TargetLaps, Race.LightsOutDelay);
}

void AFXRRaceGameMode::RestartRace()
{
	StartSession();
}

void AFXRRaceGameMode::ToggleMode()
{
	NextMode = Race.Mode == EFXRRaceMode::Race ? EFXRRaceMode::Practice : EFXRRaceMode::Race;
	StartSession();
}

void AFXRRaceGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Race.Tick(DeltaSeconds) && Race.Phase == EFXRRacePhase::Racing && Race.Mode == EFXRRaceMode::Race)
	{
		UE_LOG(LogFXRGame, Log, TEXT("Lights out - go!"));
		PlayRaceSound(GoBeep);
	}
	const int32 Lit = Race.LightsLit();
	if (Lit > LastLightsLit)
	{
		PlayRaceSound(LightBeep);
	}
	LastLightsLit = Lit;
	if (bPerfLog)
	{
		UpdatePerfLog(DeltaSeconds);
	}
	for (const TWeakObjectPtr<AFXRSlotCar>& Car : Cars)
	{
		if (Car.IsValid())
		{
			Car->SetDriveEnabled(Race.CanDrive());
		}
	}
	CheckCrossover();
}

void AFXRRaceGameMode::HandleLineCrossed(AFXRSlotCar* Car)
{
	const int32 Lane = Car->GetLane();
	if (!Race.OnLineCrossed(Lane))
	{
		return;
	}
	const FFXRCarRaceState& State = Race.Cars[Lane];
	UE_LOG(LogFXRGame, Log, TEXT("Car %d lap %d: %.3f s (best %.3f)"), Lane + 1, State.LapsCompleted, State.LastLapTime, State.BestLapTime);

	if (SavedBestLap[Lane] <= 0.0f || State.LastLapTime < SavedBestLap[Lane])
	{
		SavedBestLap[Lane] = State.LastLapTime;
		const float Best = SavedBestLap[Lane];
		UFXRSaveGame::Update([Lane, Best](UFXRSaveGame& Save) { Save.BestLap[Lane] = Best; });
	}
	if (State.bFinished)
	{
		UE_LOG(LogFXRGame, Log, TEXT("Car %d finished in %.3f s"), Lane + 1, State.FinishTime);
	}
	if (Race.Phase == EFXRRacePhase::Finished)
	{
		OnRaceFinished();
	}
}

void AFXRRaceGameMode::HandleDeslot(AFXRSlotCar* Car)
{
	Race.OnDeslot(Car->GetLane());
}

void AFXRRaceGameMode::CheckCrossover()
{
	const AFXRTrack* T = Track.Get();
	AFXRSlotCar* Car0 = GetCar(0);
	AFXRSlotCar* Car1 = GetCar(1);
	if (!T || !Car0 || !Car1 || Car0->GetState() != EFXRSlotCarState::OnTrack || Car1->GetState() != EFXRSlotCarState::OnTrack
		|| Car0->IsRejoining() || Car1->IsRejoining())
	{
		return;
	}
	// The lanes cross at an angle, so a car blocks the crossing over a little more than its
	// length: add half the other car's width.
	double Behind0, Ahead0, Behind1, Ahead1;
	Car0->GetBodyRange(Behind0, Ahead0);
	Car1->GetBodyRange(Behind1, Ahead1);
	const double Margin = 3.0;
	const double Lap0 = T->GetLane(0).GetLength();
	const double Lap1 = T->GetLane(1).GetLength();
	for (const FFXRPathCrossing& Crossing : T->GetLaneCrossings())
	{
		if (FFXRRace::CoversPoint(Lap0, Car0->GetModel().Distance, Behind0 + Margin, Ahead0 + Margin, Crossing.DistanceA)
			&& FFXRRace::CoversPoint(Lap1, Car1->GetModel().Distance, Behind1 + Margin, Ahead1 + Margin, Crossing.DistanceB))
		{
			UE_LOG(LogFXRGame, Log, TEXT("Crossover crash at (%.1f, %.1f)"), Crossing.Location.X, Crossing.Location.Y);
			Car0->ForceDeslot();
			Car1->ForceDeslot();
			return;
		}
	}
}

void AFXRRaceGameMode::OnRaceFinished()
{
	PlayRaceSound(FinishChime);
	const float Total = float(Race.RaceTime());
	bNewRecord = RecordBefore <= 0.0f || Total < RecordBefore;
	UE_LOG(LogFXRGame, Log, TEXT("Race finished: %.3f s for %d laps%s"), Total, Race.TargetLaps, bNewRecord ? TEXT(" - new record!") : TEXT(""));
	if (bNewRecord)
	{
		const int32 Laps = Race.TargetLaps;
		UFXRSaveGame::Update([Laps, Total](UFXRSaveGame& Save) { Save.BestRaceTimes.Add(Laps, Total); });
	}
}

void AFXRRaceGameMode::UpdatePerfLog(float DeltaSeconds)
{
	const double Gpu = FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0));
	Perf.Frame += DeltaSeconds * 1000.0;
	Perf.Game += FPlatformTime::ToMilliseconds(GGameThreadTime);
	Perf.Render += FPlatformTime::ToMilliseconds(GRenderThreadTime);
	Perf.Gpu += Gpu;
	Perf.MaxGpu = FMath::Max(Perf.MaxGpu, Gpu);
	++Perf.Frames;
	Perf.Elapsed += DeltaSeconds;
	if (Perf.Elapsed >= 2.0)
	{
		const double N = Perf.Frames;
		UE_LOG(LogFXRGame, Log, TEXT("Perf: frame %.2f ms (%.0f fps)  game %.2f  render %.2f  GPU %.2f (max %.2f) ms"),
			Perf.Frame / N, 1000.0 * N / Perf.Frame, Perf.Game / N, Perf.Render / N, Perf.Gpu / N, Perf.MaxGpu);
		Perf = FPerfWindow();
	}
}

void AFXRRaceGameMode::PlayRaceSound(USoundBase* Sound) const
{
	if (Sound)
	{
		UGameplayStatics::PlaySound2D(this, Sound, 0.6f);
	}
}

float AFXRRaceGameMode::GetBestLapEver(int32 Lane) const
{
	return (Lane == 0 || Lane == 1) ? SavedBestLap[Lane] : 0.0f;
}

void AFXRRaceGameMode::TakeShot()
{
	UE_LOG(LogFXRGame, Log, TEXT("Screenshot -> %s"), *ShotPath);
	// With UI, so the HUD is in the picture.
	FScreenshotRequest::RequestScreenshot(ShotPath, true, false);
	FTimerHandle QuitTimer;
	GetWorldTimerManager().SetTimer(QuitTimer, []() { FPlatformMisc::RequestExit(false, TEXT("FXRShot")); }, 2.0f, false);
}
