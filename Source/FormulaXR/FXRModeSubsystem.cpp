#include "FXRModeSubsystem.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HeadMountedDisplayFunctionLibrary.h"

bool UFXRModeSubsystem::IsMixedReality() const
{
	return GEngine && GEngine->XRSystem.IsValid() && UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled();
}

void UFXRModeSubsystem::ApplyModeVisibility(UWorld* World) const
{
	if (!World)
	{
		return;
	}
	const bool bHide = IsMixedReality();
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor->ActorHasTag(FXR_PC_ONLY_TAG))
		{
			Actor->SetActorHiddenInGame(bHide);
			continue;
		}
		TInlineComponentArray<UPrimitiveComponent*> Primitives(Actor);
		for (UPrimitiveComponent* Primitive : Primitives)
		{
			if (Primitive->ComponentHasTag(FXR_PC_ONLY_TAG))
			{
				Primitive->SetHiddenInGame(bHide);
			}
		}
	}
}
