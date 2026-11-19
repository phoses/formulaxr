#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UProceduralMeshComponent;

/**
 * Soft shadows for mixed reality grounding (docs/plan.md §4.6). Unreal has no shadow catcher, so
 * shadows on the real table are drawn as meshes with M_SoftShadow: black, translucent, opacity =
 * vertex alpha x Strength, fading to 0 at the edges.
 */
namespace FXRSoftShadow
{
	const TCHAR* const MaterialPath = TEXT("/Game/FormulaXR/Materials/M_SoftShadow.M_SoftShadow");

	/** Loads M_SoftShadow (null if missing: run Scripts/create_shadow_material.py). */
	UMaterialInterface* LoadMaterial();

	/** A unit-radius disc in the XY plane: dark in the middle, fading to nothing at the rim. Scale it to size. */
	void BuildDisc(UProceduralMeshComponent* Mesh, int32 Segments = 32);

	/** Common setup for a shadow mesh component: no collision, casts no shadow, drawn after the track. */
	void ConfigureComponent(UProceduralMeshComponent* Mesh);
}
