#include "FXRTrack.h"

#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"
#include "FXRHudWidget.h"
#include "DrawDebugHelpers.h"
#include "FXRModeSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogFXRTrack, Log, All);

static TAutoConsoleVariable<int32> CVarShowTrackDebug(
	TEXT("fxr.ShowTrackDebug"), 0, TEXT("1: draw both lanes and their crossing points."));

namespace FXRTrackLook
{
	const TCHAR* VertexColorMaterial = TEXT("/Game/FormulaXR/Materials/M_TrackVertexColor.M_TrackVertexColor");

	// Cross-section, as offsets from a slot or from the centreline, cm.
	constexpr float SlotHalfWidth = 0.2f;
	constexpr float BraidInner = 0.45f;    // braids: steel strips either side of the slot
	constexpr float BraidOuter = 0.95f;
	constexpr float EdgeMargin = 3.9f;     // track edge beyond each outer slot (gives a 15.6 cm wide track)
	constexpr float CurbWidth = 1.4f;
	constexpr float CurbBlock = 3.0f;
	constexpr float CheckerSize = 1.3f;
	constexpr float BoardMargin = 8.0f;
	constexpr float BoardThickness = 1.5f;
	constexpr float SampleStep = 1.0f;     // mesh resolution along the track, cm

	// Layer heights above the board so overlapping strips never z-fight, cm.
	constexpr float SurfaceZ = 0.0f;
	constexpr float CrossingSurfaceZ = 0.02f;   // straight B drawn over straight A at the crossover
	constexpr float CurbZ = 0.01f;
	constexpr float CheckerZ = 0.04f;
	constexpr float BraidZ = 0.05f;
	constexpr float SlotZ = 0.06f;

	const FLinearColor Asphalt(0.03f, 0.03f, 0.035f);
	const FLinearColor Slot(0.002f, 0.002f, 0.002f);
	const FLinearColor LaneColors[AFXRTrack::NumLanes] = { FLinearColor(0.65f, 0.03f, 0.02f), FLinearColor(0.02f, 0.10f, 0.75f) };
	const FLinearColor CurbRed(0.70f, 0.02f, 0.02f);
	const FLinearColor White(0.80f, 0.80f, 0.80f);
	const FLinearColor Black(0.01f, 0.01f, 0.01f);
	const FLinearColor Board(0.02f, 0.09f, 0.03f);
	const FLinearColor BoardEdge(0.08f, 0.05f, 0.03f);
}

namespace
{
	/** Collects one vertex-coloured mesh section. */
	struct FMeshBuilder
	{
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FLinearColor> Colors;

		/**
		 * Quad A-B-C-D. Its front face points up when A->B then B->C turns like +X then +Y (for a
		 * strip: along the travel direction, then towards its left normal). Unreal's front faces
		 * are clockwise seen from the front, hence the triangle order.
		 */
		void AddQuad(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& Normal, const FLinearColor& Color)
		{
			const int32 Base = Vertices.Num();
			for (const FVector& P : { A, B, C, D })
			{
				Vertices.Add(P);
				Normals.Add(Normal);
				UVs.Add(FVector2D(P.X, P.Y) * 0.01);
				Colors.Add(Color);
			}
			Triangles.Append({ Base, Base + 2, Base + 1, Base, Base + 3, Base + 2 });
		}

		/** A strip between two offsets from Path, from distance S0 to S1, at height Z, facing up. */
		void AddStrip(const FFXRFigure8Path& Path, double S0, double S1, float OffsetA, float OffsetB, float Z, const FLinearColor& Color)
		{
			// Lower offset first keeps every strip facing up, whichever side of the path it is on.
			if (OffsetA > OffsetB)
			{
				Swap(OffsetA, OffsetB);
			}
			const int32 Steps = FMath::Max(1, FMath::CeilToInt((S1 - S0) / FXRTrackLook::SampleStep));
			auto Edge = [&](double S, float Offset)
			{
				const FFXRPathSample Sample = Path.Sample(S);
				const FVector2D P = Sample.Location + Sample.LeftNormal() * Offset;
				return FVector(P.X, P.Y, Z);
			};
			for (int32 Step = 0; Step < Steps; ++Step)
			{
				const double A = FMath::Lerp(S0, S1, double(Step) / Steps);
				const double B = FMath::Lerp(S0, S1, double(Step + 1) / Steps);
				AddQuad(Edge(A, OffsetA), Edge(B, OffsetA), Edge(B, OffsetB), Edge(A, OffsetB), FVector::UpVector, Color);
			}
		}

