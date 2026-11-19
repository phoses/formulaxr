#pragma once

#include "CoreMinimal.h"
#include "FXRFigure8Path.h"
#include "GameFramework/Actor.h"
#include "FXRTrack.generated.h"

class UBoxComponent;
class UProceduralMeshComponent;
class UWidgetComponent;

/**
 * The two-lane figure-8 slot track and everything that moves with it (docs/plan.md §4.1).
 * Its origin is the crossing at track surface level; geometry is generated from FFXRFigure8Path
 * so what you see always matches the lanes the cars follow.
 *
 * Console: fxr.ShowTrackDebug 1 draws both lanes and their crossing points.
 */
UCLASS()
class FORMULAXR_API AFXRTrack : public AActor
{
	GENERATED_BODY()

public:
	AFXRTrack();

	/** Track surface height above the floor before the player adjusts it (a table), cm. */
	static constexpr float DefaultHeight = 75.0f;
	static constexpr float MinHeightOffset = -50.0f;
	static constexpr float MaxHeightOffset = 150.0f;
	static constexpr int32 NumLanes = 2;

	/** Centreline loop radius, cm. */
	UPROPERTY(EditAnywhere, Category = "Track")
	float LoopRadius = 45.0f;

	/** Distance from the crossing to each loop centre, cm (1.6 x radius gives a ~77 degree crossing). */
	UPROPERTY(EditAnywhere, Category = "Track")
	float LoopCentreDistance = 72.0f;

	/** Centre-to-centre distance of the two slots, cm (standard 1:32). */
	UPROPERTY(EditAnywhere, Category = "Track")
	float LaneSpacing = 7.8f;

	/** Start/finish line, as a distance along the lanes from the crossing, cm. */
	UPROPERTY(EditAnywhere, Category = "Track")
	float StartLineDistance = 30.0f;

	/** Lane 0 (car 1, red) is to the right of travel, lane 1 (car 2, blue) to the left. */
	const FFXRFigure8Path& GetLane(int32 Lane) const { return Lanes[Lane]; }
	const FFXRFigure8Path& GetCentreline() const { return Centreline; }

	/** Where lane 0 crosses lane 1 (two points, both in the crossover). */
	const TArray<FFXRPathCrossing>& GetLaneCrossings() const { return LaneCrossings; }

	/** A point on the track plane (cm) to world space. */
	FVector TrackToWorld(const FVector2D& TrackLocation, float Height = 0.0f) const;

	/** Moves the track up or down, clamped to [MinHeightOffset, MaxHeightOffset]. */
	void AddHeightOffset(float DeltaCm);
	float GetHeightOffset() const { return HeightOffset; }

	/** Puts the track centre above a floor position, turned to Yaw; keeps the height offset. */
	void PlaceAt(const FVector2D& InFloorXY, float InYaw);

	/** Half size of the track's footprint including the board, cm. */
	FVector2D GetHalfExtent() const { return HalfExtent; }

	/** Where cars line up before the start: a little behind the start line, distance along the lane. */
	double GetGridDistance() const { return StartLineDistance - 8.0; }

	/** Builds paths and meshes now if BeginPlay hasn't yet (cars need the lanes when they spawn). */
	void EnsureBuilt();

	/** Mixed reality: the race HUD on a board standing behind the far side of the track, facing the player. */
	void ShowLapTower();

	/** Mixed reality: a soft shadow band along both track edges so the track sits on the real table. */
	void ShowGroundingShadow();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	/** Track surface, slots, braids, curbs and start line. */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProceduralMeshComponent> TrackMesh;

	/** Board under the track; PC only (in mixed reality the real table or floor is the board). */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProceduralMeshComponent> BoardMesh;

	/** Invisible slab under the track that de-slotted cars land on, in both modes. */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBoxComponent> SurfaceCollider;

	UPROPERTY()
	TObjectPtr<UWidgetComponent> LapTower;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> GroundingShadow;

	bool bBuilt = false;
	FFXRFigure8Path Centreline;
	TArray<FFXRFigure8Path> Lanes;
	TArray<FFXRPathCrossing> LaneCrossings;
	FVector2D HalfExtent = FVector2D::ZeroVector;

	float HeightOffset = 0.0f;
	FVector2D FloorXY = FVector2D::ZeroVector;
	float Yaw = 0.0f;

	void BuildPaths();
	void BuildTrackMesh();
	void BuildBoardMesh();
	void ApplyTransform();
	void DrawDebug() const;
};
