#include "FXRHudWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Framework/Application/SlateApplication.h"
#include "FXRRaceGameMode.h"
#include "FXRSlotCar.h"

namespace
{
	const FLinearColor CarColors[2] = { FLinearColor(1.0f, 0.32f, 0.27f), FLinearColor(0.35f, 0.6f, 1.0f) };
	const FLinearColor TextWhite(0.95f, 0.95f, 0.95f);
	const FLinearColor TextDim(0.65f, 0.65f, 0.65f);
	const FLinearColor LightRed(1.0f, 0.05f, 0.03f);
	const FLinearColor LightOff(0.08f, 0.08f, 0.08f);
	const FLinearColor Gold(1.0f, 0.8f, 0.2f);

	FString FormatTime(double Seconds)
	{
		if (Seconds <= 0.0)
		{
			return TEXT("--.---");
		}
		const int32 Minutes = FMath::FloorToInt32(Seconds / 60.0);
		const double Rest = Seconds - Minutes * 60.0;
		return Minutes > 0 ? FString::Printf(TEXT("%d:%06.3f"), Minutes, Rest) : FString::Printf(TEXT("%.3f"), Rest);
	}
}

TSharedRef<SWidget> UFXRHudWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (!WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

UTextBlock* UFXRHudWidget::MakeText(int32 Size, const FLinearColor& Color, bool bBold)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = Size;
	Font.TypefaceFontName = bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular"));
	Font.OutlineSettings.OutlineSize = 1;
	Font.OutlineSettings.OutlineColor = FLinearColor(0, 0, 0, 0.8f);
	Text->SetFont(Font);
	Text->SetColorAndOpacity(FSlateColor(Color));
	return Text;
}

UBorder* UFXRHudWidget::MakePanel(UVerticalBox*& OutContent)
{
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Panel->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
	Panel->SetPadding(FMargin(18.0f, 12.0f));
	OutContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(OutContent);
	return Panel;
}

