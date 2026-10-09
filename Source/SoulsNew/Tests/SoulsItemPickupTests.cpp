#include "Interaction/SoulsItemPickup.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Attributes/SoulsHealthComponent.h"
#include "Async/TaskGraphInterfaces.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Items/SoulsItemComponent.h"
#include "Items/SoulsItemDefinition.h"
#include "Items/SoulsItemReward.h"
#include "Items/SoulsRewardComponent.h"
#include "Misc/AutomationTest.h"
#include "UObject/GarbageCollection.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace SoulsPickupTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext
		| EAutomationTestFlags::CommandletContext | EAutomationTestFlags::EngineFilter;

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
		FGuid SourceGrant;

		explicit FFixture(bool bPhysics = false)
		{
			FSoulsItemDefinitionData Data;
			Data.ItemId = TEXT("Item.Vitality");
			Data.DisplayName = FText::FromString(TEXT("Vitalidad"));
			FSoulsItemEffect Effect;
			Effect.Attribute = ESoulsItemAttribute::MaxHealth;
			Effect.Operation = ESoulsAttributeModifierOperation::AdditivePercent;
			Effect.Magnitude = 0.25f;
			Data.Effects.Add(Effect);
			Definition.Reset(USoulsItemDefinition::CreateTransient(Data));
			const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false)
				.CreatePhysicsScene(bPhysics).CreateNavigation(false).CreateAISystem(false)
				.ShouldSimulatePhysics(bPhysics).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			if (World && GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			if (World && bPhysics)
			{
				World->InitializeActorsForPlay(FURL());
				World->BeginPlay();
				World->GetWorldSettings()->NotifyBeginPlay();
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

		FOwner MakeOwner(float Base = 100.0f, bool bItems = true, bool bHealth = true, bool bRewards = false)
		{
			FOwner Owner;
			Owner.Actor = World ? World->SpawnActor<AActor>() : nullptr;
			if (!Owner.Actor) { return Owner; }
			if (bHealth)
			{
				Owner.Health = NewObject<USoulsHealthComponent>(Owner.Actor);
				Owner.Actor->AddInstanceComponent(Owner.Health);
				Owner.Health->SetBaseMaxHealth(Base);
				Owner.Health->RegisterComponent();
			}
			if (bItems)
			{
				Owner.Items = NewObject<USoulsItemComponent>(Owner.Actor);
				Owner.Actor->AddInstanceComponent(Owner.Items);
				Owner.Items->RegisterComponent();
			}
			if (bRewards)
			{
				Owner.Rewards = NewObject<USoulsRewardComponent>(Owner.Actor);
				Owner.Actor->AddInstanceComponent(Owner.Rewards);
				Owner.Rewards->RegisterComponent();
			}
			return Owner;
		}

		USoulsItemReward* MakeReward(FAutomationTestBase& Test)
		{
			Source = MakeOwner(500.0f, true, true, true);
			if (!Test.TestNotNull(TEXT("Source actor"), Source.Actor) || !Source.Health || !Source.Items || !Source.Rewards) { return nullptr; }
			if (!Test.TestTrue(TEXT("Grant source"), Source.Items->GrantItem(Definition.Get(), SourceGrant) == ESoulsItemResult::Success)) { return nullptr; }
			Test.TestEqual(TEXT("Source 500 -> 625"), Source.Health->GetMaxHealth(), 625.0f);
			if (!Test.TestTrue(TEXT("Configure source grant"), Source.Rewards->ConfigureSourceGrant(SourceGrant) == ESoulsRewardSourceResult::Success)) { return nullptr; }
			Source.Health->TakeDamage(10000.0f);
			USoulsItemReward* Reward = Source.Rewards->GetReward();
			Test.TestNotNull(TEXT("Death reward"), Reward);
			return Reward;
		}

		ASoulsItemPickup* SpawnPickup()
		{
			return World ? World->SpawnActor<ASoulsItemPickup>(FVector(1000.0f, 0.0f, 0.0f), FRotator::ZeroRotator) : nullptr;
		}

		ASoulsItemPickup* ReadyPickup(FAutomationTestBase& Test)
		{
			USoulsItemReward* Reward = MakeReward(Test);
			ASoulsItemPickup* Pickup = SpawnPickup();
			if (!Reward || !Test.TestNotNull(TEXT("Pickup"), Pickup)) { return nullptr; }
			if (!Test.TestTrue(TEXT("Initialize reward"), Pickup->InitializeReward(Reward))) { return nullptr; }
			return Pickup;
		}
	};

	bool Ready(FAutomationTestBase& Test, const FOwner& Owner)
	{
		return Test.TestNotNull(TEXT("Target actor"), Owner.Actor) && Test.TestNotNull(TEXT("Target health"), Owner.Health)
			&& Test.TestNotNull(TEXT("Target items"), Owner.Items);
	}

	void Available(FAutomationTestBase& Test, ASoulsItemPickup* Pickup)
	{
		Test.TestFalse(TEXT("Not consumed"), Pickup->IsConsumed());
		Test.TestFalse(TEXT("Not destroying"), Pickup->IsActorBeingDestroyed());
		Test.TestTrue(TEXT("Reward available"), Pickup->GetReward()->GetState() == ESoulsItemRewardState::Available);
		Test.TestTrue(TEXT("Collision remains enabled"), Pickup->GetInteractionSphere()->GetCollisionEnabled() == ECollisionEnabled::QueryOnly);
		Test.TestTrue(TEXT("Overlap remains enabled"), Pickup->GetInteractionSphere()->GetGenerateOverlapEvents());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPickupATest, "SoulsNew.Pickups.A.Initialization", SoulsPickupTests::Flags)
bool FSoulsPickupATest::RunTest(const FString& Parameters)
{
	SoulsPickupTests::FFixture F;
	USoulsItemReward* Reward = F.MakeReward(*this);
	ASoulsItemPickup* Pickup = F.SpawnPickup();
	if (!Reward || !TestNotNull(TEXT("Pickup"), Pickup)) { return false; }
	TestTrue(TEXT("Uninitialized collision disabled"), Pickup->GetInteractionSphere()->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
	TestTrue(TEXT("Initialize"), Pickup->InitializeReward(Reward));
	TestTrue(TEXT("Exactly same reward instance"), Pickup->GetReward() == Reward);
	SoulsPickupTests::Available(*this, Pickup);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPickupBTest, "SoulsNew.Pickups.B.ValidClaim", SoulsPickupTests::Flags)
bool FSoulsPickupBTest::RunTest(const FString& Parameters)
{
	SoulsPickupTests::FFixture F;
	ASoulsItemPickup* Pickup = F.ReadyPickup(*this);
	const auto Target = F.MakeOwner();
	if (!Pickup || !SoulsPickupTests::Ready(*this, Target)) { return false; }
	FGuid Id;
	TestTrue(TEXT("Claim success"), Pickup->TryClaimForActor(Target.Actor, Id) == ESoulsPickupClaimResult::Success);
	TestEqual(TEXT("Target 100 -> 125"), Target.Health->GetMaxHealth(), 125.0f);
	TestTrue(TEXT("Reward claimed"), Pickup->GetReward()->GetState() == ESoulsItemRewardState::Claimed);
	TestTrue(TEXT("Pickup consumed"), Pickup->IsConsumed());
	TestTrue(TEXT("Destruction requested"), Pickup->IsActorBeingDestroyed());
	TestTrue(TEXT("Collision disabled"), Pickup->GetInteractionSphere()->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
	TestFalse(TEXT("No more overlap events"), Pickup->GetInteractionSphere()->GetGenerateOverlapEvents());
	TestTrue(TEXT("Valid confirmed grant"), Id.IsValid() && Target.Items->HasGrant(Id));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPickupCTest, "SoulsNew.Pickups.C.SameDefinition", SoulsPickupTests::Flags)
bool FSoulsPickupCTest::RunTest(const FString& Parameters)
{
	SoulsPickupTests::FFixture F;
	ASoulsItemPickup* Pickup = F.ReadyPickup(*this);
	const auto Target = F.MakeOwner();
	if (!Pickup || !SoulsPickupTests::Ready(*this, Target)) { return false; }
	USoulsItemDefinition* Original = Pickup->GetReward()->GetItemDefinition();
	FGuid Id;
	TestTrue(TEXT("Claim"), Pickup->TryClaimForActor(Target.Actor, Id) == ESoulsPickupClaimResult::Success);
	TestTrue(TEXT("Same definition throughout"), Original == F.Definition.Get() && Target.Items->GetGrantDefinition(Id) == Original);
	TestTrue(TEXT("New target grant"), Id != F.SourceGrant);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPickupDTest, "SoulsNew.Pickups.D.InvalidTarget", SoulsPickupTests::Flags)
bool FSoulsPickupDTest::RunTest(const FString& Parameters)
{
	SoulsPickupTests::FFixture F;
	ASoulsItemPickup* Pickup = F.ReadyPickup(*this);
	const auto Target = F.MakeOwner(100.0f, false);
	if (!Pickup || !TestNotNull(TEXT("Actor without Items"), Target.Actor)) { return false; }
	FGuid Id = FGuid::NewGuid();
	TestTrue(TEXT("Null rejected"), Pickup->TryClaimForActor(nullptr, Id) == ESoulsPickupClaimResult::InvalidTarget);
	TestTrue(TEXT("Self rejected"), Pickup->TryClaimForActor(Pickup, Id) == ESoulsPickupClaimResult::InvalidTarget);
	TestTrue(TEXT("Missing Items rejected"), Pickup->TryClaimForActor(Target.Actor, Id) == ESoulsPickupClaimResult::MissingItemComponent);
	TestFalse(TEXT("No successful output"), Id.IsValid());
	SoulsPickupTests::Available(*this, Pickup);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPickupETest, "SoulsNew.Pickups.E.RetryAfterFailure", SoulsPickupTests::Flags)
bool FSoulsPickupETest::RunTest(const FString& Parameters)
{
	SoulsPickupTests::FFixture F;
	ASoulsItemPickup* Pickup = F.ReadyPickup(*this);
	const auto Incompatible = F.MakeOwner(100.0f, true, false);
	const auto Target = F.MakeOwner();
	if (!Pickup || !TestNotNull(TEXT("Incompatible actor"), Incompatible.Actor) || !SoulsPickupTests::Ready(*this, Target)) { return false; }
	FGuid Id;
	TestTrue(TEXT("Normal GrantItem rejection propagated"), Pickup->TryClaimForActor(Incompatible.Actor, Id) == ESoulsPickupClaimResult::ClaimRejected);
	TestEqual(TEXT("No partial grant"), Incompatible.Items->GetGrantCount(), 0);
	SoulsPickupTests::Available(*this, Pickup);
	TestTrue(TEXT("Retry valid target"), Pickup->TryClaimForActor(Target.Actor, Id) == ESoulsPickupClaimResult::Success);
	TestEqual(TEXT("Target 125"), Target.Health->GetMaxHealth(), 125.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPickupFTest, "SoulsNew.Pickups.F.DoubleInteraction", SoulsPickupTests::Flags)
bool FSoulsPickupFTest::RunTest(const FString& Parameters)
{
	SoulsPickupTests::FFixture F;
	ASoulsItemPickup* Pickup = F.ReadyPickup(*this);
	const auto Target = F.MakeOwner();
	if (!Pickup || !SoulsPickupTests::Ready(*this, Target)) { return false; }
	FGuid Id;
	TestTrue(TEXT("First success"), Pickup->TryClaimForActor(Target.Actor, Id) == ESoulsPickupClaimResult::Success);
	TestTrue(TEXT("Second rejected"), Pickup->TryClaimForActor(Target.Actor, Id) == ESoulsPickupClaimResult::Unavailable);
	TestFalse(TEXT("Second output invalid"), Id.IsValid());
	TestEqual(TEXT("One grant"), Target.Items->GetGrantCount(), 1);
	TestEqual(TEXT("No 150 HP"), Target.Health->GetMaxHealth(), 125.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPickupGTest, "SoulsNew.Pickups.G.MultipleItemComponents", SoulsPickupTests::Flags)
bool FSoulsPickupGTest::RunTest(const FString& Parameters)
{
	SoulsPickupTests::FFixture F;
	ASoulsItemPickup* Pickup = F.ReadyPickup(*this);
	const auto Target = F.MakeOwner();
	if (!Pickup || !SoulsPickupTests::Ready(*this, Target)) { return false; }
	USoulsItemComponent* Extra = NewObject<USoulsItemComponent>(Target.Actor);
	Target.Actor->AddInstanceComponent(Extra);
	Extra->RegisterComponent();
	FGuid Id;
	TestTrue(TEXT("Ambiguity rejected"), Pickup->TryClaimForActor(Target.Actor, Id) == ESoulsPickupClaimResult::AmbiguousItemComponents);
	TestEqual(TEXT("First untouched"), Target.Items->GetGrantCount(), 0);
	TestEqual(TEXT("Second untouched"), Extra->GetGrantCount(), 0);
	TestEqual(TEXT("Health untouched"), Target.Health->GetMaxHealth(), 100.0f);
	SoulsPickupTests::Available(*this, Pickup);
	Extra->DestroyComponent();
	TestTrue(TEXT("One remaining component accepted"), Pickup->TryClaimForActor(Target.Actor, Id) == ESoulsPickupClaimResult::Success);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPickupHTest, "SoulsNew.Pickups.H.AlreadyClaimedReward", SoulsPickupTests::Flags)
bool FSoulsPickupHTest::RunTest(const FString& Parameters)
{
	SoulsPickupTests::FFixture F;
	USoulsItemReward* Reward = F.MakeReward(*this);
	ASoulsItemPickup* Pickup = F.SpawnPickup();
	ASoulsItemPickup* LatePickup = F.SpawnPickup();
	const auto Target = F.MakeOwner();
	if (!Reward || !Pickup || !LatePickup || !SoulsPickupTests::Ready(*this, Target)) { return false; }
	TestTrue(TEXT("Initially available assignment"), Pickup->InitializeReward(Reward));
	FGuid Id;
	TestTrue(TEXT("Claim externally"), Reward->Claim(Target.Items, Id) == ESoulsRewardClaimResult::Success);
	TestFalse(TEXT("Already claimed assignment rejected"), LatePickup->InitializeReward(Reward));
	TestNull(TEXT("No retained invalid assignment"), LatePickup->GetReward());
	TestTrue(TEXT("Already claimed presentation rejected"), Pickup->TryClaimForActor(Target.Actor, Id) == ESoulsPickupClaimResult::AlreadyClaimed);
	TestFalse(TEXT("Not consumed by failed operation"), Pickup->IsConsumed());
	TestEqual(TEXT("Only original grant"), Target.Items->GetGrantCount(), 1);
	TestEqual(TEXT("No duplicated effect"), Target.Health->GetMaxHealth(), 125.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPickupITest, "SoulsNew.Pickups.I.Reentrancy", SoulsPickupTests::Flags)
bool FSoulsPickupITest::RunTest(const FString& Parameters)
{
	SoulsPickupTests::FFixture F;
	ASoulsItemPickup* Pickup = F.ReadyPickup(*this);
	const auto Target = F.MakeOwner();
	if (!Pickup || !SoulsPickupTests::Ready(*this, Target)) { return false; }
	int32 Attempts = 0;
	auto Reenter = [&]()
	{
		FGuid Id;
		TestTrue(TEXT("Recursive claim is Busy"), Pickup->TryClaimForActor(Target.Actor, Id) == ESoulsPickupClaimResult::Busy);
		TestFalse(TEXT("No recursive output"), Id.IsValid());
		TestFalse(TEXT("Cannot reinitialize during claim"), Pickup->InitializeReward(Pickup->GetReward()));
		++Attempts;
	};
	Target.Items->ItemGranted().AddLambda([&](USoulsItemComponent*, FGuid, FName) { Reenter(); });
	Pickup->GetReward()->RewardClaimed().AddLambda([&](USoulsItemReward*, USoulsItemComponent*, FGuid) { Reenter(); });
	Pickup->PickupClaimed().AddLambda([&](ASoulsItemPickup* P, AActor*, FGuid)
	{
		TestTrue(TEXT("Consumed before pickup event"), P->IsConsumed());
		TestTrue(TEXT("No collision before pickup event"), P->GetInteractionSphere()->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
		Reenter();
	});
	FGuid Id;
	TestTrue(TEXT("Outer success"), Pickup->TryClaimForActor(Target.Actor, Id) == ESoulsPickupClaimResult::Success);
	TestEqual(TEXT("Three callback stages"), Attempts, 3);
	TestEqual(TEXT("One grant"), Target.Items->GetGrantCount(), 1);
	TestEqual(TEXT("One effect"), Target.Health->GetMaxHealth(), 125.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPickupJTest, "SoulsNew.Pickups.J.GenericActor", SoulsPickupTests::Flags)
bool FSoulsPickupJTest::RunTest(const FString& Parameters)
{
	SoulsPickupTests::FFixture F;
	ASoulsItemPickup* Pickup = F.ReadyPickup(*this);
	const auto Target = F.MakeOwner();
	if (!Pickup || !SoulsPickupTests::Ready(*this, Target)) { return false; }
	TestTrue(TEXT("Exact generic Actor target"), Target.Actor->GetClass() == AActor::StaticClass());
	TestTrue(TEXT("Exact generic source"), F.Source.Actor->GetClass() == AActor::StaticClass());
	TestFalse(TEXT("No pickup Tick"), Pickup->PrimaryActorTick.bCanEverTick);
	FGuid Id;
	TestTrue(TEXT("Generic claim"), Pickup->TryClaimForActor(Target.Actor, Id) == ESoulsPickupClaimResult::Success);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPickupKTest, "SoulsNew.Pickups.K.DefinitionLifetime", SoulsPickupTests::Flags)
bool FSoulsPickupKTest::RunTest(const FString& Parameters)
{
	SoulsPickupTests::FFixture F;
	ASoulsItemPickup* Pickup = F.ReadyPickup(*this);
	if (!Pickup) { return false; }
	TWeakObjectPtr<USoulsItemReward> WeakReward(Pickup->GetReward());
	TWeakObjectPtr<USoulsItemDefinition> WeakDefinition(F.Definition.Get());
	TestTrue(TEXT("Withdraw source grant"), F.Source.Items->RemoveItem(F.SourceGrant) == ESoulsItemResult::Success);
	TestEqual(TEXT("Source returns to 500"), F.Source.Health->GetMaxHealth(), 500.0f);
	TestTrue(TEXT("Destroy source"), F.Source.Actor->Destroy());
	F.Definition.Reset();
	CollectGarbage(RF_NoFlags);
	TestTrue(TEXT("Pickup retains reward"), WeakReward.IsValid() && Pickup->GetReward() == WeakReward.Get());
	TestTrue(TEXT("Transitive definition retention"), WeakDefinition.IsValid() && WeakReward->GetItemDefinition() == WeakDefinition.Get());
	TestTrue(TEXT("Destroy final physical holder"), Pickup->Destroy());
	CollectGarbage(RF_NoFlags);
	TestFalse(TEXT("Reward can be collected"), WeakReward.IsValid());
	TestFalse(TEXT("Definition can be collected"), WeakDefinition.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPickupLTest, "SoulsNew.Pickups.L.DestroyedWithoutClaim", SoulsPickupTests::Flags)
bool FSoulsPickupLTest::RunTest(const FString& Parameters)
{
	SoulsPickupTests::FFixture F;
	ASoulsItemPickup* Pickup = F.ReadyPickup(*this);
	const auto Target = F.MakeOwner();
	if (!Pickup || !SoulsPickupTests::Ready(*this, Target)) { return false; }
	TStrongObjectPtr<USoulsItemReward> Reward(Pickup->GetReward());
	TestTrue(TEXT("Destroy without claiming"), Pickup->Destroy());
	TestFalse(TEXT("Physical disappearance is not consumption"), Pickup->IsConsumed());
	FGuid Id;
	TestTrue(TEXT("Destroyed pickup unavailable"), Pickup->TryClaimForActor(Target.Actor, Id) == ESoulsPickupClaimResult::Unavailable);
	TestTrue(TEXT("Reward remains available"), Reward->GetState() == ESoulsItemRewardState::Available);
	TestEqual(TEXT("No target grant"), Target.Items->GetGrantCount(), 0);
	TestEqual(TEXT("No target effect"), Target.Health->GetMaxHealth(), 100.0f);
	TestEqual(TEXT("Source unchanged"), F.Source.Health->GetMaxHealth(), 625.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPickupMTest, "SoulsNew.Pickups.M.TargetDestroyedDuringClaim", SoulsPickupTests::Flags)
bool FSoulsPickupMTest::RunTest(const FString& Parameters)
{
	SoulsPickupTests::FFixture F;
	ASoulsItemPickup* Pickup = F.ReadyPickup(*this);
	const auto Lost = F.MakeOwner();
	const auto EarlyLost = F.MakeOwner();
	const auto Target = F.MakeOwner();
	if (!Pickup || !SoulsPickupTests::Ready(*this, Lost) || !SoulsPickupTests::Ready(*this, EarlyLost) || !SoulsPickupTests::Ready(*this, Target)) { return false; }
	int32 Events = 0;
	Pickup->PickupClaimed().AddLambda([&Events](ASoulsItemPickup*, AActor*, FGuid) { ++Events; });
	Lost.Items->ItemGranted().AddLambda([&Lost](USoulsItemComponent*, FGuid, FName) { Lost.Actor->Destroy(); });
	FGuid Id;
	TestTrue(TEXT("Destroyed target before confirmation rejected"), Pickup->TryClaimForActor(Lost.Actor, Id) == ESoulsPickupClaimResult::ClaimRejected);
	TestFalse(TEXT("No failed output"), Id.IsValid());
	SoulsPickupTests::Available(*this, Pickup);
	EarlyLost.Health->MaxHealthChanged().AddLambda([&EarlyLost](USoulsHealthComponent*, float, float)
	{
		if (!EarlyLost.Actor->IsActorBeingDestroyed()) { EarlyLost.Actor->Destroy(); }
	});
	TestTrue(TEXT("Early destroyed target rejected"), Pickup->TryClaimForActor(EarlyLost.Actor, Id) == ESoulsPickupClaimResult::ClaimRejected);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	SoulsPickupTests::Available(*this, Pickup);
	TestEqual(TEXT("No incorrect event"), Events, 0);
	TestTrue(TEXT("Valid retry succeeds"), Pickup->TryClaimForActor(Target.Actor, Id) == ESoulsPickupClaimResult::Success);
	TestEqual(TEXT("Exactly one final event"), Events, 1);
	TestEqual(TEXT("Exactly one target grant"), Target.Items->GetGrantCount(), 1);
	TestEqual(TEXT("125 HP"), Target.Health->GetMaxHealth(), 125.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPickupNTest, "SoulsNew.Pickups.N.ConfirmedEvent", SoulsPickupTests::Flags)
bool FSoulsPickupNTest::RunTest(const FString& Parameters)
{
	SoulsPickupTests::FFixture F;
	ASoulsItemPickup* Pickup = F.ReadyPickup(*this);
	const auto Target = F.MakeOwner();
	if (!Pickup || !SoulsPickupTests::Ready(*this, Target)) { return false; }
	int32 Events = 0;
	Pickup->PickupClaimed().AddLambda([&](ASoulsItemPickup* P, AActor* Actor, FGuid Grant)
	{
		++Events;
		TestTrue(TEXT("Correct target and grant"), Actor == Target.Actor && Target.Items->HasGrant(Grant));
		TestTrue(TEXT("Reward committed"), P->GetReward()->GetState() == ESoulsItemRewardState::Claimed);
		TestTrue(TEXT("Physical state committed"), P->IsConsumed());
	});
	FGuid Id;
	Pickup->TryClaimForActor(nullptr, Id);
	TestEqual(TEXT("Zero on failure"), Events, 0);
	Pickup->TryClaimForActor(Target.Actor, Id);
	Pickup->TryClaimForActor(Target.Actor, Id);
	TestEqual(TEXT("Exactly one confirmed event"), Events, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPickupIntegrationTest, "SoulsNew.Pickups.IntegrationFlow", SoulsPickupTests::Flags)
bool FSoulsPickupIntegrationTest::RunTest(const FString& Parameters)
{
	SoulsPickupTests::FFixture F;
	USoulsItemReward* Reward = F.MakeReward(*this);
	ASoulsItemPickup* Pickup = F.SpawnPickup();
	const auto Target = F.MakeOwner();
	if (!Reward || !TestNotNull(TEXT("Pickup"), Pickup) || !SoulsPickupTests::Ready(*this, Target)) { return false; }
	TestEqual(TEXT("Source base 500"), F.Source.Health->GetBaseMaxHealth(), 500.0f);
	TestEqual(TEXT("Source effective 625"), F.Source.Health->GetMaxHealth(), 625.0f);
	TestTrue(TEXT("Source dead"), F.Source.Health->IsDead());
	TestTrue(TEXT("Reward available"), Reward->GetState() == ESoulsItemRewardState::Available);
	TestTrue(TEXT("Original source definition"), F.Source.Items->GetGrantDefinition(F.SourceGrant) == F.Definition.Get());
	TestTrue(TEXT("Original reward definition"), Reward->GetItemDefinition() == F.Definition.Get());
	TestTrue(TEXT("Initialize exact reward"), Pickup->InitializeReward(Reward));
	TestTrue(TEXT("Pickup holds that same reward"), Pickup->GetReward() == Reward);
	TestEqual(TEXT("Target starts at 100"), Target.Health->GetMaxHealth(), 100.0f);
	FGuid Id;
	TestTrue(TEXT("Pickup claim success"), Pickup->TryClaimForActor(Target.Actor, Id) == ESoulsPickupClaimResult::Success);
	TestEqual(TEXT("Target ends at 125"), Target.Health->GetMaxHealth(), 125.0f);
	TestTrue(TEXT("Reward claimed"), Reward->GetState() == ESoulsItemRewardState::Claimed);
	TestTrue(TEXT("Pickup consumed"), Pickup->IsConsumed());
	TestTrue(TEXT("Exact definition across full route"), Target.Items->GetGrantDefinition(Id) == F.Definition.Get());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPickupCollisionTest, "SoulsNew.Pickups.CollisionAndVisualConfiguration", SoulsPickupTests::Flags)
bool FSoulsPickupCollisionTest::RunTest(const FString& Parameters)
{
	SoulsPickupTests::FFixture F;
	ASoulsItemPickup* Pickup = F.ReadyPickup(*this);
	if (!Pickup) { return false; }
	const USphereComponent* Sphere = Pickup->GetInteractionSphere();
	TestTrue(TEXT("Sphere is root"), Pickup->GetRootComponent() == Sphere);
	TestEqual(TEXT("Interaction radius"), Sphere->GetUnscaledSphereRadius(), 75.0f);
	TestTrue(TEXT("Query only"), Sphere->GetCollisionEnabled() == ECollisionEnabled::QueryOnly);
	TestTrue(TEXT("WorldDynamic object"), Sphere->GetCollisionObjectType() == ECC_WorldDynamic);
	TestTrue(TEXT("Dynamic generic actors can overlap"), Sphere->GetCollisionResponseToChannel(ECC_WorldDynamic) == ECR_Overlap);
	TestTrue(TEXT("Pawn can overlap too"), Sphere->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Overlap);
	TestTrue(TEXT("Static actors can overlap"), Sphere->GetCollisionResponseToChannel(ECC_WorldStatic) == ECR_Overlap);
	TestTrue(TEXT("Overlap handler bound"), Sphere->OnComponentBeginOverlap.IsBound());
	TestNotNull(TEXT("Existing engine mesh loaded"), Pickup->GetVisualMesh()->GetStaticMesh().Get());
	TestTrue(TEXT("Visual attached to interaction sphere"), Pickup->GetVisualMesh()->GetAttachParent() == Sphere);
	TestTrue(TEXT("Visual has no collision"), Pickup->GetVisualMesh()->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPickupOverlapTest, "SoulsNew.Pickups.RealWorldOverlap", SoulsPickupTests::Flags)
bool FSoulsPickupOverlapTest::RunTest(const FString& Parameters)
{
	SoulsPickupTests::FFixture F(true);
	ASoulsItemPickup* Pickup = F.ReadyPickup(*this);
	const auto Target = F.MakeOwner();
	if (!Pickup || !SoulsPickupTests::Ready(*this, Target)) { return false; }
	USphereComponent* TargetSphere = NewObject<USphereComponent>(Target.Actor);
	Target.Actor->AddInstanceComponent(TargetSphere);
	Target.Actor->SetRootComponent(TargetSphere);
	TargetSphere->InitSphereRadius(25.0f);
	TargetSphere->SetCollisionObjectType(ECC_WorldDynamic);
	TargetSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TargetSphere->SetCollisionResponseToAllChannels(ECR_Overlap);
	TargetSphere->SetGenerateOverlapEvents(true);
	TargetSphere->SetWorldLocation(FVector(3000.0f, 0.0f, 0.0f));
	TargetSphere->RegisterComponent();
	int32 Events = 0;
	Pickup->PickupClaimed().AddLambda([&Events](ASoulsItemPickup*, AActor*, FGuid) { ++Events; });
	TestTrue(TEXT("Pickup began play"), Pickup->HasActorBegunPlay());
	TestTrue(TEXT("Target began play"), Target.Actor->HasActorBegunPlay());
	TestFalse(TEXT("Initially separate"), Pickup->IsConsumed());
	// A normal fixed game frame publishes registered shapes to Chaos's query scene.
	F.World->Tick(LEVELTICK_All, 1.0f / 60.0f);
	TestTrue(TEXT("Pickup collision registered"), Pickup->GetInteractionSphere()->IsRegistered());
	TestTrue(TEXT("Pickup physics state created"), Pickup->GetInteractionSphere()->IsPhysicsStateCreated());
	TestTrue(TEXT("Overlap delegate bound to this instance"), Pickup->GetInteractionSphere()->OnComponentBeginOverlap.Contains(Pickup, FName(TEXT("HandleBeginOverlap"))));
	TArray<FOverlapResult> QueryHits;
	F.World->OverlapMultiByObjectType(QueryHits, Pickup->GetActorLocation(), FQuat::Identity,
		FCollisionObjectQueryParams(ECC_WorldDynamic), FCollisionShape::MakeSphere(25.0f));
	TestTrue(TEXT("Pickup geometry present in real query scene"), QueryHits.ContainsByPredicate([Pickup](const FOverlapResult& Hit) { return Hit.GetActor() == Pickup; }));
	Target.Actor->SetActorLocation(Pickup->GetActorLocation(), false);
	// Engine collision queries produce the overlap notification; never broadcast or call Claim manually.
	TargetSphere->UpdateOverlaps();
	TestEqual(TEXT("Actual overlap invoked pickup callback"), Events, 1);
	TestEqual(TEXT("Actual overlap applied item"), Target.Health->GetMaxHealth(), 125.0f);
	TestTrue(TEXT("Actual overlap consumed pickup"), Pickup->IsConsumed());
	TestTrue(TEXT("Logical reward claimed"), Pickup->GetReward()->GetState() == ESoulsItemRewardState::Claimed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPickupInvalidRewardTest, "SoulsNew.Pickups.InvalidRewardCases", SoulsPickupTests::Flags)
bool FSoulsPickupInvalidRewardTest::RunTest(const FString& Parameters)
{
	SoulsPickupTests::FFixture F;
	ASoulsItemPickup* Pickup = F.SpawnPickup();
	const auto Target = F.MakeOwner();
	if (!Pickup || !SoulsPickupTests::Ready(*this, Target)) { return false; }
	FGuid Id;
	TestTrue(TEXT("Uninitialized interaction rejected"), Pickup->TryClaimForActor(Target.Actor, Id) == ESoulsPickupClaimResult::InvalidReward);
	TestFalse(TEXT("Null assignment rejected"), Pickup->InitializeReward(nullptr));
	TStrongObjectPtr<USoulsItemReward> EmptyReward(NewObject<USoulsItemReward>());
	TestFalse(TEXT("Unconfigured reward rejected"), Pickup->InitializeReward(EmptyReward.Get()));
	USoulsItemReward* Reward = F.MakeReward(*this);
	if (!Reward || !TestTrue(TEXT("Valid assignment after invalid attempts"), Pickup->InitializeReward(Reward))) { return false; }
	// Simulate corrupt/invalid transient data without introducing production mutation APIs.
	FObjectPropertyBase* DefinitionProperty = FindFProperty<FObjectPropertyBase>(USoulsItemReward::StaticClass(), TEXT("Definition"));
	if (!TestNotNull(TEXT("Reflected definition property"), DefinitionProperty)) { return false; }
	DefinitionProperty->SetObjectPropertyValue_InContainer(Reward, nullptr);
	TestTrue(TEXT("Lost definition rejected"), Pickup->TryClaimForActor(Target.Actor, Id) == ESoulsPickupClaimResult::InvalidReward);
	TestFalse(TEXT("Not incorrectly consumed"), Pickup->IsConsumed());
	TestEqual(TEXT("No partial grant"), Target.Items->GetGrantCount(), 0);
	TestTrue(TEXT("Reward still available"), Reward->GetState() == ESoulsItemRewardState::Available);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPickupAssignmentTest, "SoulsNew.Pickups.SingleRewardAssignment", SoulsPickupTests::Flags)
bool FSoulsPickupAssignmentTest::RunTest(const FString& Parameters)
{
	SoulsPickupTests::FFixture F;
	ASoulsItemPickup* Pickup = F.ReadyPickup(*this);
	if (!Pickup) { return false; }
	USoulsItemReward* First = Pickup->GetReward();
	USoulsItemReward* Second = F.MakeReward(*this);
	if (!Second) { return false; }
	TestFalse(TEXT("No same assignment repeated"), Pickup->InitializeReward(First));
	TestFalse(TEXT("No replacement with another reward"), Pickup->InitializeReward(Second));
	TestTrue(TEXT("Original retained"), Pickup->GetReward() == First);
	SoulsPickupTests::Available(*this, Pickup);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPickupDestroyCallbackTest, "SoulsNew.Pickups.DestroyDuringCallbacks", SoulsPickupTests::Flags)
bool FSoulsPickupDestroyCallbackTest::RunTest(const FString& Parameters)
{
	SoulsPickupTests::FFixture F;
	ASoulsItemPickup* EarlyPickup = F.ReadyPickup(*this);
	const auto EarlyTarget = F.MakeOwner();
	if (!EarlyPickup || !SoulsPickupTests::Ready(*this, EarlyTarget)) { return false; }
	int32 EarlyEvents = 0;
	EarlyPickup->PickupClaimed().AddLambda([&EarlyEvents](ASoulsItemPickup*, AActor*, FGuid) { ++EarlyEvents; });
	EarlyTarget.Items->ItemGranted().AddLambda([&EarlyPickup](USoulsItemComponent*, FGuid, FName) { EarlyPickup->Destroy(); });
	FGuid Id;
	TestTrue(TEXT("Confirmed grant survives physical destruction"), EarlyPickup->TryClaimForActor(EarlyTarget.Actor, Id) == ESoulsPickupClaimResult::Success);
	TestTrue(TEXT("Consumed even if destroyed during grant callback"), EarlyPickup->IsConsumed());
	TestEqual(TEXT("One successful native notification"), EarlyEvents, 1);
	TestEqual(TEXT("No lost or duplicate grant"), EarlyTarget.Items->GetGrantCount(), 1);
	TestEqual(TEXT("Effect retained"), EarlyTarget.Health->GetMaxHealth(), 125.0f);

	ASoulsItemPickup* LatePickup = F.ReadyPickup(*this);
	const auto LateTarget = F.MakeOwner();
	if (!LatePickup || !SoulsPickupTests::Ready(*this, LateTarget)) { return false; }
	int32 LateEvents = 0;
	LatePickup->PickupClaimed().AddLambda([&](ASoulsItemPickup* P, AActor* Candidate, FGuid)
	{
		++LateEvents;
		P->Destroy();
		FGuid Recursive;
		TestTrue(TEXT("Reentry after callback destroy remains blocked"), P->TryClaimForActor(Candidate, Recursive) == ESoulsPickupClaimResult::Busy);
	});
	TestTrue(TEXT("Claim event may safely destroy actor"), LatePickup->TryClaimForActor(LateTarget.Actor, Id) == ESoulsPickupClaimResult::Success);
	TestEqual(TEXT("Single pickup notification"), LateEvents, 1);
	TestEqual(TEXT("Single grant"), LateTarget.Items->GetGrantCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPickupSharedRewardTest, "SoulsNew.Pickups.SharedRewardSingleGrant", SoulsPickupTests::Flags)
bool FSoulsPickupSharedRewardTest::RunTest(const FString& Parameters)
{
	SoulsPickupTests::FFixture F;
	ASoulsItemPickup* First = F.ReadyPickup(*this);
	ASoulsItemPickup* Second = F.SpawnPickup();
	const auto Target = F.MakeOwner();
	if (!First || !Second || !SoulsPickupTests::Ready(*this, Target)) { return false; }
	TestTrue(TEXT("Same reward held by another physical instance"), Second->InitializeReward(First->GetReward()));
	FGuid Id;
	TestTrue(TEXT("First succeeds"), First->TryClaimForActor(Target.Actor, Id) == ESoulsPickupClaimResult::Success);
	TestTrue(TEXT("Shared reward remains single authority"), Second->TryClaimForActor(Target.Actor, Id) == ESoulsPickupClaimResult::AlreadyClaimed);
	TestEqual(TEXT("One grant across both pickups"), Target.Items->GetGrantCount(), 1);
	TestEqual(TEXT("125 not 150"), Target.Health->GetMaxHealth(), 125.0f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
