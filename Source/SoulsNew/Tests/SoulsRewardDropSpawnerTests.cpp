#include "Interaction/SoulsRewardDropSpawnerComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Attributes/SoulsHealthComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/SoulsItemPickup.h"
#include "Items/SoulsItemComponent.h"
#include "Items/SoulsItemDefinition.h"
#include "Items/SoulsItemReward.h"
#include "Items/SoulsRewardComponent.h"
#include "Misc/AutomationTest.h"
#include "UObject/GarbageCollection.h"
#include "UObject/StrongObjectPtr.h"

namespace SoulsDropTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::EngineFilter;
	struct FOwner
	{
		AActor* Actor = nullptr;
		USoulsHealthComponent* Health = nullptr;
		USoulsItemComponent* Items = nullptr;
		USoulsRewardComponent* Rewards = nullptr;
	};
	class FFixture
	{
	public:
		UWorld* World = nullptr;
		TStrongObjectPtr<USoulsItemDefinition> Definition;
		FOwner Source;
		USoulsRewardDropSpawnerComponent* Drop = nullptr;
		FGuid SourceGrant;
		explicit FFixture(bool bPhysics = false)
		{
			FSoulsItemDefinitionData Data;
			Data.ItemId = TEXT("Item.Vitality");
			Data.DisplayName = FText::FromString(TEXT("Vitalidad"));
			FSoulsItemEffect Effect;
			Effect.Operation = ESoulsAttributeModifierOperation::AdditivePercent;
			Effect.Magnitude = 0.25f;
			Data.Effects.Add(Effect);
			Definition.Reset(USoulsItemDefinition::CreateTransient(Data));
			const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(bPhysics)
				.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(bPhysics).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			if (World && GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			if (World && bPhysics)
			{
				World->InitializeActorsForPlay(FURL());
				World->BeginPlay();
				World->GetWorldSettings()->NotifyBeginPlay();
			}
			Source = MakeOwner(500.0f, true);
			if (Source.Actor)
			{
				Source.Actor->SetActorLocation(FVector(1000, 0, 0));
				Drop = AddDrop(Source.Actor);
			}
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
		USoulsRewardDropSpawnerComponent* AddDrop(AActor* Actor)
		{
			auto* Component = NewObject<USoulsRewardDropSpawnerComponent>(Actor);
			Actor->AddInstanceComponent(Component);
			Component->RegisterComponent();
			return Component;
		}
		FOwner MakeOwner(float Base = 100.0f, bool bRewards = false)
		{
			FOwner Owner;
			Owner.Actor = World ? World->SpawnActor<AActor>() : nullptr;
			if (!Owner.Actor) { return Owner; }
			auto* Root = NewObject<USceneComponent>(Owner.Actor);
			Owner.Actor->AddInstanceComponent(Root);
			Owner.Actor->SetRootComponent(Root);
			Root->SetMobility(EComponentMobility::Movable);
			Root->RegisterComponent();
			Owner.Health = NewObject<USoulsHealthComponent>(Owner.Actor);
			Owner.Actor->AddInstanceComponent(Owner.Health);
			Owner.Health->SetBaseMaxHealth(Base);
			Owner.Health->RegisterComponent();
			Owner.Items = NewObject<USoulsItemComponent>(Owner.Actor);
			Owner.Actor->AddInstanceComponent(Owner.Items);
			Owner.Items->RegisterComponent();
			if (bRewards)
			{
				Owner.Rewards = NewObject<USoulsRewardComponent>(Owner.Actor);
				Owner.Actor->AddInstanceComponent(Owner.Rewards);
				Owner.Rewards->RegisterComponent();
			}
			return Owner;
		}
		USoulsItemReward* KillSource(FAutomationTestBase& Test)
		{
			if (!Test.TestNotNull(TEXT("World"), World) || !Source.Actor || !Drop) { return nullptr; }
			if (!Test.TestTrue(TEXT("Grant source"), Source.Items->GrantItem(Definition.Get(), SourceGrant) == ESoulsItemResult::Success)) { return nullptr; }
			Test.TestEqual(TEXT("Source 500 -> 625"), Source.Health->GetMaxHealth(), 625.0f);
			if (!Test.TestTrue(TEXT("Configure reward source"), Source.Rewards->ConfigureSourceGrant(SourceGrant) == ESoulsRewardSourceResult::Success)) { return nullptr; }
			Source.Health->TakeDamage(10000.0f);
			USoulsItemReward* Reward = Source.Rewards->GetReward();
			Test.TestNotNull(TEXT("Generated reward"), Reward);
			return Reward;
		}
		int32 PickupCount() const
		{
			int32 Count = 0;
			for (TActorIterator<ASoulsItemPickup> It(World); It; ++It) { if (!It->IsActorBeingDestroyed()) { ++Count; } }
			return Count;
		}
		USphereComponent* AddTargetSphere(AActor* Actor, const FVector& Position)
		{
			auto* Sphere = NewObject<USphereComponent>(Actor);
			Actor->AddInstanceComponent(Sphere);
			Actor->SetRootComponent(Sphere);
			Sphere->InitSphereRadius(25.0f);
			Sphere->SetCollisionObjectType(ECC_WorldDynamic);
			Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Sphere->SetCollisionResponseToAllChannels(ECR_Overlap);
			Sphere->SetGenerateOverlapEvents(true);
			Sphere->SetWorldLocation(Position);
			Sphere->RegisterComponent();
			return Sphere;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsDropATest, "SoulsNew.DropSpawner.A.SpawnFromRewardGenerated", SoulsDropTests::Flags)
bool FSoulsDropATest::RunTest(const FString& Parameters)
{
	SoulsDropTests::FFixture F;
	if (!F.KillSource(*this)) { return false; }
	TestNotNull(TEXT("Automatic physical pickup"), F.Drop->GetActivePickup());
	TestTrue(TEXT("Active state"), F.Drop->GetDropState() == ESoulsRewardDropState::PickupActive);
	TestNull(TEXT("No simultaneous pending reward"), F.Drop->GetPendingReward());
	TestEqual(TEXT("Exactly one pickup"), F.PickupCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsDropBTest, "SoulsNew.DropSpawner.B.SameRewardInstance", SoulsDropTests::Flags)
bool FSoulsDropBTest::RunTest(const FString& Parameters)
{
	SoulsDropTests::FFixture F;
	USoulsItemReward* Reward = F.KillSource(*this);
	ASoulsItemPickup* Pickup = F.Drop->GetActivePickup();
	if (!Reward || !TestNotNull(TEXT("Pickup"), Pickup)) { return false; }
	TestTrue(TEXT("Same reward pointer"), Pickup->GetReward() == Reward);
	TestEqual(TEXT("Same reward ID"), Pickup->GetReward()->GetRewardId(), Reward->GetRewardId());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsDropCTest, "SoulsNew.DropSpawner.C.SameDefinitionInstance", SoulsDropTests::Flags)
bool FSoulsDropCTest::RunTest(const FString& Parameters)
{
	SoulsDropTests::FFixture F;
	if (!F.KillSource(*this) || !TestNotNull(TEXT("Pickup"), F.Drop->GetActivePickup())) { return false; }
	TestTrue(TEXT("Definition shared across source and pickup"), F.Source.Items->GetGrantDefinition(F.SourceGrant)
		== F.Drop->GetActivePickup()->GetReward()->GetItemDefinition());
	TestTrue(TEXT("No definition copy"), F.Drop->GetActivePickup()->GetReward()->GetItemDefinition() == F.Definition.Get());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsDropDTest, "SoulsNew.DropSpawner.D.SpawnTransform", SoulsDropTests::Flags)
bool FSoulsDropDTest::RunTest(const FString& Parameters)
{
	SoulsDropTests::FFixture F;
	const FTransform Transform(FRotator(0, 90, 0), FVector(1000, 200, 300), FVector(2, 3, 4));
	const FVector Offset(20, 30, 40);
	F.Source.Actor->SetActorTransform(Transform);
	TestTrue(TEXT("Set offset"), F.Drop->SetSpawnOffset(Offset));
	if (!F.KillSource(*this) || !TestNotNull(TEXT("Pickup"), F.Drop->GetActivePickup())) { return false; }
	const FTransform Actual = F.Drop->GetActivePickup()->GetActorTransform();
	TestTrue(TEXT("Local offset includes rotation and owner scale"), Actual.GetLocation().Equals(Transform.TransformPosition(Offset), 0.001));
	TestTrue(TEXT("Owner rotation"), Actual.GetRotation().Equals(Transform.GetRotation(), 0.001));
	TestTrue(TEXT("Unit pickup scale"), Actual.GetScale3D().Equals(FVector::OneVector));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsDropETest, "SoulsNew.DropSpawner.E.InvalidPickupClass", SoulsDropTests::Flags)
bool FSoulsDropETest::RunTest(const FString& Parameters)
{
	SoulsDropTests::FFixture F;
	F.Drop->SetPickupClass(nullptr);
	USoulsItemReward* Reward = F.KillSource(*this);
	if (!Reward) { return false; }
	TestTrue(TEXT("Invalid class result"), F.Drop->GetLastSpawnResult() == ESoulsRewardDropResult::InvalidPickupClass);
	TestTrue(TEXT("Available"), Reward->GetState() == ESoulsItemRewardState::Available);
	TestTrue(TEXT("Same reward pending"), F.Drop->GetPendingReward() == Reward);
	TestEqual(TEXT("No pickup"), F.PickupCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsDropFTest, "SoulsNew.DropSpawner.F.RetryAfterSpawnFailure", SoulsDropTests::Flags)
bool FSoulsDropFTest::RunTest(const FString& Parameters)
{
	SoulsDropTests::FFixture F;
	F.Drop->SetPickupClass(nullptr);
	USoulsItemReward* Reward = F.KillSource(*this);
	if (!Reward) { return false; }
	F.Drop->SetPickupClass(ASoulsItemPickup::StaticClass());
	TestTrue(TEXT("Retry success"), F.Drop->TrySpawnPendingReward() == ESoulsRewardDropResult::Success);
	if (!TestNotNull(TEXT("Pickup"), F.Drop->GetActivePickup())) { return false; }
	TestTrue(TEXT("Exact pending reward reused"), F.Drop->GetActivePickup()->GetReward() == Reward);
	TestNull(TEXT("Pending cleared"), F.Drop->GetPendingReward());
	TestEqual(TEXT("Only one pickup"), F.PickupCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsDropGTest, "SoulsNew.DropSpawner.G.NoDuplicatePickup", SoulsDropTests::Flags)
bool FSoulsDropGTest::RunTest(const FString& Parameters)
{
	SoulsDropTests::FFixture F;
	USoulsItemReward* Reward = F.KillSource(*this);
	if (!Reward) { return false; }
	ASoulsItemPickup* First = F.Drop->GetActivePickup();
	F.Source.Rewards->RewardGenerated().Broadcast(F.Source.Rewards, Reward);
	TestTrue(TEXT("Direct duplicate rejected"), F.Drop->TrySpawnRewardPickup(Reward) == ESoulsRewardDropResult::AlreadyActive);
	TestTrue(TEXT("Retry duplicate rejected"), F.Drop->TrySpawnPendingReward() == ESoulsRewardDropResult::AlreadyActive);
	TestTrue(TEXT("Original pickup"), F.Drop->GetActivePickup() == First);
	TestEqual(TEXT("No duplicate"), F.PickupCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsDropHTest, "SoulsNew.DropSpawner.H.SourceDestroyedAfterSpawn", SoulsDropTests::Flags)
bool FSoulsDropHTest::RunTest(const FString& Parameters)
{
	SoulsDropTests::FFixture F;
	USoulsItemReward* Reward = F.KillSource(*this);
	ASoulsItemPickup* Pickup = F.Drop->GetActivePickup();
	if (!Reward || !TestNotNull(TEXT("Pickup"), Pickup)) { return false; }
	F.Source.Actor->Destroy();
	TestFalse(TEXT("Pickup survives source"), Pickup->IsActorBeingDestroyed());
	TestNull(TEXT("No source ownership"), Pickup->GetOwner());
	const auto Target = F.MakeOwner();
	FGuid Id;
	TestTrue(TEXT("Claim after source destruction"), Pickup->TryClaimForActor(Target.Actor, Id) == ESoulsPickupClaimResult::Success);
	TestEqual(TEXT("Target 125"), Target.Health->GetMaxHealth(), 125.0f);
	TestTrue(TEXT("Same definition granted"), Target.Items->GetGrantDefinition(Id) == Reward->GetItemDefinition());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsDropITest, "SoulsNew.DropSpawner.I.EndToEndPhysical", SoulsDropTests::Flags)
bool FSoulsDropITest::RunTest(const FString& Parameters)
{
	SoulsDropTests::FFixture F(true);
	USoulsItemReward* Reward = F.KillSource(*this);
	ASoulsItemPickup* Pickup = F.Drop->GetActivePickup();
	if (!Reward || !TestNotNull(TEXT("Automatically spawned pickup"), Pickup)) { return false; }
	const auto Target = F.MakeOwner();
	USphereComponent* Sphere = F.AddTargetSphere(Target.Actor, FVector(3000, 0, 0));
	int32 Claims = 0;
	FGuid Grant;
	Pickup->PickupClaimed().AddLambda([&](ASoulsItemPickup*, AActor*, FGuid Id) { ++Claims; Grant = Id; });
	TestTrue(TEXT("Pickup has finished BeginPlay"), Pickup->HasActorBegunPlay());
	TestEqual(TEXT("Target initially 100"), Target.Health->GetMaxHealth(), 100.0f);
	F.World->Tick(LEVELTICK_All, 1.0f / 60.0f);
	TestTrue(TEXT("Physical sphere exists"), Pickup->GetInteractionSphere()->IsPhysicsStateCreated());
	Target.Actor->SetActorLocation(Pickup->GetActorLocation(), false);
	Sphere->UpdateOverlaps(); // Real motor query; no direct claim or synthetic broadcast.
	TestEqual(TEXT("One physical claim"), Claims, 1);
	TestEqual(TEXT("Target 100 -> 125"), Target.Health->GetMaxHealth(), 125.0f);
	TestTrue(TEXT("Reward claimed"), Reward->GetState() == ESoulsItemRewardState::Claimed);
	TestTrue(TEXT("Pickup consumed and destroyed"), Pickup->IsConsumed() && Pickup->IsActorBeingDestroyed());
	TestTrue(TEXT("Same definition end to end"), Target.Items->GetGrantDefinition(Grant) == F.Definition.Get());
	TestTrue(TEXT("Completed"), F.Drop->GetDropState() == ESoulsRewardDropState::Completed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsDropJTest, "SoulsNew.DropSpawner.J.PickupDestroyedUnclaimed", SoulsDropTests::Flags)
bool FSoulsDropJTest::RunTest(const FString& Parameters)
{
	SoulsDropTests::FFixture F;
	USoulsItemReward* Reward = F.KillSource(*this);
	ASoulsItemPickup* First = F.Drop->GetActivePickup();
	if (!Reward || !TestNotNull(TEXT("Pickup"), First)) { return false; }
	const TWeakObjectPtr<ASoulsItemPickup> Old(First);
	First->Destroy();
	TestTrue(TEXT("Available and pending"), Reward->GetState() == ESoulsItemRewardState::Available && F.Drop->GetPendingReward() == Reward);
	TestNull(TEXT("Destroyed not active"), F.Drop->GetActivePickup());
	CollectGarbage(RF_NoFlags);
	TestFalse(TEXT("No strong pickup retention"), Old.IsValid());
	TestTrue(TEXT("Explicit retry"), F.Drop->TrySpawnPendingReward() == ESoulsRewardDropResult::Success);
	if (!TestNotNull(TEXT("New pickup"), F.Drop->GetActivePickup())) { return false; }
	TestTrue(TEXT("Same reward rematerialized"), F.Drop->GetActivePickup()->GetReward() == Reward);
	TestEqual(TEXT("Only one live pickup"), F.PickupCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsDropKTest, "SoulsNew.DropSpawner.K.ClaimedRewardNoRespawn", SoulsDropTests::Flags)
bool FSoulsDropKTest::RunTest(const FString& Parameters)
{
	SoulsDropTests::FFixture F;
	USoulsItemReward* Reward = F.KillSource(*this);
	ASoulsItemPickup* Pickup = F.Drop->GetActivePickup();
	if (!Reward || !TestNotNull(TEXT("Pickup"), Pickup)) { return false; }
	const auto Target = F.MakeOwner();
	FGuid Id;
	TestTrue(TEXT("Claim"), Pickup->TryClaimForActor(Target.Actor, Id) == ESoulsPickupClaimResult::Success);
	TestTrue(TEXT("Retry rejected"), F.Drop->TrySpawnPendingReward() == ESoulsRewardDropResult::AlreadyClaimed);
	F.Source.Rewards->RewardGenerated().Broadcast(F.Source.Rewards, Reward);
	TestEqual(TEXT("No respawn"), F.PickupCount(), 0);
	TestNull(TEXT("No pending claimed reward"), F.Drop->GetPendingReward());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsDropLTest, "SoulsNew.DropSpawner.L.GenericActor", SoulsDropTests::Flags)
bool FSoulsDropLTest::RunTest(const FString& Parameters)
{
	SoulsDropTests::FFixture F;
	if (!F.KillSource(*this) || !TestNotNull(TEXT("Pickup"), F.Drop->GetActivePickup())) { return false; }
	const auto Target = F.MakeOwner();
	TestTrue(TEXT("Source is exactly AActor"), F.Source.Actor->GetClass() == AActor::StaticClass());
	TestTrue(TEXT("Target is exactly AActor"), Target.Actor->GetClass() == AActor::StaticClass());
	FGuid Id;
	TestTrue(TEXT("Generic composition works"), F.Drop->GetActivePickup()->TryClaimForActor(Target.Actor, Id) == ESoulsPickupClaimResult::Success);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsDropMTest, "SoulsNew.DropSpawner.M.Events", SoulsDropTests::Flags)
bool FSoulsDropMTest::RunTest(const FString& Parameters)
{
	SoulsDropTests::FFixture F(true);
	int32 Spawned = 0;
	int32 Failed = 0;
	F.Drop->PickupSpawned().AddLambda([&](USoulsRewardDropSpawnerComponent*, ASoulsItemPickup* Pickup, USoulsItemReward* Reward)
	{
		++Spawned;
		TestTrue(TEXT("Event after initialization and construction"), Pickup->GetReward() == Reward && Pickup->GetVisualMesh() != nullptr);
		TestTrue(TEXT("Spawn fully finalized before success"), Pickup->HasActorBegunPlay());
		TestTrue(TEXT("Active state committed before event"), F.Drop->GetActivePickup() == Pickup);
	});
	F.Drop->PickupSpawnFailed().AddLambda([&](USoulsRewardDropSpawnerComponent*, USoulsItemReward* Reward, ESoulsRewardDropResult Reason)
	{
		++Failed;
		TestTrue(TEXT("Failure retains pending"), F.Drop->GetPendingReward() == Reward);
		TestTrue(TEXT("Correct failure reason"), Reason == ESoulsRewardDropResult::InvalidPickupClass);
	});
	F.Drop->SetPickupClass(nullptr);
	if (!F.KillSource(*this)) { return false; }
	TestEqual(TEXT("No premature success"), Spawned, 0);
	TestEqual(TEXT("One failure"), Failed, 1);
	F.Drop->SetPickupClass(ASoulsItemPickup::StaticClass());
	F.Drop->TrySpawnPendingReward();
	F.Drop->TrySpawnPendingReward();
	TestEqual(TEXT("Only one success"), Spawned, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsDropNTest, "SoulsNew.DropSpawner.N.Reentrancy", SoulsDropTests::Flags)
bool FSoulsDropNTest::RunTest(const FString& Parameters)
{
	SoulsDropTests::FFixture F;
	int32 Events = 0;
	const FDelegateHandle SpawnHandle = F.World->AddOnActorPreSpawnInitialization(FOnActorSpawned::FDelegate::CreateLambda([&](AActor* Actor)
	{
		if (Cast<ASoulsItemPickup>(Actor))
		{
			TestTrue(TEXT("Retry during actual SpawnActor busy"), F.Drop->TrySpawnPendingReward() == ESoulsRewardDropResult::Busy);
		}
	}));
	F.Drop->PickupSpawnFailed().AddLambda([&](USoulsRewardDropSpawnerComponent*, USoulsItemReward* Reward, ESoulsRewardDropResult)
	{
		TestTrue(TEXT("Failure retry busy"), F.Drop->TrySpawnPendingReward() == ESoulsRewardDropResult::Busy);
		F.Source.Rewards->RewardGenerated().Broadcast(F.Source.Rewards, Reward);
	});
	F.Drop->PickupSpawned().AddLambda([&](USoulsRewardDropSpawnerComponent*, ASoulsItemPickup*, USoulsItemReward* Reward)
	{
		++Events;
		TestTrue(TEXT("Success retry busy"), F.Drop->TrySpawnPendingReward() == ESoulsRewardDropResult::Busy);
		TestTrue(TEXT("Success direct spawn busy"), F.Drop->TrySpawnRewardPickup(Reward) == ESoulsRewardDropResult::Busy);
		F.Source.Rewards->RewardGenerated().Broadcast(F.Source.Rewards, Reward);
		TestFalse(TEXT("Config cannot mutate during callback"), F.Drop->SetPickupClass(nullptr));
	});
	F.Drop->SetPickupClass(nullptr);
	if (!F.KillSource(*this)) { return false; }
	F.Drop->SetPickupClass(ASoulsItemPickup::StaticClass());
	F.Drop->TrySpawnPendingReward();
	F.World->RemoveOnActorPreSpawnInitialization(SpawnHandle);
	TestEqual(TEXT("One callback"), Events, 1);
	TestEqual(TEXT("No reentrant pickup"), F.PickupCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsDropOTest, "SoulsNew.DropSpawner.O.Lifetime", SoulsDropTests::Flags)
bool FSoulsDropOTest::RunTest(const FString& Parameters)
{
	SoulsDropTests::FFixture F;
	USoulsItemReward* Reward = F.KillSource(*this);
	ASoulsItemPickup* Pickup = F.Drop->GetActivePickup();
	if (!Reward || !TestNotNull(TEXT("Pickup"), Pickup)) { return false; }
	TWeakObjectPtr<AActor> Source(F.Source.Actor);
	TWeakObjectPtr<USoulsRewardDropSpawnerComponent> Drop(F.Drop);
	TWeakObjectPtr<USoulsItemReward> WeakReward(Reward);
	TWeakObjectPtr<USoulsItemDefinition> Definition(F.Definition.Get());
	F.Definition.Reset();
	F.Source.Actor->Destroy();
	CollectGarbage(RF_NoFlags);
	TestFalse(TEXT("Source collected"), Source.IsValid());
	TestFalse(TEXT("Spawner collected"), Drop.IsValid());
	TestTrue(TEXT("Reward held only by physical holder survives"), WeakReward.IsValid());
	TestTrue(TEXT("Definition survives"), Definition.IsValid());
	TestTrue(TEXT("Same reward after GC"), Pickup->GetReward() == WeakReward.Get());
	const auto Target = F.MakeOwner();
	FGuid Id;
	TestTrue(TEXT("Claim after GC"), Pickup->TryClaimForActor(Target.Actor, Id) == ESoulsPickupClaimResult::Success);
	TestTrue(TEXT("Same surviving definition granted"), Target.Items->GetGrantDefinition(Id) == Definition.Get());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsDropPTest, "SoulsNew.DropSpawner.P.InvalidOrMissingRewardComponent", SoulsDropTests::Flags)
bool FSoulsDropPTest::RunTest(const FString& Parameters)
{
	SoulsDropTests::FFixture F;
	const auto Missing = F.MakeOwner();
	auto* MissingDrop = F.AddDrop(Missing.Actor);
	TestFalse(TEXT("Missing source rejected"), MissingDrop->RefreshRewardBinding());
	TestTrue(TEXT("Missing source remains idle"), MissingDrop->GetDropState() == ESoulsRewardDropState::Idle);
	TestTrue(TEXT("Nothing to retry"), MissingDrop->TrySpawnPendingReward() == ESoulsRewardDropResult::NoPendingReward);
	auto* Extra = NewObject<USoulsRewardComponent>(F.Source.Actor);
	F.Source.Actor->AddInstanceComponent(Extra);
	Extra->RegisterComponent();
	TestFalse(TEXT("Ambiguous sources rejected and unbound"), F.Drop->RefreshRewardBinding());
	USoulsItemReward* Reward = F.KillSource(*this);
	if (!Reward) { return false; }
	TestEqual(TEXT("No arbitrary source selected"), F.PickupCount(), 0);
	Extra->DestroyComponent();
	TestTrue(TEXT("Corrected late binding"), F.Drop->RefreshRewardBinding());
	if (!TestNotNull(TEXT("Existing reward recovered"), F.Drop->GetActivePickup())) { return false; }
	TestTrue(TEXT("Same previously generated reward"), F.Drop->GetActivePickup()->GetReward() == Reward);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsDropWorldTest, "SoulsNew.DropSpawner.MissingWorld", SoulsDropTests::Flags)
bool FSoulsDropWorldTest::RunTest(const FString& Parameters)
{
	SoulsDropTests::FFixture F;
	F.Drop->SetPickupClass(nullptr);
	USoulsItemReward* Reward = F.KillSource(*this);
	if (!Reward) { return false; }
	TStrongObjectPtr<USoulsRewardDropSpawnerComponent> Detached(NewObject<USoulsRewardDropSpawnerComponent>());
	TestFalse(TEXT("No owner binding"), Detached->RefreshRewardBinding());
	TestTrue(TEXT("Missing world failure"), Detached->TrySpawnRewardPickup(Reward) == ESoulsRewardDropResult::InvalidWorld);
	TestTrue(TEXT("Reward remains available"), Reward->GetState() == ESoulsItemRewardState::Available);
	TestTrue(TEXT("Detached holder retains exact pending reward"), Detached->GetPendingReward() == Reward);
	TestTrue(TEXT("Retry without world stays safe"), Detached->TrySpawnPendingReward() == ESoulsRewardDropResult::InvalidWorld);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsDropInitializationTest, "SoulsNew.DropSpawner.InitializationFailureCleanup", SoulsDropTests::Flags)
bool FSoulsDropInitializationTest::RunTest(const FString& Parameters)
{
	SoulsDropTests::FFixture F;
	TWeakObjectPtr<ASoulsItemPickup> Partial;
	// A competing spawn observer assigns early: the spawner's single-assignment initialization must fail.
	const FDelegateHandle Handle = F.World->AddOnActorPreSpawnInitialization(FOnActorSpawned::FDelegate::CreateLambda([&](AActor* Actor)
	{
		if (auto* Pickup = Cast<ASoulsItemPickup>(Actor))
		{
			Partial = Pickup;
			TestTrue(TEXT("Competing assignment"), Pickup->InitializeReward(F.Source.Rewards->GetReward()));
		}
	}));
	USoulsItemReward* Reward = F.KillSource(*this);
	F.World->RemoveOnActorPreSpawnInitialization(Handle);
	if (!Reward) { return false; }
	TestTrue(TEXT("Initialization failure result"), F.Drop->GetLastSpawnResult() == ESoulsRewardDropResult::InitializationFailed);
	TestTrue(TEXT("Failed partial pickup destroyed"), !Partial.IsValid() || Partial->IsActorBeingDestroyed());
	TestEqual(TEXT("No invalid actor left alive"), F.PickupCount(), 0);
	TestTrue(TEXT("Same pending reward"), F.Drop->GetPendingReward() == Reward);
	TestTrue(TEXT("Retry after observer removed"), F.Drop->TrySpawnPendingReward() == ESoulsRewardDropResult::Success);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsDropCallbackTest, "SoulsNew.DropSpawner.DestructionDuringSpawnCallback", SoulsDropTests::Flags)
bool FSoulsDropCallbackTest::RunTest(const FString& Parameters)
{
	SoulsDropTests::FFixture F;
	ASoulsItemPickup* Spawned = nullptr;
	F.Drop->PickupSpawned().AddLambda([&](USoulsRewardDropSpawnerComponent*, ASoulsItemPickup* Pickup, USoulsItemReward*)
	{
		Spawned = Pickup;
		F.Source.Actor->Destroy();
	});
	USoulsItemReward* Reward = F.KillSource(*this);
	if (!Reward || !TestNotNull(TEXT("Spawned before source destruction"), Spawned)) { return false; }
	TestFalse(TEXT("Physical reward survives callback"), Spawned->IsActorBeingDestroyed());
	TestTrue(TEXT("Ended spawner cannot retry"), F.Drop->TrySpawnPendingReward() == ESoulsRewardDropResult::Unavailable);
	TestTrue(TEXT("Same reward retained by pickup"), Spawned->GetReward() == Reward);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsDropDestroyedCallbackTest, "SoulsNew.DropSpawner.PickupDestroyedDuringSpawnCallback", SoulsDropTests::Flags)
bool FSoulsDropDestroyedCallbackTest::RunTest(const FString& Parameters)
{
	SoulsDropTests::FFixture F;
	const FDelegateHandle Handle = F.Drop->PickupSpawned().AddLambda([&](USoulsRewardDropSpawnerComponent*, ASoulsItemPickup* Pickup, USoulsItemReward*)
	{
		Pickup->Destroy();
		TestTrue(TEXT("Callback retry busy"), F.Drop->TrySpawnPendingReward() == ESoulsRewardDropResult::Busy);
	});
	USoulsItemReward* Reward = F.KillSource(*this);
	F.Drop->PickupSpawned().Remove(Handle);
	if (!Reward) { return false; }
	TestTrue(TEXT("Destruction returns reward to pending"), F.Drop->GetPendingReward() == Reward);
	TestTrue(TEXT("Later explicit retry succeeds"), F.Drop->TrySpawnPendingReward() == ESoulsRewardDropResult::Success);
	TestEqual(TEXT("Exactly one replacement"), F.PickupCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsDropImmediateClaimTest, "SoulsNew.DropSpawner.ImmediateOverlapOnActivation", SoulsDropTests::Flags)
bool FSoulsDropImmediateClaimTest::RunTest(const FString& Parameters)
{
	SoulsDropTests::FFixture F(true);
	const auto Target = F.MakeOwner();
	F.AddTargetSphere(Target.Actor, FVector(1000, 0, 100));
	F.World->Tick(LEVELTICK_All, 1.0f / 60.0f);
	int32 SpawnEvents = 0;
	F.Drop->PickupSpawned().AddLambda([&](USoulsRewardDropSpawnerComponent*, ASoulsItemPickup* Pickup, USoulsItemReward* Reward)
	{
		++SpawnEvents;
		TestTrue(TEXT("Correct reward even with immediate claim"), Pickup->GetReward() == Reward);
		// Consumption calls EndPlay before this notification; HasActorBegunPlay is then false.
		TestTrue(TEXT("Full construction before publication"), Pickup->IsActorInitialized());
	});
	USoulsItemReward* Reward = F.KillSource(*this);
	if (!Reward) { return false; }
	TestEqual(TEXT("One materialization"), SpawnEvents, 1);
	TestEqual(TEXT("Immediate physical overlap target 125"), Target.Health->GetMaxHealth(), 125.0f);
	TestTrue(TEXT("Completed despite immediate actor destruction"), F.Drop->GetDropState() == ESoulsRewardDropState::Completed);
	TestTrue(TEXT("No respawn after immediate claim"), F.Drop->TrySpawnPendingReward() == ESoulsRewardDropResult::AlreadyClaimed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsDropSpawnFailureTest, "SoulsNew.DropSpawner.SpawnFailureRetry", SoulsDropTests::Flags)
bool FSoulsDropSpawnFailureTest::RunTest(const FString& Parameters)
{
	SoulsDropTests::FFixture F;
	const FDelegateHandle Handle = F.World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateLambda([](AActor* Actor)
	{
		if (Cast<ASoulsItemPickup>(Actor)) { Actor->Destroy(); }
	}));
	USoulsItemReward* Reward = F.KillSource(*this);
	F.World->RemoveOnActorSpawnedHandler(Handle);
	if (!Reward) { return false; }
	TestTrue(TEXT("Destroyed during allocation is spawn failure"), F.Drop->GetLastSpawnResult() == ESoulsRewardDropResult::SpawnFailed);
	TestEqual(TEXT("No failed actor left alive"), F.PickupCount(), 0);
	TestTrue(TEXT("Same reward remains pending"), F.Drop->GetPendingReward() == Reward);
	TestTrue(TEXT("Explicit retry after failure"), F.Drop->TrySpawnPendingReward() == ESoulsRewardDropResult::Success);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsDropCyclesTest, "SoulsNew.DropSpawner.NextLifeAndPendingReplacement", SoulsDropTests::Flags)
bool FSoulsDropCyclesTest::RunTest(const FString& Parameters)
{
	SoulsDropTests::FFixture F;
	TStrongObjectPtr<USoulsItemReward> First(F.KillSource(*this));
	if (!First.IsValid() || !TestNotNull(TEXT("First pickup"), F.Drop->GetActivePickup())) { return false; }
	const auto OtherSource = F.MakeOwner(500, true);
	FGuid OtherGrant;
	OtherSource.Items->GrantItem(F.Definition.Get(), OtherGrant);
	OtherSource.Rewards->ConfigureSourceGrant(OtherGrant);
	OtherSource.Health->TakeDamage(10000);
	USoulsItemReward* OtherReward = OtherSource.Rewards->GetReward();
	if (!TestNotNull(TEXT("Another reward"), OtherReward)) { return false; }
	TestTrue(TEXT("Cannot discard an available reward"), F.Drop->TrySpawnRewardPickup(OtherReward) == ESoulsRewardDropResult::RewardPending);
	TestTrue(TEXT("Original still materialized"), F.Drop->GetActivePickup()->GetReward() == First.Get());
	const auto Target = F.MakeOwner();
	FGuid Id;
	F.Drop->GetActivePickup()->TryClaimForActor(Target.Actor, Id);
	TestTrue(TEXT("Reset health explicitly"), F.Source.Health->ResetHealthForNewLife());
	TestTrue(TEXT("Rearm rewards explicitly"), F.Source.Rewards->Rearm() == ESoulsRewardSourceResult::Success);
	F.Source.Health->TakeDamage(10000);
	USoulsItemReward* Second = F.Source.Rewards->GetReward();
	if (!TestNotNull(TEXT("Second life pickup"), F.Drop->GetActivePickup()) || !TestNotNull(TEXT("Second reward"), Second)) { return false; }
	TestTrue(TEXT("New reward per life"), Second != First.Get() && Second->GetRewardId() != First->GetRewardId());
	TestTrue(TEXT("Same definition across lives"), Second->GetItemDefinition() == First->GetItemDefinition());
	TestTrue(TEXT("Second reward materialized"), F.Drop->GetActivePickup()->GetReward() == Second);
	TestEqual(TEXT("Exactly one active pickup"), F.PickupCount(), 1);
	return true;
}

#endif
