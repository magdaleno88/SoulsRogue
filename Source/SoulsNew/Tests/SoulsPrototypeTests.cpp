#include "Characters/SoulsPrototypeBoss.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Attributes/SoulsHealthComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/SoulsPrototypeDebug.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/IConsoleManager.h"
#include "Interaction/SoulsItemPickup.h"
#include "Interaction/SoulsRewardDropSpawnerComponent.h"
#include "Items/SoulsItemComponent.h"
#include "Items/SoulsItemDefinition.h"
#include "Items/SoulsItemReward.h"
#include "Items/SoulsRewardComponent.h"
#include "Misc/AutomationTest.h"
#include "SoulsNewCharacter.h"

namespace SoulsPrototypeTests
{
	// Blueprint classes are existing uncooked project content; the native parent remains abstract.
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	class FFixture
	{
	public:
		UWorld* World = nullptr;
		explicit FFixture(bool bBeginPlay = true)
		{
			const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(true).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			if (World && GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			if (World && bBeginPlay) { BeginPlay(); }
		}
		void BeginPlay()
		{
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
			World->GetWorldSettings()->NotifyBeginPlay();
		}
		~FFixture()
		{
			if (World)
			{
				if (World->HasBegunPlay()) { World->EndPlay(EEndPlayReason::Quit); }
				World->DestroyWorld(false);
				if (GEngine) { GEngine->DestroyWorldContext(World); }
			}
		}
		ASoulsNewCharacter* Player(FAutomationTestBase& Test)
		{
			UClass* Class = LoadClass<ASoulsNewCharacter>(nullptr,
				TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
			if (!Test.TestNotNull(TEXT("Existing ThirdPerson Blueprint class"), Class)) { return nullptr; }
			Test.TestTrue(TEXT("Resolved real C++ parent"), Class->GetSuperClass() == ASoulsNewCharacter::StaticClass());
			FActorSpawnParameters Parameters;
			Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ASoulsNewCharacter* Actor = World->SpawnActor<ASoulsNewCharacter>(Class, FVector(3000, 0, 100), FRotator::ZeroRotator, Parameters);
			Test.TestNotNull(TEXT("Real ThirdPerson player instance"), Actor);
			return Actor;
		}
		ASoulsPrototypeBoss* Boss(FAutomationTestBase& Test, FName Name = NAME_None, bool bAssignDefinition = true, USoulsItemDefinition* Definition = nullptr)
		{
			FActorSpawnParameters Parameters;
			Parameters.Name = Name;
			Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Parameters.bDeferConstruction = true;
			auto* Actor = World->SpawnActor<ASoulsPrototypeBoss>(FVector(1000, 0, 100), FRotator::ZeroRotator, Parameters);
			Test.TestNotNull(TEXT("Prototype boss instance"), Actor);
			if (!Actor) { return nullptr; }
			if (bAssignDefinition)
			{
				if (!Definition) { Definition = LoadObject<USoulsItemDefinition>(nullptr, TEXT("/Game/SoulsNew/Items/DA_Item_Vitality.DA_Item_Vitality")); }
				if (!Test.TestNotNull(TEXT("Configured definition asset"), Definition)) { Actor->Destroy(); return nullptr; }
				Test.TestTrue(TEXT("Configure before BeginPlay"), Actor->SetPrototypeItemDefinition(Definition));
			}
			Actor->FinishSpawning(FTransform(FVector(1000, 0, 100)));
			return Actor;
		}
	};
	void Components(FAutomationTestBase& Test, ASoulsNewCharacter* Player)
	{
		TArray<USoulsHealthComponent*> Health;
		TArray<USoulsItemComponent*> Items;
		Player->GetComponents(Health);
		Player->GetComponents(Items);
		Test.TestEqual(TEXT("Exactly one health"), Health.Num(), 1);
		Test.TestEqual(TEXT("Exactly one item component"), Items.Num(), 1);
		Test.TestTrue(TEXT("Health is native default subobject"), Player->GetSoulsHealth()->HasAnyFlags(RF_DefaultSubObject));
		Test.TestTrue(TEXT("Items are native default subobject"), Player->GetSoulsItems()->HasAnyFlags(RF_DefaultSubObject));
	}
	ASoulsItemPickup* Kill(FAutomationTestBase& Test, ASoulsPrototypeBoss* Boss)
	{
		if (!Test.TestTrue(TEXT("Ready before damage"), Boss->IsPrototypeReady())) { return nullptr; }
		Boss->GetSoulsHealth()->TakeDamage(Boss->GetSoulsHealth()->GetCurrentHealth());
		ASoulsItemPickup* Pickup = Boss->GetDropSpawner()->GetActivePickup();
		Test.TestNotNull(TEXT("Automatic pickup"), Pickup);
		return Pickup;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPrototypeATest, "SoulsNew.Prototype.A.RealPlayerClassComponents", SoulsPrototypeTests::Flags)
bool FSoulsPrototypeATest::RunTest(const FString& Parameters)
{
	const ASoulsNewCharacter* Defaults = GetDefault<ASoulsNewCharacter>();
	TestNotNull(TEXT("Native parent health"), Defaults->GetSoulsHealth());
	TestNotNull(TEXT("Native parent items"), Defaults->GetSoulsItems());
	TestEqual(TEXT("Native base 100"), Defaults->GetSoulsHealth()->GetBaseMaxHealth(), 100.0f);
	SoulsPrototypeTests::FFixture F;
	ASoulsNewCharacter* Player = F.Player(*this);
	if (!Player) { return false; }
	SoulsPrototypeTests::Components(*this, Player);
	TestEqual(TEXT("Real player base 100"), Player->GetSoulsHealth()->GetBaseMaxHealth(), 100.0f);
	TestEqual(TEXT("Real player max 100"), Player->GetSoulsHealth()->GetMaxHealth(), 100.0f);
	TestEqual(TEXT("Real player current 100"), Player->GetSoulsHealth()->GetCurrentHealth(), 100.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPrototypeBTest, "SoulsNew.Prototype.B.PlayerExistingFunctionality", SoulsPrototypeTests::Flags)
bool FSoulsPrototypeBTest::RunTest(const FString& Parameters)
{
	SoulsPrototypeTests::FFixture F;
	auto* Player = F.Player(*this);
	if (!Player) { return false; }
	TestNotNull(TEXT("CharacterMovement retained"), Player->GetCharacterMovement());
	TestNotNull(TEXT("Capsule retained"), Player->GetCapsuleComponent());
	TestNotNull(TEXT("SpringArm retained"), Player->GetCameraBoom());
	TestNotNull(TEXT("Camera retained"), Player->GetFollowCamera());
	TestNotNull(TEXT("Existing skeletal mesh retained"), Player->GetMesh()->GetSkeletalMeshAsset());
	TestNotNull(TEXT("Existing anim instance retained"), Player->GetMesh()->GetAnimInstance());
	TestTrue(TEXT("Camera still on boom"), Player->GetFollowCamera()->GetAttachParent() == Player->GetCameraBoom());
	TestTrue(TEXT("Movement still controls capsule"), Player->GetCharacterMovement()->UpdatedComponent == Player->GetCapsuleComponent());
	TestTrue(TEXT("Positive configured walk speed"), Player->GetCharacterMovement()->MaxWalkSpeed > 0.0f);
	Player->DoJumpStart();
	TestTrue(TEXT("Jump request preserved"), Player->bPressedJump);
	Player->DoJumpEnd();
	TestFalse(TEXT("Stop jump preserved"), Player->bPressedJump);
	TestTrue(TEXT("Existing capsule already permits pickup overlaps"), Player->GetCapsuleComponent()->GetGenerateOverlapEvents());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPrototypeCTest, "SoulsNew.Prototype.C.PrototypeBossInitialization", SoulsPrototypeTests::Flags)
bool FSoulsPrototypeCTest::RunTest(const FString& Parameters)
{
	SoulsPrototypeTests::FFixture F;
	auto* Boss = F.Boss(*this);
	if (!Boss) { return false; }
	TestTrue(TEXT("Ready"), Boss->IsPrototypeReady());
	TestEqual(TEXT("Boss base 500"), Boss->GetSoulsHealth()->GetBaseMaxHealth(), 500.0f);
	TestEqual(TEXT("Boss modified max 625"), Boss->GetSoulsHealth()->GetMaxHealth(), 625.0f);
	TestEqual(TEXT("Boss full 625"), Boss->GetSoulsHealth()->GetCurrentHealth(), 625.0f);
	TestEqual(TEXT("One actual grant"), Boss->GetSoulsItems()->GetGrantCount(), 1);
	TestTrue(TEXT("Source grant is returned item grant"), Boss->GetRewards()->GetSourceGrantId() == Boss->GetPrototypeGrantId());
	TestNotNull(TEXT("Visible existing engine cube"), Boss->GetPrototypeMesh()->GetStaticMesh().Get());
	TestTrue(TEXT("Visible before death"), Boss->GetPrototypeMesh()->IsVisible());
	TestTrue(TEXT("Physical collision before death"), Boss->GetActorEnableCollision());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPrototypeDTest, "SoulsNew.Prototype.D.PrototypeBossSameDefinition", SoulsPrototypeTests::Flags)
bool FSoulsPrototypeDTest::RunTest(const FString& Parameters)
{
	SoulsPrototypeTests::FFixture F;
	auto* Boss = F.Boss(*this);
	if (!Boss) { return false; }
	auto* Definition = Boss->GetPrototypeDefinition();
	TestFalse(TEXT("Configured persistent definition asset"), Definition->HasAnyFlags(RF_Transient));
	TestTrue(TEXT("Source grant holds exact prototype definition"), Boss->GetSoulsItems()->GetGrantDefinition(Boss->GetPrototypeGrantId()) == Definition);
	if (!SoulsPrototypeTests::Kill(*this, Boss)) { return false; }
	TestTrue(TEXT("Reward captures exact definition"), Boss->GetRewards()->GetReward()->GetItemDefinition() == Definition);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPrototypeETest, "SoulsNew.Prototype.E.PrototypeBossDeath", SoulsPrototypeTests::Flags)
bool FSoulsPrototypeETest::RunTest(const FString& Parameters)
{
	SoulsPrototypeTests::FFixture F;
	auto* Boss = F.Boss(*this);
	if (!Boss) { return false; }
	int32 Deaths = 0;
	Boss->GetSoulsHealth()->Death().AddLambda([&](USoulsHealthComponent*) { ++Deaths; });
	if (!SoulsPrototypeTests::Kill(*this, Boss)) { return false; }
	Boss->GetSoulsHealth()->TakeDamage(1000);
	TestEqual(TEXT("One normal health death"), Deaths, 1);
	TestEqual(TEXT("Current health zero"), Boss->GetSoulsHealth()->GetCurrentHealth(), 0.0f);
	TestTrue(TEXT("Reward generated"), Boss->GetRewards()->HasHandledDeath());
	TestFalse(TEXT("No source destruction"), Boss->IsActorBeingDestroyed());
	TestFalse(TEXT("Death hides visual"), Boss->GetPrototypeMesh()->IsVisible());
	TestFalse(TEXT("Death disables collision"), Boss->GetActorEnableCollision());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPrototypeFTest, "SoulsNew.Prototype.F.PrototypeBossAutomaticDrop", SoulsPrototypeTests::Flags)
bool FSoulsPrototypeFTest::RunTest(const FString& Parameters)
{
	SoulsPrototypeTests::FFixture F;
	auto* Boss = F.Boss(*this);
	if (!Boss) { return false; }
	ASoulsItemPickup* Pickup = SoulsPrototypeTests::Kill(*this, Boss);
	if (!Pickup) { return false; }
	TestTrue(TEXT("Normal spawner active"), Boss->GetDropSpawner()->GetDropState() == ESoulsRewardDropState::PickupActive);
	TestTrue(TEXT("Existing pickup class"), Pickup->GetClass() == ASoulsItemPickup::StaticClass());
	TestTrue(TEXT("Same logical reward"), Pickup->GetReward() == Boss->GetRewards()->GetReward());
	TestTrue(TEXT("Drop beside boss above floor"), Pickup->GetActorLocation().Equals(FVector(1160, 0, 45), 0.001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPrototypeGTest, "SoulsNew.Prototype.G.PlayerClaimsDrop", SoulsPrototypeTests::Flags)
bool FSoulsPrototypeGTest::RunTest(const FString& Parameters)
{
	SoulsPrototypeTests::FFixture F;
	auto* Player = F.Player(*this);
	auto* Boss = F.Boss(*this);
	if (!Player || !Boss) { return false; }
	auto* Pickup = SoulsPrototypeTests::Kill(*this, Boss);
	if (!Pickup) { return false; }
	FGuid Grant;
	TestTrue(TEXT("Claim through normal pickup API"), Pickup->TryClaimForActor(Player, Grant) == ESoulsPickupClaimResult::Success);
	TestEqual(TEXT("Player effective 125"), Player->GetSoulsHealth()->GetMaxHealth(), 125.0f);
	TestEqual(TEXT("Player current 125"), Player->GetSoulsHealth()->GetCurrentHealth(), 125.0f);
	TestEqual(TEXT("Player base stays 100"), Player->GetSoulsHealth()->GetBaseMaxHealth(), 100.0f);
	TestEqual(TEXT("Exactly one grant"), Player->GetSoulsItems()->GetGrantCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPrototypeHTest, "SoulsNew.Prototype.H.SameDefinitionEndToEnd", SoulsPrototypeTests::Flags)
bool FSoulsPrototypeHTest::RunTest(const FString& Parameters)
{
	SoulsPrototypeTests::FFixture F;
	auto* Player = F.Player(*this);
	auto* Boss = F.Boss(*this);
	if (!Player || !Boss) { return false; }
	auto* Pickup = SoulsPrototypeTests::Kill(*this, Boss);
	if (!Pickup) { return false; }
	auto* Definition = Boss->GetSoulsItems()->GetGrantDefinition(Boss->GetPrototypeGrantId());
	auto* Reward = Boss->GetRewards()->GetReward();
	TestTrue(TEXT("Boss == Reward definition"), Definition == Reward->GetItemDefinition());
	TestTrue(TEXT("Reward == Pickup Reward"), Pickup->GetReward() == Reward);
	FGuid Grant;
	if (!TestTrue(TEXT("Claim"), Pickup->TryClaimForActor(Player, Grant) == ESoulsPickupClaimResult::Success)) { return false; }
	TestTrue(TEXT("Pickup == Player definition"), Player->GetSoulsItems()->GetGrantDefinition(Grant) == Definition);
	TestTrue(TEXT("New grant, same definition"), Grant != Boss->GetPrototypeGrantId());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPrototypeITest, "SoulsNew.Prototype.I.PlayerBossIndependence", SoulsPrototypeTests::Flags)
bool FSoulsPrototypeITest::RunTest(const FString& Parameters)
{
	SoulsPrototypeTests::FFixture F;
	auto* Player = F.Player(*this);
	auto* Boss = F.Boss(*this);
	if (!Player || !Boss) { return false; }
	auto* Pickup = SoulsPrototypeTests::Kill(*this, Boss);
	if (!Pickup) { return false; }
	FGuid Grant;
	if (!TestTrue(TEXT("Claim"), Pickup->TryClaimForActor(Player, Grant) == ESoulsPickupClaimResult::Success)) { return false; }
	TestTrue(TEXT("Remove source grant"), Boss->GetSoulsItems()->RemoveItem(Boss->GetPrototypeGrantId()) == ESoulsItemResult::Success);
	TestEqual(TEXT("Boss max back to 500"), Boss->GetSoulsHealth()->GetMaxHealth(), 500.0f);
	TestEqual(TEXT("Player remains 125"), Player->GetSoulsHealth()->GetMaxHealth(), 125.0f);
	TestTrue(TEXT("Player grant retained"), Player->GetSoulsItems()->HasGrant(Grant));
	TestTrue(TEXT("Change source base"), Boss->GetSoulsHealth()->SetBaseMaxHealth(700));
	TestEqual(TEXT("Player base independent"), Player->GetSoulsHealth()->GetBaseMaxHealth(), 100.0f);
	TestEqual(TEXT("Player max independent"), Player->GetSoulsHealth()->GetMaxHealth(), 125.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPrototypeJTest, "SoulsNew.Prototype.J.NoDuplicatePlayerComponents", SoulsPrototypeTests::Flags)
bool FSoulsPrototypeJTest::RunTest(const FString& Parameters)
{
	SoulsPrototypeTests::FFixture F;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		auto* Player = F.Player(*this);
		if (!Player) { return false; }
		SoulsPrototypeTests::Components(*this, Player);
#if WITH_EDITOR
		Player->RerunConstructionScripts();
		Player->RerunConstructionScripts();
#endif
		SoulsPrototypeTests::Components(*this, Player);
		TestEqual(TEXT("No grant on reconstruction"), Player->GetSoulsItems()->GetGrantCount(), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPrototypeKTest, "SoulsNew.Prototype.K.GenericSystemsRemainGeneric", SoulsPrototypeTests::Flags)
bool FSoulsPrototypeKTest::RunTest(const FString& Parameters)
{
	SoulsPrototypeTests::FFixture F;
	auto* Boss = F.Boss(*this);
	if (!Boss) { return false; }
	auto* Generic = F.World->SpawnActor<AActor>();
	auto* Health = NewObject<USoulsHealthComponent>(Generic);
	Generic->AddInstanceComponent(Health);
	Health->RegisterComponent();
	auto* Items = NewObject<USoulsItemComponent>(Generic);
	Generic->AddInstanceComponent(Items);
	Items->RegisterComponent();
	auto* Pickup = SoulsPrototypeTests::Kill(*this, Boss);
	if (!Pickup) { return false; }
	FGuid Grant;
	TestTrue(TEXT("Generic AActor still collects prototype drop"), Pickup->TryClaimForActor(Generic, Grant) == ESoulsPickupClaimResult::Success);
	TestEqual(TEXT("Generic same percentage"), Health->GetMaxHealth(), 125.0f);
	TestTrue(TEXT("Generic same definition"), Items->GetGrantDefinition(Grant) == Boss->GetPrototypeDefinition());
	return true;
}

#if !UE_BUILD_SHIPPING
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPrototypeLTest, "SoulsNew.Prototype.L.DebugKillUsesHealth", SoulsPrototypeTests::Flags)
bool FSoulsPrototypeLTest::RunTest(const FString& Parameters)
{
	SoulsPrototypeTests::FFixture F;
	auto* Boss = F.Boss(*this);
	if (!Boss) { return false; }
	int32 HealthChanges = 0, Deaths = 0, Rewards = 0, Drops = 0;
	Boss->GetSoulsHealth()->HealthChanged().AddLambda([&](USoulsHealthComponent*, float Old, float New)
	{
		++HealthChanges;
		TestEqual(TEXT("Health started at 625"), Old, 625.0f);
		TestEqual(TEXT("Damage changed actual health to zero"), New, 0.0f);
	});
	Boss->GetSoulsHealth()->Death().AddLambda([&](USoulsHealthComponent*) { ++Deaths; });
	Boss->GetRewards()->RewardGenerated().AddLambda([&](USoulsRewardComponent*, USoulsItemReward*) { ++Rewards; });
	Boss->GetDropSpawner()->PickupSpawned().AddLambda([&](USoulsRewardDropSpawnerComponent*, ASoulsItemPickup*, USoulsItemReward*) { ++Drops; });
	TestNotNull(TEXT("Debug kill command registered"), IConsoleManager::Get().FindConsoleObject(TEXT("Souls.Debug.KillPrototypeBoss")));
	TestNotNull(TEXT("Debug status command registered"), IConsoleManager::Get().FindConsoleObject(TEXT("Souls.Debug.Status")));
	TestTrue(TEXT("Execute real registered console command"), IConsoleManager::Get().ProcessUserConsoleInput(TEXT("Souls.Debug.KillPrototypeBoss"), *GLog, F.World));
	TestEqual(TEXT("Actual health transition"), HealthChanges, 1);
	TestEqual(TEXT("Normal death"), Deaths, 1);
	TestEqual(TEXT("Normal reward"), Rewards, 1);
	TestEqual(TEXT("Normal automatic drop"), Drops, 1);
	TestFalse(TEXT("Dead boss cannot be killed again"), SoulsPrototypeDebug::KillPrototypeBoss(F.World));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPrototypeSelectionTest, "SoulsNew.Prototype.DebugDeterministicSelection", SoulsPrototypeTests::Flags)
bool FSoulsPrototypeSelectionTest::RunTest(const FString& Parameters)
{
	SoulsPrototypeTests::FFixture F;
	auto* B = F.Boss(*this, TEXT("Prototype_B"));
	auto* A = F.Boss(*this, TEXT("Prototype_A"));
	if (!A || !B) { return false; }
	TestTrue(TEXT("Stable actor path selection independent of spawn order"), SoulsPrototypeDebug::FindLivePrototypeBoss(F.World) == A);
	TestTrue(TEXT("First debug kill"), SoulsPrototypeDebug::KillPrototypeBoss(F.World));
	TestTrue(TEXT("First selected boss dead"), A->GetSoulsHealth()->IsDead());
	TestFalse(TEXT("Other boss unchanged"), B->GetSoulsHealth()->IsDead());
	TestTrue(TEXT("Next live boss selected"), SoulsPrototypeDebug::FindLivePrototypeBoss(F.World) == B);
	TestFalse(TEXT("No world rejected"), SoulsPrototypeDebug::KillPrototypeBoss(nullptr));
	return true;
}
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsDataAssetATest, "SoulsNew.DataAssets.A.BossWithoutDefinition", SoulsPrototypeTests::Flags)
bool FSoulsDataAssetATest::RunTest(const FString& Parameters)
{
	AddExpectedMessagePlain(TEXT("PrototypeItemDefinition is missing"), ELogVerbosity::Warning);
	SoulsPrototypeTests::FFixture F;
	auto* Boss = F.Boss(*this, NAME_None, false);
	if (!Boss) { return false; }
	TestEqual(TEXT("Unconfigured base"), Boss->GetSoulsHealth()->GetBaseMaxHealth(), 500.0f);
	TestEqual(TEXT("Unconfigured max"), Boss->GetSoulsHealth()->GetMaxHealth(), 500.0f);
	TestEqual(TEXT("Unconfigured current"), Boss->GetSoulsHealth()->GetCurrentHealth(), 500.0f);
	TestEqual(TEXT("No grant"), Boss->GetSoulsItems()->GetGrantCount(), 0);
	TestFalse(TEXT("No source grant"), Boss->GetRewards()->GetSourceGrantId().IsValid());
	TestFalse(TEXT("Not ready for debug kill"), Boss->IsPrototypeReady());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsDataAssetBTest, "SoulsNew.DataAssets.B.BossWithDataDefinition", SoulsPrototypeTests::Flags)
bool FSoulsDataAssetBTest::RunTest(const FString& Parameters)
{
	SoulsPrototypeTests::FFixture F;
	auto* Boss = F.Boss(*this);
	if (!Boss) { return false; }
	TestEqual(TEXT("Base"), Boss->GetSoulsHealth()->GetBaseMaxHealth(), 500.0f);
	TestEqual(TEXT("Data-driven max"), Boss->GetSoulsHealth()->GetMaxHealth(), 625.0f);
	TestTrue(TEXT("Same configured definition in grant"), Boss->GetSoulsItems()->GetGrantDefinition(Boss->GetPrototypeGrantId()) == Boss->GetPrototypeDefinition());
	TestTrue(TEXT("Persistent asset"), !Boss->GetPrototypeDefinition()->HasAnyFlags(RF_Transient));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsDataAssetCTest, "SoulsNew.DataAssets.C.NoRuntimeVitalityConstruction", SoulsPrototypeTests::Flags)
bool FSoulsDataAssetCTest::RunTest(const FString& Parameters)
{
	AddExpectedMessagePlain(TEXT("PrototypeItemDefinition is missing"), ELogVerbosity::Warning);
	SoulsPrototypeTests::FFixture F;
	auto* Boss = F.Boss(*this, NAME_None, false);
	if (!Boss) { return false; }
	TestNull(TEXT("No synthesized definition"), Boss->GetPrototypeDefinition());
	TestFalse(TEXT("No synthesized grant id"), Boss->GetPrototypeGrantId().IsValid());
	Boss->GetSoulsHealth()->TakeDamage(500.0f);
	TestNull(TEXT("No synthesized reward"), Boss->GetRewards()->GetReward());
	TestNull(TEXT("No fallback pickup"), Boss->GetDropSpawner()->GetActivePickup());
	TestEqual(TEXT("No effect even after death"), Boss->GetSoulsHealth()->GetMaxHealth(), 500.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsDataAssetDTest, "SoulsNew.DataAssets.D.SameDefinitionEndToEnd", SoulsPrototypeTests::Flags)
bool FSoulsDataAssetDTest::RunTest(const FString& Parameters)
{
	SoulsPrototypeTests::FFixture F;
	auto* Player = F.Player(*this);
	auto* Boss = F.Boss(*this);
	if (!Player || !Boss) { return false; }
	auto* Definition = Boss->GetPrototypeDefinition();
	auto* Pickup = SoulsPrototypeTests::Kill(*this, Boss);
	if (!Pickup) { return false; }
	auto* Reward = Pickup->GetReward();
	TestTrue(TEXT("Boss grant == configured asset"), Boss->GetSoulsItems()->GetGrantDefinition(Boss->GetPrototypeGrantId()) == Definition);
	TestTrue(TEXT("Reward == configured asset"), Reward->GetItemDefinition() == Definition);
	FGuid PlayerGrant;
	Pickup->PickupClaimed().AddLambda([&](ASoulsItemPickup*, AActor*, FGuid Grant) { PlayerGrant = Grant; });
	F.World->Tick(LEVELTICK_All, 1.0f / 60.0f);
	Player->SetActorLocation(Pickup->GetActorLocation(), false, nullptr, ETeleportType::TeleportPhysics);
	Player->GetCapsuleComponent()->UpdateOverlaps();
	TestTrue(TEXT("Real overlap claims asset"), PlayerGrant.IsValid());
	TestTrue(TEXT("Player grant == configured asset"), Player->GetSoulsItems()->GetGrantDefinition(PlayerGrant) == Definition);
	TestTrue(TEXT("Reward claimed"), Reward->GetState() == ESoulsItemRewardState::Claimed);
	TestTrue(TEXT("Pickup consumed"), Pickup->IsConsumed() && Pickup->IsActorBeingDestroyed());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsDataAssetETest, "SoulsNew.DataAssets.E.PlayerResult", SoulsPrototypeTests::Flags)
bool FSoulsDataAssetETest::RunTest(const FString& Parameters)
{
	SoulsPrototypeTests::FFixture F;
	auto* Player = F.Player(*this);
	auto* Boss = F.Boss(*this);
	if (!Player || !Boss) { return false; }
	auto* Pickup = SoulsPrototypeTests::Kill(*this, Boss);
	if (!Pickup) { return false; }
	FGuid Grant;
	TestTrue(TEXT("Normal pickup claim"), Pickup->TryClaimForActor(Player, Grant) == ESoulsPickupClaimResult::Success);
	TestEqual(TEXT("Base preserved"), Player->GetSoulsHealth()->GetBaseMaxHealth(), 100.0f);
	TestEqual(TEXT("Max"), Player->GetSoulsHealth()->GetMaxHealth(), 125.0f);
	TestEqual(TEXT("Current follows PreservePercentage"), Player->GetSoulsHealth()->GetCurrentHealth(), 125.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsDataAssetFTest, "SoulsNew.DataAssets.F.ExistingCoreRegression", SoulsPrototypeTests::Flags)
bool FSoulsDataAssetFTest::RunTest(const FString& Parameters)
{
	SoulsPrototypeTests::FFixture F;
	auto* Player = F.Player(*this);
	if (!Player) { return false; }
	auto* Definition = LoadObject<USoulsItemDefinition>(nullptr, TEXT("/Game/SoulsNew/Items/DA_Item_Vitality.DA_Item_Vitality"));
	FGuid First, Second;
	if (!TestTrue(TEXT("First data asset grant"), Player->GetSoulsItems()->GrantItem(Definition, First) == ESoulsItemResult::Success)) { return false; }
	Player->GetSoulsHealth()->TakeDamage(62.5f);
	TestTrue(TEXT("Second independent grant"), Player->GetSoulsItems()->GrantItem(Definition, Second) == ESoulsItemResult::Success);
	TestEqual(TEXT("Existing additive stacking"), Player->GetSoulsHealth()->GetMaxHealth(), 150.0f);
	TestEqual(TEXT("Existing PreservePercentage"), Player->GetSoulsHealth()->GetCurrentHealth(), 75.0f);
	TestTrue(TEXT("Remove first"), Player->GetSoulsItems()->RemoveItem(First) == ESoulsItemResult::Success);
	TestEqual(TEXT("Independent second effect"), Player->GetSoulsHealth()->GetMaxHealth(), 125.0f);
	TestTrue(TEXT("Remove second"), Player->GetSoulsItems()->RemoveItem(Second) == ESoulsItemResult::Success);
	TestEqual(TEXT("Return to base"), Player->GetSoulsHealth()->GetMaxHealth(), 100.0f);
	TestEqual(TEXT("Percentage preserved on withdrawal"), Player->GetSoulsHealth()->GetCurrentHealth(), 50.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsDataAssetGTest, "SoulsNew.DataAssets.G.RealVitalityAssetLoads", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FSoulsDataAssetGTest::RunTest(const FString& Parameters)
{
	auto* Definition = LoadObject<USoulsItemDefinition>(nullptr, TEXT("/Game/SoulsNew/Items/DA_Item_Vitality.DA_Item_Vitality"));
	if (!TestNotNull(TEXT("Real content asset loads"), Definition)) { return false; }
	TestFalse(TEXT("Not transient"), Definition->HasAnyFlags(RF_Transient));
	const auto& Data = Definition->GetData();
	TestTrue(TEXT("Logical ID"), Data.ItemId == FName(TEXT("Item.Vitality")));
	TestEqual(TEXT("Display name"), Data.DisplayName.ToString(), FString(TEXT("Vitalidad")));
	TestFalse(TEXT("Description authored"), Data.Description.IsEmpty());
	if (!TestEqual(TEXT("Exactly one effect"), Data.Effects.Num(), 1)) { return false; }
	TestTrue(TEXT("MaxHealth"), Data.Effects[0].Attribute == ESoulsItemAttribute::MaxHealth);
	TestTrue(TEXT("AdditivePercent"), Data.Effects[0].Operation == ESoulsAttributeModifierOperation::AdditivePercent);
	TestEqual(TEXT("Exactly 0.25 (rejects 25, 1.25, 0.025)"), Data.Effects[0].Magnitude, 0.25f);
	TestTrue(TEXT("No invented tags"), Data.ItemTags.IsEmpty() && Data.ExclusiveGroups.IsEmpty());
	TestTrue(TEXT("Logical primary id"), Definition->GetPrimaryAssetId() == FPrimaryAssetId(FPrimaryAssetType(TEXT("SoulsItem")), TEXT("Item.Vitality")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsDataAssetHTest, "SoulsNew.DataAssets.H.ConfigurationBeforePlayOnly", SoulsPrototypeTests::Flags)
bool FSoulsDataAssetHTest::RunTest(const FString& Parameters)
{
	SoulsPrototypeTests::FFixture F;
	auto* Boss = F.Boss(*this);
	if (!Boss) { return false; }
	auto* Definition = Boss->GetPrototypeDefinition();
	TestFalse(TEXT("No runtime configuration replacement"), Boss->SetPrototypeItemDefinition(nullptr));
	TestTrue(TEXT("Configured definition unchanged"), Boss->GetPrototypeDefinition() == Definition);
	TestEqual(TEXT("Grant unchanged"), Boss->GetSoulsItems()->GetGrantCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsDataAssetITest, "SoulsNew.DataAssets.I.BossUsesArbitraryDefinition", SoulsPrototypeTests::Flags)
bool FSoulsDataAssetITest::RunTest(const FString& Parameters)
{
	SoulsPrototypeTests::FFixture F;
	FSoulsItemDefinitionData Data;
	Data.ItemId = TEXT("Test.FlatHealth");
	Data.DisplayName = FText::FromString(TEXT("Test flat health"));
	FSoulsItemEffect Effect;
	Effect.Attribute = ESoulsItemAttribute::MaxHealth;
	Effect.Operation = ESoulsAttributeModifierOperation::Flat;
	Effect.Magnitude = 10.0f;
	Data.Effects.Add(Effect);
	auto* Definition = USoulsItemDefinition::CreateTransient(Data); // Test input only; boss never creates definitions.
	auto* Boss = F.Boss(*this, NAME_None, true, Definition);
	if (!Boss) { return false; }
	TestEqual(TEXT("Generic configured effect, no hidden 25 percent"), Boss->GetSoulsHealth()->GetMaxHealth(), 510.0f);
	TestTrue(TEXT("Exact arbitrary definition retained"), Boss->GetPrototypeDefinition() == Definition);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPrototypePhysicalTest, "SoulsNew.Prototype.EndToEndRealPlayerOverlap", SoulsPrototypeTests::Flags)
bool FSoulsPrototypePhysicalTest::RunTest(const FString& Parameters)
{
	SoulsPrototypeTests::FFixture F;
	auto* Player = F.Player(*this);
	auto* Boss = F.Boss(*this);
	if (!Player || !Boss) { return false; }
	TestEqual(TEXT("Real player initially 100"), Player->GetSoulsHealth()->GetMaxHealth(), 100.0f);
	TestEqual(TEXT("Boss base initially 500"), Boss->GetSoulsHealth()->GetBaseMaxHealth(), 500.0f);
	TestEqual(TEXT("Boss effective initially 625"), Boss->GetSoulsHealth()->GetMaxHealth(), 625.0f);
	auto* Pickup = SoulsPrototypeTests::Kill(*this, Boss);
	if (!Pickup) { return false; }
	auto* Reward = Boss->GetRewards()->GetReward();
	int32 Claims = 0;
	FGuid Grant;
	Pickup->PickupClaimed().AddLambda([&](ASoulsItemPickup*, AActor* Target, FGuid Id)
	{
		++Claims;
		Grant = Id;
		TestTrue(TEXT("Actual overlap target is real player"), Target == Player);
	});
	F.World->Tick(LEVELTICK_All, 1.0f / 60.0f);
	Player->SetActorLocation(Pickup->GetActorLocation(), false, nullptr, ETeleportType::TeleportPhysics);
	Player->GetCapsuleComponent()->UpdateOverlaps(); // Normal engine query, no direct Claim or synthetic event.
	TestEqual(TEXT("Physical overlap claimed exactly once"), Claims, 1);
	TestEqual(TEXT("Real player effective 125"), Player->GetSoulsHealth()->GetMaxHealth(), 125.0f);
	TestEqual(TEXT("Real player base unchanged 100"), Player->GetSoulsHealth()->GetBaseMaxHealth(), 100.0f);
	TestTrue(TEXT("Reward claimed"), Reward->GetState() == ESoulsItemRewardState::Claimed);
	TestTrue(TEXT("Physical pickup consumed/destroyed"), Pickup->IsConsumed() && Pickup->IsActorBeingDestroyed());
	TestTrue(TEXT("Same definition boss to real player"), Player->GetSoulsItems()->GetGrantDefinition(Grant) == Boss->GetPrototypeDefinition());
	TestTrue(TEXT("Same reward captured into pickup"), Pickup->GetReward() == Reward);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPrototypeBeforePlayTest, "SoulsNew.Prototype.BaseBeforeBeginPlay", SoulsPrototypeTests::Flags)
bool FSoulsPrototypeBeforePlayTest::RunTest(const FString& Parameters)
{
	SoulsPrototypeTests::FFixture F(false);
	auto* Boss = F.Boss(*this);
	if (!Boss) { return false; }
	TestEqual(TEXT("Actual base before item grant"), Boss->GetSoulsHealth()->GetBaseMaxHealth(), 500.0f);
	TestEqual(TEXT("Actual max before item grant"), Boss->GetSoulsHealth()->GetMaxHealth(), 500.0f);
	TestEqual(TEXT("No item before BeginPlay"), Boss->GetSoulsItems()->GetGrantCount(), 0);
	TestFalse(TEXT("Not ready before initialization"), Boss->IsPrototypeReady());
	F.BeginPlay();
	TestEqual(TEXT("Normal BeginPlay applies data-driven item"), Boss->GetSoulsHealth()->GetMaxHealth(), 625.0f);
	TestTrue(TEXT("Ready after initialization"), Boss->IsPrototypeReady());
	return true;
}

#endif
