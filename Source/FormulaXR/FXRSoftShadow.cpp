#include "FXRSoftShadow.h"

#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

UMaterialInterface* FXRSoftShadow::LoadMaterial()
{
	return LoadObject<UMaterialInterface>(nullptr, MaterialPath);
}

void FXRSoftShadow::ConfigureComponent(UProceduralMeshComponent* Mesh)
{
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->SetTranslucentSortPriority(1);
}

void FXRSoftShadow::BuildDisc(UProceduralMeshComponent* Mesh, int32 Segments)
{
	// Centre, an inner ring that is still nearly full strength, and a rim that fades to 0.
	const struct { float Radius; float Alpha; } Rings[] = { { 0.55f, 0.85f }, { 1.0f, 0.0f } };
	TArray<FVector> Vertices = { FVector::ZeroVector };
	TArray<FLinearColor> Colors = { FLinearColor(0, 0, 0, 1.0f) };
	for (const auto& Ring : Rings)
	{
		for (int32 Index = 0; Index < Segments; ++Index)
		{
			const float Angle = 2.0f * UE_PI * Index / Segments;
			Vertices.Add(FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Ring.Radius);
			Colors.Add(FLinearColor(0, 0, 0, Ring.Alpha));
		}
	}
	// Facing up: Unreal's front faces are clockwise seen from the front (see FXRTrack's mesh builder).
	TArray<int32> Triangles;
	for (int32 Index = 0; Index < Segments; ++Index)
	{
		const int32 Next = (Index + 1) % Segments;
		const int32 InnerA = 1 + Index, InnerB = 1 + Next;
		const int32 OuterA = 1 + Segments + Index, OuterB = 1 + Segments + Next;
		Triangles.Append({ 0, InnerB, InnerA });
		Triangles.Append({ InnerA, InnerB, OuterB, InnerA, OuterB, OuterA });
	}
	TArray<FVector> Normals;
	Normals.Init(FVector::UpVector, Vertices.Num());
	TArray<FVector2D> UVs;
	UVs.Init(FVector2D::ZeroVector, Vertices.Num());
	Mesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, TArray<FProcMeshTangent>(), false);
}