void UFXRHudWidget::BuildTree()
{
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Canvas;

	auto Place = [Canvas](UWidget* Widget, FVector2D Anchor, FVector2D Offset)
	{
		UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Widget);
		CanvasSlot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
		CanvasSlot->SetAlignment(Anchor);
		CanvasSlot->SetPosition(Offset);
		CanvasSlot->SetAutoSize(true);
	};

	// Top centre: start lights, status, race clock.
	{
		UVerticalBox* Content;
		UBorder* Panel = MakePanel(Content);
		UHorizontalBox* LightRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		for (int32 Index = 0; Index < 5; ++Index)
		{
			UImage* Light = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			FSlateBrush Brush;
			Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
			Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
			Brush.ImageSize = FVector2D(34.0f, 34.0f);
			Light->SetBrush(Brush);
			Light->SetColorAndOpacity(LightOff);
			UHorizontalBoxSlot* LightSlot = LightRow->AddChildToHorizontalBox(Light);
			LightSlot->SetPadding(FMargin(5.0f, 0.0f));
			Lights[Index] = Light;
		}
		UVerticalBoxSlot* RowSlot = Content->AddChildToVerticalBox(LightRow);
		RowSlot->SetHorizontalAlignment(HAlign_Center);
		RowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));

		Status = MakeText(30, TextWhite, true);
		Content->AddChildToVerticalBox(Status)->SetHorizontalAlignment(HAlign_Center);
		Clock = MakeText(24, TextWhite);
		Content->AddChildToVerticalBox(Clock)->SetHorizontalAlignment(HAlign_Center);
		Place(Panel, FVector2D(0.5f, 0.0f), FVector2D(0.0f, 16.0f));
	}

	// Car panels: car 1 top left, car 2 top right.
	for (int32 Car = 0; Car < 2; ++Car)
	{
		UVerticalBox* Content;
		UBorder* Panel = MakePanel(Content);
		CarTitle[Car] = MakeText(26, CarColors[Car], true);
		CarTitle[Car]->SetText(FText::FromString(Car == 0 ? TEXT("CAR 1  (left trigger)") : TEXT("CAR 2  (right trigger)")));
		Content->AddChildToVerticalBox(CarTitle[Car]);
		CarLap[Car] = MakeText(24, TextWhite, true);
		Content->AddChildToVerticalBox(CarLap[Car]);
		CarTimes[Car] = MakeText(18, TextWhite);
		Content->AddChildToVerticalBox(CarTimes[Car]);
		CarState[Car] = MakeText(20, Gold, true);
		Content->AddChildToVerticalBox(CarState[Car]);
		Place(Panel, FVector2D(Car == 0 ? 0.0f : 1.0f, 0.0f), FVector2D(Car == 0 ? 16.0f : -16.0f, 16.0f));
	}

	// Results in the middle once the race is over.
	{
		UVerticalBox* Content;
		ResultsPanel = MakePanel(Content);
		Results = MakeText(28, TextWhite, true);
		Results->SetJustification(ETextJustify::Center);
		Content->AddChildToVerticalBox(Results)->SetHorizontalAlignment(HAlign_Center);
		Place(ResultsPanel, FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
	}

	// Bottom: controls.
	Hint = MakeText(16, TextDim);
	Place(Hint, FVector2D(0.5f, 1.0f), FVector2D(0.0f, -12.0f));

	// Controller missing: big prompt in the middle, above everything else.
	{
		UVerticalBox* Content;
		ControllerPrompt = MakePanel(Content);
		UTextBlock* Text = MakeText(30, Gold, true);
		Text->SetJustification(ETextJustify::Center);
		Text->SetText(FText::FromString(TEXT("CONNECT A CONTROLLER\nLeft trigger drives car 1, right trigger car 2")));
		Content->AddChildToVerticalBox(Text)->SetHorizontalAlignment(HAlign_Center);
		Place(ControllerPrompt, FVector2D(0.5f, 0.5f), FVector2D(0.0f, 120.0f));
		ControllerPrompt->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UFXRHudWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const AFXRRaceGameMode* Game = GetWorld() ? GetWorld()->GetAuthGameMode<AFXRRaceGameMode>() : nullptr;
	if (!Game || !Status)
	{
		return;
	}
	const FFXRRace& Race = Game->GetRace();
	const bool bRace = Race.Mode == EFXRRaceMode::Race;

	// Lights and status.
	const int32 Lit = Race.LightsLit();
	for (int32 Index = 0; Index < 5; ++Index)
	{
		Lights[Index]->SetColorAndOpacity(Index < Lit ? LightRed : LightOff);
		Lights[Index]->SetVisibility(bRace ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	FString StatusText;
	switch (Race.Phase)
	{
	case EFXRRacePhase::PreRace:   StatusText = TEXT("GET READY"); break;
	case EFXRRacePhase::Countdown: StatusText = TEXT("WAIT FOR LIGHTS OUT"); break;
	case EFXRRacePhase::Racing:
		StatusText = !bRace ? TEXT("PRACTICE") : Race.PhaseTime < 1.5 ? TEXT("GO!") : FString::Printf(TEXT("RACE  %d LAPS"), Race.TargetLaps);
		break;
	case EFXRRacePhase::Finished:  StatusText = TEXT("FINISHED"); break;
	}
	Status->SetText(FText::FromString(StatusText));
	Clock->SetText(FText::FromString(Race.CanDrive() ? FormatTime(Race.RaceTime()) : TEXT("0.000")));

	// Car panels.
	for (int32 Car = 0; Car < 2; ++Car)
	{
		const FFXRCarRaceState& State = Race.Cars[Car];
		const int32 OnLap = FMath::Min(State.LapsCompleted + 1, Race.TargetLaps);
		CarLap[Car]->SetText(FText::FromString(bRace
			? (State.bFinished ? FString::Printf(TEXT("%d / %d laps"), Race.TargetLaps, Race.TargetLaps) : FString::Printf(TEXT("Lap %d / %d"), OnLap, Race.TargetLaps))
			: FString::Printf(TEXT("Laps %d"), State.LapsCompleted)));
		CarTimes[Car]->SetText(FText::FromString(FString::Printf(TEXT("Lap %s   Last %s\nBest %s   Record %s\nDe-slots %d"),
			*FormatTime(State.CurrentLapTime(Race.Clock)), *FormatTime(State.LastLapTime),
			*FormatTime(State.BestLapTime), *FormatTime(Game->GetBestLapEver(Car)), State.Deslots)));

		FString CarStateText;
		const AFXRSlotCar* SlotCar = Game->GetCar(Car);
		if (State.bFinished)
		{
			CarStateText = FString::Printf(TEXT("FINISHED  %s"), *FormatTime(State.FinishTime));
		}
		else if (SlotCar && SlotCar->GetState() == EFXRSlotCarState::Deslotted)
		{
			CarStateText = TEXT("OFF!  marshal coming");
		}
		CarState[Car]->SetText(FText::FromString(CarStateText));
		CarState[Car]->SetVisibility(CarStateText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	// Results.
	if (Race.Phase == EFXRRacePhase::Finished)
	{
		FString Text = FString::Printf(TEXT("TOTAL  %s\n"), *FormatTime(Race.RaceTime()));
		Text += Game->IsNewRecord() ? TEXT("NEW RECORD!") : FString::Printf(TEXT("Record  %s"), *FormatTime(Game->GetRecordBefore()));
		Text += FString::Printf(TEXT("\nBest laps  %s  /  %s\n\nMenu: race again"),
			*FormatTime(Race.Cars[0].BestLapTime), *FormatTime(Race.Cars[1].BestLapTime));
		Results->SetText(FText::FromString(Text));
		ResultsPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		ResultsPanel->SetVisibility(ESlateVisibility::Collapsed);
	}

	const bool bGamepad = FSlateApplication::IsInitialized() && FSlateApplication::Get().IsGamepadAttached();
	ControllerPrompt->SetVisibility(bGamepad ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

	Hint->SetText(FText::FromString(bRace
		? TEXT("Menu: restart    View: practice    Right stick: track height")
		: TEXT("Menu: restart    View: race    Right stick: track height")));
}
