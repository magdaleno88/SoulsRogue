#pragma once

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING
class UWorld;
class ASoulsPrototypeBoss;

/** Development-only console adapter. No reward, drop or effect mutations. */
namespace SoulsPrototypeDebug
{
	SOULSNEW_API ASoulsPrototypeBoss* FindLivePrototypeBoss(UWorld* World);
	SOULSNEW_API bool KillPrototypeBoss(UWorld* World);
	SOULSNEW_API void LogStatus(UWorld* World);
}
#endif
