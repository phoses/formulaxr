#include "FXRPlayerController.h"

#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "Engine/GameInstance.h"
#include "FXRHudWidget.h"
#include "FXRModeSubsystem.h"
#include "FXRPlayerPawn.h"
#include "FXRRaceGameMode.h"
#include "FXRSlotCar.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	/** Full right stick deflection moves the track this fast, cm/s. */
	constexpr float TrackHeightSpeed = 30.0f;
	constexpr float StickDeadZone = 0.2f;
}

void AFXRPlayerController::CreateInputAssets()
{
	if (CommonContext)
	{
		return;
	}
	const TCHAR* ThrottleNames[2] = { TEXT("IA_ThrottleCar1"), TEXT("IA_ThrottleCar2") };
	for (int32 Car = 0; Car < 2; ++Car)
	{
		ThrottleActions[Car] = NewObject<UInputAction>(this, ThrottleNames[Car]);
		ThrottleActions[Car]->ValueType = EInputActionValueType::Axis1D;
	}
	TrackHeightAction = NewObject<UInputAction>(this, TEXT("IA_TrackHeight"));
	TrackHeightAction->ValueType = EInputActionValueType::Axis1D;
	RecenterAction = NewObject<UInputAction>(this, TEXT("IA_Recenter"));
	RecenterAction->ValueType = EInputActionValueType::Boolean;
	RestartAction = NewObject<UInputAction>(this, TEXT("IA_Restart"));
	RestartAction->ValueType = EInputActionValueType::Boolean;
	ToggleModeAction = NewObject<UInputAction>(this, TEXT("IA_ToggleMode"));
	ToggleModeAction->ValueType = EInputActionValueType::Boolean;

	CommonContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Common"));
	// Trigger dead zone and response curve live in the car tuning (FFXRSlotCarParams).
	CommonContext->MapKey(ThrottleActions[0], EKeys::Gamepad_LeftTriggerAxis);
	CommonContext->MapKey(ThrottleActions[1], EKeys::Gamepad_RightTriggerAxis);
	FEnhancedActionKeyMapping& Height = CommonContext->MapKey(TrackHeightAction, EKeys::Gamepad_RightY);
	UInputModifierDeadZone* DeadZone = NewObject<UInputModifierDeadZone>(CommonContext);
	DeadZone->LowerThreshold = StickDeadZone;
	DeadZone->Type = EDeadZoneType::Axial;
	Height.Modifiers.Add(DeadZone);
	CommonContext->MapKey(RestartAction, EKeys::Gamepad_Special_Right);   // Menu
	CommonContext->MapKey(ToggleModeAction, EKeys::Gamepad_Special_Left); // View

	MixedRealityContext = NewObject<UInputMappingContext>(this, TEXT("IMC_MR"));
	MixedRealityContext->MapKey(RecenterAction, EKeys::Gamepad_RightThumbstick);
}

void AFXRPlayerController::BeginPlay()
{
	Super::BeginPlay();
	CreateInputAssets();
	// Ask the mode subsystem, not the pawn: the pawn's BeginPlay may not have run yet.
	const UFXRModeSubsystem* Mode = GetGameInstance()->GetSubsystem<UFXRModeSubsystem>();
	const bool bMixedReality = Mode && Mode->IsMixedReality();
	if (UEnhancedInputLocalPlayerSubsystem* Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Input->AddMappingContext(CommonContext, 0);
		if (bMixedReality)
		{
			Input->AddMappingContext(MixedRealityContext, 1);
		}
	}

	// PC mode: race HUD on screen. Mixed reality shows it on the track's lap tower instead.
	if (!bMixedReality)
	{
		if (UFXRHudWidget* Hud = CreateWidget<UFXRHudWidget>(this, UFXRHudWidget::StaticClass()))
		{
			Hud->AddToViewport();
		}
	}

	FString Auto;
	if (FParse::Value(FCommandLine::Get(), TEXT("FXRAutoThrottle="), Auto, false))
	{
		TArray<FString> Parts;
		Auto.ParseIntoArray(Parts, TEXT(","));
		for (int32 Car = 0; Car < FMath::Min(Parts.Num(), 2); ++Car)
		{
			AutoThrottle[Car] = FCString::Atof(*Parts[Car]);
		}
		bAutoThrottle = true;
	}
}

void AFXRPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	CreateInputAssets();
	if (UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(InputComponent))
	{
		Enhanced->BindAction(TrackHeightAction, ETriggerEvent::Triggered, this, &AFXRPlayerController::OnTrackHeight);
		Enhanced->BindAction(RecenterAction, ETriggerEvent::Started, this, &AFXRPlayerController::OnRecenter);
		Enhanced->BindAction(RestartAction, ETriggerEvent::Started, this, &AFXRPlayerController::OnRestart);
		Enhanced->BindAction(ToggleModeAction, ETriggerEvent::Started, this, &AFXRPlayerController::OnToggleMode);
	}
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AFXRPlayerController::OnQuit);
}

AFXRSlotCar* AFXRPlayerController::FindCar(int32 Lane)
{
	if (!Cars[Lane].IsValid())
	{
		for (TActorIterator<AFXRSlotCar> It(GetWorld()); It; ++It)
		{
			if (It->GetLane() == Lane)
			{
				Cars[Lane] = *It;
				break;
			}
		}
	}
	return Cars[Lane].Get();
}

void AFXRPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	const UEnhancedPlayerInput* Input = Cast<UEnhancedPlayerInput>(PlayerInput);
	for (int32 Lane = 0; Lane < 2; ++Lane)
	{
		if (AFXRSlotCar* Car = FindCar(Lane))
		{
			const float Trigger = bAutoThrottle ? AutoThrottle[Lane]
				: Input ? Input->GetActionValue(ThrottleActions[Lane]).Get<float>() : 0.0f;
			Car->SetTrigger(Trigger);
		}
	}
}

void AFXRPlayerController::OnTrackHeight(const FInputActionValue& Value)
{
	if (AFXRPlayerPawn* FXRPawn = GetPawn<AFXRPlayerPawn>())
	{
		FXRPawn->AdjustTrackHeight(Value.Get<float>() * TrackHeightSpeed * GetWorld()->GetDeltaSeconds());
	}
}

void AFXRPlayerController::OnRecenter(const FInputActionValue& Value)
{
	if (AFXRPlayerPawn* FXRPawn = GetPawn<AFXRPlayerPawn>())
	{
		FXRPawn->RecenterTrack();
	}
}

void AFXRPlayerController::OnRestart(const FInputActionValue& Value)
{
	if (AFXRRaceGameMode* Game = GetWorld()->GetAuthGameMode<AFXRRaceGameMode>())
	{
		Game->RestartRace();
	}
}

void AFXRPlayerController::OnToggleMode(const FInputActionValue& Value)
{
	if (AFXRRaceGameMode* Game = GetWorld()->GetAuthGameMode<AFXRRaceGameMode>())
	{
		Game->ToggleMode();
	}
}

void AFXRPlayerController::OnQuit()
{
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}