		void Commit(UProceduralMeshComponent* Mesh, UMaterialInterface* Material) const
		{
			Mesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, TArray<FProcMeshTangent>(), false);
			if (Material)
			{
				Mesh->SetMaterial(0, Material);
			}
		}
	};
}

AFXRTrack::AFXRTrack()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	RootComponent = Root;

	TrackMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TrackMesh"));
	TrackMesh->SetupAttachment(Root);
	TrackMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	BoardMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("BoardMesh"));
	BoardMesh->SetupAttachment(Root);
	BoardMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoardMesh->ComponentTags.Add(FXR_PC_ONLY_TAG);

	SurfaceCollider = CreateDefaultSubobject<UBoxComponent>(TEXT("SurfaceCollider"));
	SurfaceCollider->SetupAttachment(Root);
	SurfaceCollider->SetCollisionProfileName(TEXT("BlockAll"));
	SurfaceCollider->SetHiddenInGame(true);
}

void AFXRTrack::BeginPlay()
{
	Super::BeginPlay();
	EnsureBuilt();
}

void AFXRTrack::EnsureBuilt()
{
	if (bBuilt)
	{
		return;
	}
	bBuilt = true;
	BuildPaths();
	BuildTrackMesh();
	BuildBoardMesh();
	// A 2 cm slab whose top is the board surface, wide enough to catch every crash in mixed reality,
	// where there is no virtual table under the board.
	SurfaceCollider->SetBoxExtent(FVector(HalfExtent.X + 60.0f, HalfExtent.Y + 60.0f, 1.0f));
	SurfaceCollider->SetRelativeLocation(FVector(0.0f, 0.0f, -1.0f));
	ApplyTransform();
}

void AFXRTrack::BuildPaths()
{
	FFXRFigure8Path::FParams Params;
	Params.LoopRadius = LoopRadius;
	Params.LoopCentreDistance = LoopCentreDistance;
	Centreline = FFXRFigure8Path(Params);

	Lanes.Reset();
	for (int32 Lane = 0; Lane < NumLanes; ++Lane)
	{
		Params.LaneOffset = (Lane == 0 ? -0.5f : 0.5f) * LaneSpacing;
		Lanes.Add(FFXRFigure8Path(Params));
	}
	LaneCrossings = FFXRFigure8Path::FindCrossings(Lanes[0], Lanes[1]);

	const float TrackHalfWidth = 0.5f * LaneSpacing + FXRTrackLook::EdgeMargin + FXRTrackLook::CurbWidth;
	HalfExtent = FVector2D(LoopCentreDistance + LoopRadius, LoopRadius) + FVector2D(TrackHalfWidth + FXRTrackLook::BoardMargin);

	UE_LOG(LogFXRTrack, Log, TEXT("Figure 8: lap %.1f cm (lane 0 %.3f, lane 1 %.3f), %d lane crossings, footprint %.0f x %.0f cm"),
		Centreline.GetLength(), Lanes[0].GetLength(), Lanes[1].GetLength(), LaneCrossings.Num(), 2 * HalfExtent.X, 2 * HalfExtent.Y);
}

