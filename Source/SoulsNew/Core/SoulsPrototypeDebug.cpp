#include "Core/SoulsPrototypeDebug.h"

#if !UE_BUILD_SHIPPING

#include "Attributes/SoulsHealthComponent.h"
#include "Characters/SoulsPrototypeBoss.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Interaction/SoulsItemPickup.h"
#include "Interaction/SoulsRewardDropSpawnerComponent.h"
#include "Items/SoulsItemComponent.h"
#include "Items/SoulsItemDefinition.h"
#include "Items/SoulsItemReward.h"
#include "Items/SoulsRewardComponent.h"
#include "SoulsNew.h"
#include "SoulsNewCharacter.h"

ASoulsPrototypeBoss* SoulsPrototypeDebug::FindLivePrototypeBoss(UWorld* World)
{
	check(IsInGameThread());
	if (!IsValid(World) || !World->IsGameWorld() || World->bIsTearingDown) { return nullptr; }
	ASoulsPrototypeBoss* Selected = nullptr;
	for (TActorIterator<ASoulsPrototypeBoss> It(World); It; ++It)
	{
		ASoulsPrototypeBoss* Boss = *It;
		if (IsValid(Boss) && !Boss->IsActorBeingDestroyed() && Boss->IsPrototypeReady() && !Boss->GetSoulsHealth()->IsDead()
			&& (!Selected || Boss->GetPathName().Compare(Selected->GetPathName(), ESearchCase::CaseSensitive) < 0)) { Selected = Boss; }
	}
	return Selected;
}

bool SoulsPrototypeDebug::KillPrototypeBoss(UWorld* World)
{
	ASoulsPrototypeBoss* Boss = FindLivePrototypeBoss(World);
	if (!Boss) { return false; }
	USoulsHealthComponent* Health = Boss->GetSoulsHealth();
	// Only public health damage; its normal Death observers generate and materialize the reward.
	return Health->TakeDamage(Health->GetCurrentHealth()) > 0.0f;
}

void SoulsPrototypeDebug::LogStatus(UWorld* World)
{
	check(IsInGameThread());
	if (!IsValid(World) || !World->IsGameWorld())
	{
		UE_LOG(LogSoulsNew, Display, TEXT("[SoulsPrototype] Status requires an active Game/PIE world."));
		return;
	}
	for (TActorIterator<ASoulsNewCharacter> It(World); It; ++It)
	{
		const auto* Player = *It;
		const auto* Health = Player->GetSoulsHealth();
		UE_LOG(LogSoulsNew, Display, TEXT("[SoulsPrototype] Player %s Health=%g/%g Base=%g Grants=%d"),
			*Player->GetName(), Health->GetCurrentHealth(), Health->GetMaxHealth(), Health->GetBaseMaxHealth(), Player->GetSoulsItems()->GetGrantCount());
	}
	for (TActorIterator<ASoulsPrototypeBoss> It(World); It; ++It)
	{
		const auto* Boss = *It;
		const auto* Health = Boss->GetSoulsHealth();
		USoulsItemReward* Reward = Boss->GetRewards()->GetReward();
		ASoulsItemPickup* Pickup = Boss->GetDropSpawner()->GetActivePickup();
		UE_LOG(LogSoulsNew, Display, TEXT("[SoulsPrototype] Boss %s Health=%g/%g Base=%g %s Ready=%d Grants=%d Reward=%s RewardState=%s DropState=%s Pickup=%s"),
			*Boss->GetName(), Health->GetCurrentHealth(), Health->GetMaxHealth(), Health->GetBaseMaxHealth(),
			Health->IsDead() ? TEXT("Dead") : TEXT("Alive"), Boss->IsPrototypeReady(), Boss->GetSoulsItems()->GetGrantCount(),
			Reward ? *Reward->GetRewardId().ToString() : TEXT("None"),
			Reward ? (Reward->GetState() == ESoulsItemRewardState::Available ? TEXT("Available") : TEXT("Claimed")) : TEXT("None"),
			*StaticEnum<ESoulsRewardDropState>()->GetNameStringByValue(static_cast<int64>(Boss->GetDropSpawner()->GetDropState())),
			*GetNameSafe(Pickup));
	}
}

namespace
{
	FAutoConsoleCommandWithWorld KillCommand(TEXT("Souls.Debug.KillPrototypeBoss"),
		TEXT("Development prototype: kill the ready live boss with the smallest actor path via Health.TakeDamage."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (!SoulsPrototypeDebug::KillPrototypeBoss(World))
			{
				UE_LOG(LogSoulsNew, Display, TEXT("[SoulsPrototype] No ready live prototype boss found in this Game/PIE world."));
			}
		}), ECVF_Cheat);
	FAutoConsoleCommandWithWorld StatusCommand(TEXT("Souls.Debug.Status"),
		TEXT("Development prototype: log ThirdPerson player health/grants and prototype boss reward/drop state."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&SoulsPrototypeDebug::LogStatus), ECVF_Cheat);
}

#endif
