#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Attributes/SoulsAttributeModifier.h"
#include "SoulsItemDefinition.generated.h"

/** Only implemented target. Extend this schema when another attribute is authorized. */
UENUM()
enum class ESoulsItemAttribute : uint8
{
	MaxHealth
};

USTRUCT()
struct SOULSNEW_API FSoulsItemEffect
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category="Effect")
	ESoulsItemAttribute Attribute = ESoulsItemAttribute::MaxHealth;

	UPROPERTY(EditDefaultsOnly, Category="Effect")
	ESoulsAttributeModifierOperation Operation = ESoulsAttributeModifierOperation::AdditivePercent;

	UPROPERTY(EditDefaultsOnly, Category="Effect")
	float Magnitude = 0.0f;
};

/** Definition data only. No owner, grant/modifier IDs, health, timers or mutable execution state. */
USTRUCT()
struct SOULSNEW_API FSoulsItemDefinitionData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category="Identity")
	FName ItemId;

	UPROPERTY(EditDefaultsOnly, Category="Identity")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, Category="Identity", meta=(MultiLine=true))
	FText Description;

	UPROPERTY(EditDefaultsOnly, Category="Effects")
	TArray<FSoulsItemEffect> Effects;

	/** Categories may be expressed here. Metadata only in this phase. */
	UPROPERTY(EditDefaultsOnly, Category="Compatibility")
	FGameplayTagContainer ItemTags;

	/** Reserved schema; exclusivity is NOT enforced yet. */
	UPROPERTY(EditDefaultsOnly, Category="Compatibility")
	FGameplayTagContainer ExclusiveGroups;
};

/** Shared read-only runtime definition; editable Data Asset content, no owner or execution state. */
UCLASS(BlueprintType)
class SOULSNEW_API USoulsItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Copies definition data once into a transient UObject. Caller must retain a GC-safe reference. */
	static USoulsItemDefinition* CreateTransient(const FSoulsItemDefinitionData& InData);

	const FSoulsItemDefinitionData& GetData() const { return Data; }
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UFUNCTION(BlueprintPure, Category="SoulsNew|Items")
	FName GetItemId() const { return Data.ItemId; }

	UFUNCTION(BlueprintPure, Category="SoulsNew|Items")
	FText GetDisplayName() const { return Data.DisplayName; }

private:
	UPROPERTY(EditDefaultsOnly, Category="SoulsNew|Items", meta=(ShowOnlyInnerProperties))
	FSoulsItemDefinitionData Data;
};
