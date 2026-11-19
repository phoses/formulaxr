#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "FXRPlayerPawn.generated.h"

class AFXRTrack;
class UCameraComponent;

/**
 * The player's view. PC mode: a fixed camera looking at the track. Mixed reality: the headset
 * camera at a floor-level tracking origin, with Quest passthrough behind the scene.
 */
UCLASS()
class FORMULAXR_API AFXRPlayerPawn : public APawn
{
	GENERATED_BODY()

public:
	AFXRPlayerPawn();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

	bool IsMixedReality() const { return bMixedReality; }

	/** Right stick vertical: moves the track up or down. */
	void AdjustTrackHeight(float DeltaCm);

	/** Mixed reality: puts the track in front of the headset, facing the player. */
	void RecenterTrack();

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> TrackingOrigin;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> Camera;

	TWeakObjectPtr<AFXRTrack> Track;
	bool bMixedReality = false;
	bool bPlaced = false;

	AFXRTrack* FindTrack();
	void SetupPC();
	void SetupMixedReality();
	/** Hide the PC room; show the lap tower, track grounding shadow and car soft shadows. */
	void ShowMixedRealityLook();
	void SpawnPreviewFloor();
	void ShowPassthrough(bool bVisible);
};