void AFXRTrack::BuildTrackMesh()
{
	using namespace FXRTrackLook;
	FMeshBuilder Mesh;
	const float HalfWidth = 0.5f * LaneSpacing + EdgeMargin;
	const double Lap = Centreline.GetLength();

	// Surface. Straight B (segment 2) is drawn slightly higher so the crossover never z-fights.
	for (int32 Segment = 0; Segment < FFXRFigure8Path::NumSegments; ++Segment)
	{
		double S0, S1;
		Centreline.GetSegmentRange(Segment, S0, S1);
		Mesh.AddStrip(Centreline, S0, S1, -HalfWidth, HalfWidth, Segment == 2 ? CrossingSurfaceZ : SurfaceZ, Asphalt);
	}

	// Slots and braids follow each lane; drawn above the surface so both lanes show through the crossover.
	for (int32 Lane = 0; Lane < NumLanes; ++Lane)
	{
		const FFXRFigure8Path& Path = Lanes[Lane];
		const double LaneLap = Path.GetLength();
		Mesh.AddStrip(Path, 0.0, LaneLap, -SlotHalfWidth, SlotHalfWidth, SlotZ, Slot);
		Mesh.AddStrip(Path, 0.0, LaneLap, -BraidOuter, -BraidInner, BraidZ, LaneColors[Lane]);
		Mesh.AddStrip(Path, 0.0, LaneLap, BraidInner, BraidOuter, BraidZ, LaneColors[Lane]);
	}

	// Red/white curbs on the outside of each loop: left of travel on the clockwise right loop,
	// right of travel on the counter-clockwise left loop.
	for (const int32 Segment : { 1, 3 })
	{
		double S0, S1;
		Centreline.GetSegmentRange(Segment, S0, S1);
		const float Inner = Segment == 1 ? HalfWidth : -HalfWidth;
		const float Outer = Segment == 1 ? HalfWidth + CurbWidth : -HalfWidth - CurbWidth;
		// Blocks are measured on the curb's own radius so they look even.
		const double CurbRadius = LoopRadius + HalfWidth + 0.5f * CurbWidth;
		const int32 Blocks = FMath::Max(2, FMath::RoundToInt((S1 - S0) * CurbRadius / LoopRadius / CurbBlock));
		for (int32 Block = 0; Block < Blocks; ++Block)
		{
			const double A = FMath::Lerp(S0, S1, double(Block) / Blocks);
			const double B = FMath::Lerp(S0, S1, double(Block + 1) / Blocks);
			Mesh.AddStrip(Centreline, A, B, Inner, Outer, CurbZ, Block % 2 == 0 ? CurbRed : White);
		}
	}

	// Chequered start/finish line across the track.
	const int32 Columns = FMath::RoundToInt(2.0f * HalfWidth / CheckerSize);
	const float ColumnWidth = 2.0f * HalfWidth / Columns;
	for (int32 Row = 0; Row < 2; ++Row)
	{
		for (int32 Column = 0; Column < Columns; ++Column)
		{
			const float L0 = -HalfWidth + Column * ColumnWidth;
			Mesh.AddStrip(Centreline, StartLineDistance + Row * CheckerSize, StartLineDistance + (Row + 1) * CheckerSize,
				L0, L0 + ColumnWidth, CheckerZ, (Row + Column) % 2 == 0 ? White : Black);
		}
	}

	Mesh.Commit(TrackMesh, LoadObject<UMaterialInterface>(nullptr, VertexColorMaterial));
	UE_LOG(LogFXRTrack, Log, TEXT("Track mesh: %d triangles"), Mesh.Triangles.Num() / 3);
}

