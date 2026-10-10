#if WITH_DEV_AUTOMATION_TESTS

#include "Attributes/SoulsHealthComponent.h"
#include "Characters/SoulsPrototypeBoss.h"
#include "Combat/SoulsMeleeCombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "Interaction/SoulsItemPickup.h"
#include "Interaction/SoulsRewardDropSpawnerComponent.h"
#include "Items/SoulsItemReward.h"
#include "Items/SoulsRewardComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "SoulsNewCharacter.h"
#include "Tests/AutomationCommon.h"
#include "UObject/Package.h"

namespace SoulsCombatMapTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter;
	constexpr const TCHAR* Map = TEXT("/Game/SoulsNew/Maps/Lvl_SoulsPrototype");
	class FAttackMapCommand : public IAutomationLatentCommand
	{
	public:
		FAttackMapCommand(FAutomationTestBase* InTest, bool bInKill) : Test(InTest), bKill(bInKill) {}
		virtual bool Update() override
		{
			if (FPlatformTime::Seconds() - Started > 60)
			{
				Test->AddError(TEXT("Timed out waiting for the authored map/player or world-time cooldown.")); return true;
			}
			UWorld* World = AutomationCommon::GetAnyGameWorld();
			if (!World || !World->HasBegunPlay()) { return false; }
			if (Attacks == 0)
			{
				auto* P = Cast<ASoulsNewCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0));
				if (!P || !P->HasActorBegunPlay()) { return false; }
				ASoulsPrototypeBoss* B = nullptr; int32 Count = 0;
				for (TActorIterator<ASoulsPrototypeBoss> It(World); It; ++It) { B = *It; ++Count; }
				if (!Test->TestEqual(TEXT("One authored boss"), Count, 1)) { return true; }
				if (!Test->TestTrue(TEXT("Official authored map"), World->GetOutermost()->GetName().Contains(TEXT("Lvl_SoulsPrototype")))) { return true; }
				Player = P; Boss = B;
				Test->TestEqual(TEXT("Authored boss initially 625"), B->GetSoulsHealth()->GetCurrentHealth(), 625.0f);
				Test->TestEqual(TEXT("Existing possessed real BP player"), P->GetClass()->GetPathName(), FString(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C")));
				Test->TestNotNull(TEXT("Real controller"), P->GetController());
				P->SetActorLocation(B->GetActorLocation() - FVector(175, 0, 0), false, nullptr, ETeleportType::TeleportPhysics);
				P->SetActorRotation(FRotator::ZeroRotator);
			}
			if (!Player.IsValid() || !Boss.IsValid()) { Test->AddError(TEXT("Map actors disappeared.")); return true; }
			if (World->GetTimeSeconds() < NextTime) { return false; }
			auto* Combat = Player->GetSoulsMeleeCombat();
			if (!Test->TestTrue(TEXT("Attack through the same component API as input"), Combat->TryLightAttack())) { return true; }
			++Attacks;
			const float Expected = FMath::Max(0.0f, 625.0f - 100.0f * Attacks);
			Test->TestEqual(TEXT("Actual sweep damage at each step"), Boss->GetSoulsHealth()->GetCurrentHealth(), Expected);
			Test->TestFalse(TEXT("Immediate input spam gated"), Combat->TryLightAttack());
			if (!bKill) { return true; }
			if (Attacks < 7) { NextTime = World->GetTimeSeconds() + Combat->GetLightAttackCooldown() + 0.001; return false; }
			auto* Reward = Boss->GetRewards()->GetReward();
			auto* Pickup = Boss->GetDropSpawner()->GetActivePickup();
			Test->TestTrue(TEXT("Death committed"), Boss->GetSoulsHealth()->IsDead());
			if (Test->TestNotNull(TEXT("Reward from combat death"), Reward) && Test->TestNotNull(TEXT("Automatic physical drop"), Pickup))
			{
				Test->TestTrue(TEXT("Pickup retains same reward"), Pickup->GetReward() == Reward);
				Test->TestTrue(TEXT("Original Vitality definition retained"), Reward->GetItemDefinition() == Boss->GetPrototypeDefinition());
				Test->TestTrue(TEXT("Available before pickup"), Reward->GetState() == ESoulsItemRewardState::Available);
				int32 Count = 0; for (TActorIterator<ASoulsItemPickup> It(World); It; ++It) { ++Count; }
				Test->TestEqual(TEXT("One physical reward"), Count, 1);
			}
			return true;
		}
	private:
		FAutomationTestBase* Test;
		bool bKill;
		double Started = FPlatformTime::Seconds();
		double NextTime = 0;
		int32 Attacks = 0;
		TWeakObjectPtr<ASoulsNewCharacter> Player;
		TWeakObjectPtr<ASoulsPrototypeBoss> Boss;
	};

	/** Simulated raw key: exercises installed context, action, Started binding and Character routing. */
	class FLeftMouseMapCommand : public IAutomationLatentCommand
	{
	public:
		explicit FLeftMouseMapCommand(FAutomationTestBase* InTest) : Test(InTest) {}
		virtual bool Update() override
		{
			if (FPlatformTime::Seconds() - Started > 60) { Test->AddError(TEXT("Timed out validating Enhanced Input.")); Release(); return true; }
			UWorld* World = AutomationCommon::GetAnyGameWorld();
			if (!World || !World->HasBegunPlay()) { return false; }
			if (Stage == 0)
			{
				auto* P = Cast<ASoulsNewCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0));
				if (!P || !P->GetController()) { return false; }
				auto* PC = Cast<APlayerController>(P->GetController());
				if (!PC || !PC->PlayerInput || !PC->GetLocalPlayer()) { return false; }
				ASoulsPrototypeBoss* B = nullptr; for (TActorIterator<ASoulsPrototypeBoss> It(World); It; ++It) { B = *It; break; }
				if (!Test->TestNotNull(TEXT("Authored boss"), B)) { return true; }
				Controller = PC; Boss = B;
				const auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
				if (!Test->TestNotNull(TEXT("Real Enhanced Input subsystem"), Subsystem)) { return true; }
				P->SetActorLocation(B->GetActorLocation() - FVector(175, 0, 0), false, nullptr, ETeleportType::TeleportPhysics);
				P->SetActorRotation(FRotator::ZeroRotator);
				Stage = 1; return false;
			}
			if (Stage == 1)
			{
				const FInputKeyEventArgs Key = FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton, IE_Pressed, 1.0f);
				Controller->InputKey(Key);
				Stage = 2; return false;
			}
			if (Boss->GetSoulsHealth()->GetCurrentHealth() == 625.0f) { return false; }
			Test->TestEqual(TEXT("Raw LeftMouseButton -> mapping -> Started -> component -> sweep"), Boss->GetSoulsHealth()->GetCurrentHealth(), 525.0f);
			Release(); return true;
		}
	private:
		void Release()
		{
			if (Controller.IsValid())
			{
				const FInputKeyEventArgs Key = FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton, IE_Released, 0.0f);
				Controller->InputKey(Key);
			}
		}
		FAutomationTestBase* Test;
		double Started = FPlatformTime::Seconds();
		int32 Stage = 0;
		TWeakObjectPtr<APlayerController> Controller;
		TWeakObjectPtr<ASoulsPrototypeBoss> Boss;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsMapLightAttack, "SoulsNew.ZCombatMap.A.RealMapLightAttack", SoulsCombatMapTests::Flags)
