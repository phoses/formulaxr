#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FXRSlotCarModel.h"
#include "FXRSlotCarTuning.generated.h"

/**
 * Driving feel as an editable asset (docs/plan.md §4.2), so numbers change without recompiling.
 * Cars without an assigned asset use the defaults in FFXRSlotCarParams.
 */
UCLASS(BlueprintType)
class FORMULAXR_API UFXRSlotCarTuning : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tuning", meta = (ShowOnlyInnerProperties))
	FFXRSlotCarParams Params;
};