void AFXRTrack::BuildBoardMesh()
{
	using namespace FXRTrackLook;
	FMeshBuilder Mesh;
	const float X = HalfExtent.X, Y = HalfExtent.Y;
	const float Top = -0.01f, Bottom = -BoardThickness;
	Mesh.AddQuad(FVector(-X, -Y, Top), FVector(X, -Y, Top), FVector(X, Y, Top), FVector(-X, Y, Top), FVector::UpVector, Board);
	Mesh.AddQuad(FVector(-X, -Y, Bottom), FVector(X, -Y, Bottom), FVector(X, -Y, Top), FVector(-X, -Y, Top), -FVector::YAxisVector, BoardEdge);
	Mesh.AddQuad(FVector(X, Y, Bottom), FVector(-X, Y, Bottom), FVector(-X, Y, Top), FVector(X, Y, Top), FVector::YAxisVector, BoardEdge);
	Mesh.AddQuad(FVector(X, -Y, Bottom), FVector(X, Y, Bottom), FVector(X, Y, Top), FVector(X, -Y, Top), FVector::XAxisVector, BoardEdge);
	Mesh.AddQuad(FVector(-X, Y, Bottom), FVector(-X, -Y, Bottom), FVector(-X, -Y, Top), FVector(-X, Y, Top), -FVector::XAxisVector, BoardEdge);
	Mesh.Commit(BoardMesh, LoadObject<UMaterialInterface>(nullptr, VertexColorMaterial));
}

void AFXRTrack::ShowLapTower()
{
	if (LapTower)
	{
		return;
	}
	LapTower = NewObject<UWidgetComponent>(this, TEXT("LapTower"));
	LapTower->SetupAttachment(Root);
	LapTower->SetWidgetSpace(EWidgetSpace::World);
	LapTower->SetWidgetClass(UFXRHudWidget::StaticClass());
	LapTower->SetDrawSize(FVector2D(1600.0f, 700.0f));
	LapTower->SetBlendMode(EWidgetBlendMode::Transparent);
	LapTower->SetTwoSided(true);
	LapTower->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// After re-centering the player stands on the track's +Y side, so the tower stands on the -Y
	// side and faces +Y. 1600 px at 0.07 = 112 cm wide, bottom edge just above the board.
	constexpr float PixelScale = 0.07f;
	LapTower->SetRelativeScale3D(FVector(PixelScale));
	LapTower->SetRelativeLocation(FVector(0.0f, -HalfExtent.Y - 5.0f, 0.5f * 700.0f * PixelScale + 2.0f));
	LapTower->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
	LapTower->RegisterComponent();
}

FVector AFXRTrack::TrackToWorld(const FVector2D& TrackLocation, float Height) const
{
	return GetActorTransform().TransformPosition(FVector(TrackLocation.X, TrackLocation.Y, Height));
}

void AFXRTrack::AddHeightOffset(float DeltaCm)
{
	HeightOffset = FMath::Clamp(HeightOffset + DeltaCm, MinHeightOffset, MaxHeightOffset);
	ApplyTransform();
}

void AFXRTrack::PlaceAt(const FVector2D& InFloorXY, float InYaw)
{
	FloorXY = InFloorXY;
	Yaw = InYaw;
	ApplyTransform();
}

void AFXRTrack::ApplyTransform()
{
	SetActorLocationAndRotation(FVector(FloorXY.X, FloorXY.Y, DefaultHeight + HeightOffset), FRotator(0.0f, Yaw, 0.0f));
}

void AFXRTrack::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (CVarShowTrackDebug.GetValueOnGameThread() != 0)
	{
		DrawDebug();
	}
}

void AFXRTrack::DrawDebug() const
{
	UWorld* World = GetWorld();
	const FColor Colors[NumLanes] = { FColor::Red, FColor::Cyan };
	for (int32 Lane = 0; Lane < Lanes.Num(); ++Lane)
	{
		const FFXRFigure8Path& Path = Lanes[Lane];
		FVector Previous = TrackToWorld(Path.Sample(0.0).Location, 0.5f);
		for (double S = 2.0; S <= Path.GetLength() + 1.0; S += 2.0)
		{
			const FVector Next = TrackToWorld(Path.Sample(S).Location, 0.5f);
			DrawDebugLine(World, Previous, Next, Colors[Lane], false, -1.0f, SDPG_Foreground, 0.15f);
			Previous = Next;
		}
	}
	for (const FFXRPathCrossing& Crossing : LaneCrossings)
	{
		DrawDebugSphere(World, TrackToWorld(Crossing.Location, 0.5f), 1.0f, 12, FColor::Yellow, false, -1.0f, SDPG_Foreground, 0.1f);
	}
}
