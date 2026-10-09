#pragma once

#include "CoreMinimal.h"
#include "SoulsAttributeModifier.generated.h"

/** Editor-reflected operations, not a Blueprint mutation API. Percentages are fractions. */
UENUM()
enum class ESoulsAttributeModifierOperation : uint8
{
	Flat,
	AdditivePercent,
	Multiplicative
};

/** A value contribution, not an item or an effect instance. The caller retains its stable ID. */
struct SOULSNEW_API FSoulsAttributeModifier
{
	FGuid Id;
	ESoulsAttributeModifierOperation Operation = ESoulsAttributeModifierOperation::AdditivePercent;
	float Magnitude = 0.0f;
};
