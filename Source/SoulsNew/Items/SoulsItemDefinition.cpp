#include "Items/SoulsItemDefinition.h"

#include "UObject/Package.h"

USoulsItemDefinition* USoulsItemDefinition::CreateTransient(const FSoulsItemDefinitionData& InData)
{
	check(IsInGameThread());
	USoulsItemDefinition* Definition = NewObject<USoulsItemDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
	Definition->Data = InData;
	return Definition;
}

FPrimaryAssetId USoulsItemDefinition::GetPrimaryAssetId() const
{
	return Data.ItemId.IsNone() ? FPrimaryAssetId() : FPrimaryAssetId(FPrimaryAssetType(TEXT("SoulsItem")), Data.ItemId);
}
