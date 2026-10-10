#if WITH_DEV_AUTOMATION_TESTS

#include "Characters/SoulsPrototypeBoss.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "Items/SoulsItemDefinition.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

namespace SoulsPrototypeMapTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter;
	class FLoadedMap
	{
	public:
		explicit FLoadedMap(UWorld* InWorld) : World(InWorld) {}
		~FLoadedMap()
		{
			// Editor asset loading initializes inactive-world subsystems. Finish their
			// lifetime before a later GC test can collect them in arbitrary order.
			// A map currently used by Game/PIE/Editor remains owned by that session.
			if (World.IsValid() && World->WorldType == EWorldType::Inactive && World->IsInitialized())
			{
				World->CleanupWorld();
			}
		}
		bool IsValid() const { return World.IsValid(); }
		UWorld* Get() const { return World.Get(); }
		UWorld* operator->() const { return World.Get(); }
	private:
		TStrongObjectPtr<UWorld> World;
	};
	// Inspect the authored package without starting gameplay or saving its world.
	FLoadedMap Load(FAutomationTestBase& Test)
	{
		UWorld* World = LoadObject<UWorld>(nullptr, TEXT("/Game/SoulsNew/Maps/Lvl_SoulsPrototype.Lvl_SoulsPrototype"));
		Test.TestNotNull(TEXT("Official prototype map loads"), World);
		if (World) { Test.TestNotNull(TEXT("Persistent level exists"), World->PersistentLevel.Get()); }
		return FLoadedMap(World);
	}
	TArray<ASoulsPrototypeBoss*> Bosses(UWorld* World)
	{
		TArray<ASoulsPrototypeBoss*> Result;
		for (AActor* Actor : World->PersistentLevel->Actors)
		{
			if (auto* Boss = Cast<ASoulsPrototypeBoss>(Actor)) { Result.Add(Boss); }
		}
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPrototypeMapLoads, "SoulsNew.PrototypeMap.A.PrototypeMapLoads", SoulsPrototypeMapTests::Flags)
bool FSoulsPrototypeMapLoads::RunTest(const FString& Parameters)
{
	auto World = SoulsPrototypeMapTests::Load(*this);
	return World.IsValid() && World->PersistentLevel;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPrototypeMapSingleBoss, "SoulsNew.PrototypeMap.B.ContainsSinglePrototypeBoss", SoulsPrototypeMapTests::Flags)
bool FSoulsPrototypeMapSingleBoss::RunTest(const FString& Parameters)
{
	auto World = SoulsPrototypeMapTests::Load(*this);
	if (!World.IsValid() || !World->PersistentLevel) { return false; }
	TestEqual(TEXT("Exactly one authored prototype boss"), SoulsPrototypeMapTests::Bosses(World.Get()).Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPrototypeMapVitality, "SoulsNew.PrototypeMap.C.BossUsesRealVitalityAsset", SoulsPrototypeMapTests::Flags)
bool FSoulsPrototypeMapVitality::RunTest(const FString& Parameters)
{
	auto World = SoulsPrototypeMapTests::Load(*this);
	if (!World.IsValid() || !World->PersistentLevel) { return false; }
	auto Bosses = SoulsPrototypeMapTests::Bosses(World.Get());
	if (!TestEqual(TEXT("Single authored boss"), Bosses.Num(), 1)) { return false; }
	auto* Vitality = LoadObject<USoulsItemDefinition>(nullptr, TEXT("/Game/SoulsNew/Items/DA_Item_Vitality.DA_Item_Vitality"));
	if (!TestNotNull(TEXT("Existing persistent Vitality asset"), Vitality)) { return false; }
	TestTrue(TEXT("Boss retains the exact real definition"), Bosses[0]->GetPrototypeDefinition() == Vitality);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPrototypeMapConfiguration, "SoulsNew.PrototypeMap.D.MapPlayableConfiguration", SoulsPrototypeMapTests::Flags)
bool FSoulsPrototypeMapConfiguration::RunTest(const FString& Parameters)
{
	auto World = SoulsPrototypeMapTests::Load(*this);
	if (!World.IsValid() || !World->PersistentLevel) { return false; }
	int32 Starts = 0;
	for (AActor* Actor : World->PersistentLevel->Actors) { if (IsValid(Cast<APlayerStart>(Actor))) { ++Starts; } }
	TestTrue(TEXT("Authored PlayerStart exists"), Starts > 0);
	const auto* Settings = World->GetWorldSettings();
	if (!TestNotNull(TEXT("World Settings exists"), Settings)) { return false; }
	auto* ModeClass = LoadClass<AGameModeBase>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode.BP_ThirdPersonGameMode_C"));
	if (!TestNotNull(TEXT("Existing ThirdPerson GameMode"), ModeClass)) { return false; }
	TestTrue(TEXT("Map overrides with existing ThirdPerson GameMode"), Settings->DefaultGameMode.Get() == ModeClass);
	const auto* Mode = ModeClass->GetDefaultObject<AGameModeBase>();
	auto* PawnClass = LoadClass<APawn>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
	auto* ControllerClass = LoadClass<APlayerController>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.BP_ThirdPersonPlayerController_C"));
	if (!TestNotNull(TEXT("Existing player Blueprint"), PawnClass) || !TestNotNull(TEXT("Existing controller Blueprint"), ControllerClass)) { return false; }
	TestTrue(TEXT("Existing real player configured"), Mode->DefaultPawnClass.Get() == PawnClass);
	TestTrue(TEXT("Existing real controller configured"), Mode->PlayerControllerClass.Get() == ControllerClass);
	return true;
}

#endif