bool FSoulsMapLightAttack::RunTest(const FString&)
{
	if (!AutomationOpenMap(SoulsCombatMapTests::Map, true)) { AddError(TEXT("Unable to open official map.")); return false; }
	ADD_LATENT_AUTOMATION_COMMAND(SoulsCombatMapTests::FAttackMapCommand(this, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsMapCombatDeath, "SoulsNew.ZCombatMap.B.RealMapCombatDeath", SoulsCombatMapTests::Flags)
bool FSoulsMapCombatDeath::RunTest(const FString&)
{
	if (!AutomationOpenMap(SoulsCombatMapTests::Map, true)) { AddError(TEXT("Unable to open official map.")); return false; }
	ADD_LATENT_AUTOMATION_COMMAND(SoulsCombatMapTests::FAttackMapCommand(this, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsMapLeftMouseInput, "SoulsNew.ZCombatMap.C.LeftMouseEnhancedInput", SoulsCombatMapTests::Flags)
bool FSoulsMapLeftMouseInput::RunTest(const FString&)
{
	if (!AutomationOpenMap(SoulsCombatMapTests::Map, true)) { AddError(TEXT("Unable to open official map.")); return false; }
	ADD_LATENT_AUTOMATION_COMMAND(SoulsCombatMapTests::FLeftMouseMapCommand(this));
	return true;
}

#endif
