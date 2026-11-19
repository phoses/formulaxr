#include "FXRSaveGame.h"

#include "Kismet/GameplayStatics.h"

namespace
{
	const TCHAR* SlotName = TEXT("FormulaXR");
}

UFXRSaveGame* UFXRSaveGame::Load()
{
	if (UFXRSaveGame* Existing = Cast<UFXRSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0)))
	{
		return Existing;
	}
	return Cast<UFXRSaveGame>(UGameplayStatics::CreateSaveGameObject(UFXRSaveGame::StaticClass()));
}

void UFXRSaveGame::Update(TFunctionRef<void(UFXRSaveGame&)> Change)
{
	UFXRSaveGame* Save = Load();
	Change(*Save);
	UGameplayStatics::SaveGameToSlot(Save, SlotName, 0);
}
