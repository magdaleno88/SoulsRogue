#pragma once

#include "CoreMinimal.h"

/** C++-only operations. Percent magnitudes are fractions; multiplicative magnitudes are factors. */
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
