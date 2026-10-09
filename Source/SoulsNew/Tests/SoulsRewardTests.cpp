#include "Items/SoulsItemReward.h"
#include "Items/SoulsRewardComponent.h"
#include "Items/SoulsItemComponent.h"
#include "Items/SoulsItemDefinition.h"
#include "Attributes/SoulsHealthComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Async/TaskGraphInterfaces.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "UObject/GarbageCollection.h"
#include "UObject/StrongObjectPtr.h"

namespace SoulsRewardTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext
		| EAutomationTestFlags::CommandletContext | EAutomationTestFlags::EngineFilter;

	FSoulsItemDefinitionData VitalityData()
	{
		FSoulsItemDefinitionData Data;
		Data.ItemId = TEXT("Item.Vitality");
		Data.DisplayName = FText::FromString(TEXT("Vitalidad"));
		FSoulsItemEffect Effect;
		Effect.Attribute = ESoulsItemAttribute::MaxHealth;
		Effect.Operation = ESoulsAttributeModifierOperation::AdditivePercent;
		Effect.Magnitude = 0.25f;
		Data.Effects.Add(Effect);
		return Data;
	}

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

		FFixture() : Definition(USoulsItemDefinition::CreateTransient(VitalityData()))
		{
			const UWorld::InitializationValues Values = UWorld::InitializationValues()
				.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false)
				.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			if (World && GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
		}
		~FFixture()
		{
			if (World)
			{
				World->DestroyWorld(false);
				if (GEngine) { GEngine->DestroyWorldContext(World); }
			}
		}

		FOwner MakeOwner(float Base = 100.0f, bool bRewards = false, bool bHealth = true)
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
	};

	bool Ready(FAutomationTestBase& Test, const FOwner& Owner)
	{
		return Test.TestNotNull(TEXT("Actor"), Owner.Actor) && Test.TestNotNull(TEXT("Health"), Owner.Health)
			&& Test.TestNotNull(TEXT("Items"), Owner.Items);
	}

	bool PrepareSource(FAutomationTestBase& Test, FFixture& Fixture, FOwner& Source, FGuid& GrantId)
	{
		Source = Fixture.MakeOwner(500.0f, true);
		if (!Ready(Test, Source) || !Test.TestNotNull(TEXT("Rewards"), Source.Rewards)) { return false; }
		if (!Test.TestTrue(TEXT("Grant source"), Source.Items->GrantItem(Fixture.Definition.Get(), GrantId) == ESoulsItemResult::Success)) { return false; }
		return Test.TestTrue(TEXT("Configure selected grant"), Source.Rewards->ConfigureSourceGrant(GrantId) == ESoulsRewardSourceResult::Success);
	}

	USoulsItemReward* Kill(FAutomationTestBase& Test, const FOwner& Source)
	{
		Test.TestEqual(TEXT("Lethal damage applied"), Source.Health->TakeDamage(10000.0f), Source.Health->GetMaxHealth());
		USoulsItemReward* Reward = Source.Rewards->GetReward();
		Test.TestNotNull(TEXT("Generated reward"), Reward);
		return Reward;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsRewardATest, "SoulsNew.Rewards.A.GeneratedOnDeath", SoulsRewardTests::Flags)
bool FSoulsRewardATest::RunTest(const FString& Parameters)
{
	SoulsRewardTests::FFixture Fixture;
	SoulsRewardTests::FOwner Source;
	FGuid Id;
	if (!SoulsRewardTests::PrepareSource(*this, Fixture, Source, Id)) { return false; }
	TestEqual(TEXT("Source 500 -> 625"), Source.Health->GetMaxHealth(), 625.0f);
	TestNull(TEXT("No reward before death"), Source.Rewards->GetReward());
	USoulsItemReward* Reward = SoulsRewardTests::Kill(*this, Source);
	if (!Reward) { return false; }
	TestTrue(TEXT("Available"), Reward->GetState() == ESoulsItemRewardState::Available);
	TestTrue(TEXT("Valid RewardId"), Reward->GetRewardId().IsValid());
	TestTrue(TEXT("Generation confirmed"), Source.Rewards->GetLastGenerationResult() == ESoulsRewardSourceResult::Success);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsRewardBTest, "SoulsNew.Rewards.B.SameDefinitionInstance", SoulsRewardTests::Flags)
bool FSoulsRewardBTest::RunTest(const FString& Parameters)
{
	SoulsRewardTests::FFixture Fixture;
	SoulsRewardTests::FOwner Source;
	FGuid Id;
	if (!SoulsRewardTests::PrepareSource(*this, Fixture, Source, Id)) { return false; }
	USoulsItemDefinition* SourceDefinition = Source.Items->GetGrantDefinition(Id);
	USoulsItemReward* Reward = SoulsRewardTests::Kill(*this, Source);
	if (!Reward) { return false; }
	TestTrue(TEXT("Exactly source grant definition"), Reward->GetItemDefinition() == SourceDefinition);
	TestTrue(TEXT("Exactly original UObject, not an equivalent copy"), Reward->GetItemDefinition() == Fixture.Definition.Get());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsRewardCTest, "SoulsNew.Rewards.C.SingleDeathReward", SoulsRewardTests::Flags)
bool FSoulsRewardCTest::RunTest(const FString& Parameters)
{
	SoulsRewardTests::FFixture Fixture;
	SoulsRewardTests::FOwner Source;
	FGuid Id;
	if (!SoulsRewardTests::PrepareSource(*this, Fixture, Source, Id)) { return false; }
	int32 Generated = 0;
	Source.Rewards->RewardGenerated().AddLambda([&Generated](USoulsRewardComponent*, USoulsItemReward*) { ++Generated; });
	USoulsItemReward* Reward = SoulsRewardTests::Kill(*this, Source);
	if (!Reward) { return false; }
	const FGuid RewardId = Reward->GetRewardId();
	Source.Health->TakeDamage(10000.0f);
	Source.Health->TakeDamage(1.0f);
	// Also defend against duplicated native notification; normal damage already latches Death.
	Source.Health->Death().Broadcast(Source.Health);
	TestEqual(TEXT("Exactly one generated event"), Generated, 1);
	TestTrue(TEXT("Same reward object"), Source.Rewards->GetReward() == Reward);
	TestEqual(TEXT("Same reward ID"), Source.Rewards->GetReward()->GetRewardId(), RewardId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsRewardDTest, "SoulsNew.Rewards.D.SourceGrantWithdrawal", SoulsRewardTests::Flags)
bool FSoulsRewardDTest::RunTest(const FString& Parameters)
{
	SoulsRewardTests::FFixture Fixture;
	SoulsRewardTests::FOwner Source;
	FGuid Id;
	if (!SoulsRewardTests::PrepareSource(*this, Fixture, Source, Id)) { return false; }
	USoulsItemReward* Reward = SoulsRewardTests::Kill(*this, Source);
	if (!Reward) { return false; }
	TestTrue(TEXT("Withdraw original grant"), Source.Items->RemoveItem(Id) == ESoulsItemResult::Success);
	TestEqual(TEXT("Source 625 -> 500"), Source.Health->GetMaxHealth(), 500.0f);
	TestEqual(TEXT("Source stays dead"), Source.Health->GetCurrentHealth(), 0.0f);
	TestNull(TEXT("No source grant lookup"), Source.Items->GetGrantDefinition(Id));
	TestTrue(TEXT("Reward retains same definition independently"), Reward->GetItemDefinition() == Fixture.Definition.Get());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsRewardETest, "SoulsNew.Rewards.E.ClaimTo100", SoulsRewardTests::Flags)
bool FSoulsRewardETest::RunTest(const FString& Parameters)
{
	SoulsRewardTests::FFixture Fixture;
	SoulsRewardTests::FOwner Source;
	FGuid Id;
	if (!SoulsRewardTests::PrepareSource(*this, Fixture, Source, Id)) { return false; }
	const auto Target = Fixture.MakeOwner();
	if (!SoulsRewardTests::Ready(*this, Target)) { return false; }
	USoulsItemReward* Reward = SoulsRewardTests::Kill(*this, Source);
	if (!Reward) { return false; }
	FGuid TargetId;
	TestTrue(TEXT("Claim success"), Reward->Claim(Target.Items, TargetId) == ESoulsRewardClaimResult::Success);
	TestEqual(TEXT("Target 100 -> 125"), Target.Health->GetMaxHealth(), 125.0f);
	TestTrue(TEXT("Claimed"), Reward->GetState() == ESoulsItemRewardState::Claimed);
	TestTrue(TEXT("Target possesses new grant"), Target.Items->HasGrant(TargetId));
	TestEqual(TEXT("Reward remembers confirmed target grant"), Reward->GetClaimedGrantId(), TargetId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsRewardFTest, "SoulsNew.Rewards.F.IndependentIds", SoulsRewardTests::Flags)
bool FSoulsRewardFTest::RunTest(const FString& Parameters)
{
	SoulsRewardTests::FFixture Fixture;
	SoulsRewardTests::FOwner Source;
	FGuid SourceId;
	if (!SoulsRewardTests::PrepareSource(*this, Fixture, Source, SourceId)) { return false; }
	const auto Target = Fixture.MakeOwner();
	if (!SoulsRewardTests::Ready(*this, Target)) { return false; }
	USoulsItemReward* Reward = SoulsRewardTests::Kill(*this, Source);
	if (!Reward) { return false; }
	FGuid TargetId;
	if (!TestTrue(TEXT("Claim"), Reward->Claim(Target.Items, TargetId) == ESoulsRewardClaimResult::Success)) { return false; }
	FSoulsItemGrantSnapshot SourceGrant, TargetGrant;
	TestTrue(TEXT("Source snapshot"), Source.Items->FindGrant(SourceId, SourceGrant));
	TestTrue(TEXT("Target snapshot"), Target.Items->FindGrant(TargetId, TargetGrant));
	TestEqual(TEXT("One source modifier"), SourceGrant.ModifierIds.Num(), 1);
	TestEqual(TEXT("One target modifier"), TargetGrant.ModifierIds.Num(), 1);
	if (SourceGrant.ModifierIds.Num() != 1 || TargetGrant.ModifierIds.Num() != 1) { return false; }
	const TArray<FGuid> Ids = { SourceId, SourceGrant.ModifierIds[0], Reward->GetRewardId(), TargetId, TargetGrant.ModifierIds[0] };
	for (int32 Left = 0; Left < Ids.Num(); ++Left)
	{
		TestTrue(TEXT("All IDs valid"), Ids[Left].IsValid());
		for (int32 Right = Left + 1; Right < Ids.Num(); ++Right) { TestTrue(TEXT("All runtime identities distinct"), Ids[Left] != Ids[Right]); }
	}
	TestTrue(TEXT("Definition identity preserved"), Source.Items->GetGrantDefinition(SourceId) == Target.Items->GetGrantDefinition(TargetId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsRewardGTest, "SoulsNew.Rewards.G.DoubleClaimRejected", SoulsRewardTests::Flags)
bool FSoulsRewardGTest::RunTest(const FString& Parameters)
{
	SoulsRewardTests::FFixture Fixture;
	SoulsRewardTests::FOwner Source;
	FGuid SourceId;
	if (!SoulsRewardTests::PrepareSource(*this, Fixture, Source, SourceId)) { return false; }
	const auto Target = Fixture.MakeOwner();
	const auto OtherTarget = Fixture.MakeOwner();
	if (!SoulsRewardTests::Ready(*this, Target) || !SoulsRewardTests::Ready(*this, OtherTarget)) { return false; }
	USoulsItemReward* Reward = SoulsRewardTests::Kill(*this, Source);
	if (!Reward) { return false; }
	FGuid First, Second = FGuid::NewGuid();
	TestTrue(TEXT("First success"), Reward->Claim(Target.Items, First) == ESoulsRewardClaimResult::Success);
	TestTrue(TEXT("Second rejected"), Reward->Claim(Target.Items, Second) == ESoulsRewardClaimResult::AlreadyClaimed);
	TestFalse(TEXT("Failure output invalid"), Second.IsValid());
	TestEqual(TEXT("No duplicate +25%"), Target.Health->GetMaxHealth(), 125.0f);
	TestEqual(TEXT("One target grant"), Target.Items->GetGrantCount(), 1);
	TestTrue(TEXT("Other target also rejected"), Reward->Claim(OtherTarget.Items, Second) == ESoulsRewardClaimResult::AlreadyClaimed);
	TestEqual(TEXT("Other target untouched"), OtherTarget.Health->GetMaxHealth(), 100.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsRewardHTest, "SoulsNew.Rewards.H.FailedClaimRetry", SoulsRewardTests::Flags)
bool FSoulsRewardHTest::RunTest(const FString& Parameters)
{
	SoulsRewardTests::FFixture Fixture;
	SoulsRewardTests::FOwner Source;
	FGuid SourceId;
	if (!SoulsRewardTests::PrepareSource(*this, Fixture, Source, SourceId)) { return false; }
	const auto BadTarget = Fixture.MakeOwner(100.0f, false, false);
	const auto Target = Fixture.MakeOwner();
	if (!TestNotNull(TEXT("Incompatible target"), BadTarget.Items) || !SoulsRewardTests::Ready(*this, Target)) { return false; }
	USoulsItemReward* Reward = SoulsRewardTests::Kill(*this, Source);
	if (!Reward) { return false; }
	FGuid Id;
	TestTrue(TEXT("Null target fails"), Reward->Claim(nullptr, Id) == ESoulsRewardClaimResult::InvalidTarget);
	TestTrue(TEXT("Missing Health fails"), Reward->Claim(BadTarget.Items, Id) == ESoulsRewardClaimResult::GrantRejected);
	TestFalse(TEXT("Failure output invalid"), Id.IsValid());
	TestEqual(TEXT("No partial grant"), BadTarget.Items->GetGrantCount(), 0);
	TestTrue(TEXT("Still available"), Reward->GetState() == ESoulsItemRewardState::Available);
	TestTrue(TEXT("No confirmed target grant"), !Reward->GetClaimedGrantId().IsValid());
	TestTrue(TEXT("Valid retry succeeds"), Reward->Claim(Target.Items, Id) == ESoulsRewardClaimResult::Success);
	TestEqual(TEXT("Valid target receives effect"), Target.Health->GetMaxHealth(), 125.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsRewardITest, "SoulsNew.Rewards.I.ConfirmedEventsOnly", SoulsRewardTests::Flags)
bool FSoulsRewardITest::RunTest(const FString& Parameters)
{
	SoulsRewardTests::FFixture Fixture;
	SoulsRewardTests::FOwner Source;
	FGuid SourceId;
	if (!SoulsRewardTests::PrepareSource(*this, Fixture, Source, SourceId)) { return false; }
	const auto Target = Fixture.MakeOwner();
	if (!SoulsRewardTests::Ready(*this, Target)) { return false; }
	int32 Generated = 0, Claimed = 0;
	Source.Rewards->RewardGenerated().AddLambda([this, &Generated](USoulsRewardComponent* Component, USoulsItemReward* Reward)
	{
		++Generated;
		TestTrue(TEXT("Generation committed before event"), Component->GetReward() == Reward && Component->HasHandledDeath());
	});
	USoulsItemReward* Reward = SoulsRewardTests::Kill(*this, Source);
	if (!Reward) { return false; }
	Reward->RewardClaimed().AddLambda([this, &Claimed, &Fixture](USoulsItemReward* Value, USoulsItemComponent* Items, FGuid Id)
	{
		++Claimed;
		TestTrue(TEXT("State committed before event"), Value->GetState() == ESoulsItemRewardState::Claimed);
		TestTrue(TEXT("Grant committed before event"), Items->GetGrantDefinition(Id) == Fixture.Definition.Get());
	});
	FGuid Id;
	TestTrue(TEXT("Failed claim"), Reward->Claim(nullptr, Id) == ESoulsRewardClaimResult::InvalidTarget);
	TestEqual(TEXT("No failure event"), Claimed, 0);
	TestTrue(TEXT("Valid claim"), Reward->Claim(Target.Items, Id) == ESoulsRewardClaimResult::Success);
	Source.Health->TakeDamage(5.0f);
	TestTrue(TEXT("Repeated claim fails"), Reward->Claim(Target.Items, Id) == ESoulsRewardClaimResult::AlreadyClaimed);
	TestEqual(TEXT("Generated once"), Generated, 1);
	TestEqual(TEXT("Claimed once"), Claimed, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsRewardJTest, "SoulsNew.Rewards.J.GenericActors", SoulsRewardTests::Flags)
bool FSoulsRewardJTest::RunTest(const FString& Parameters)
{
	SoulsRewardTests::FFixture Fixture;
	SoulsRewardTests::FOwner Source;
	FGuid SourceId;
	if (!SoulsRewardTests::PrepareSource(*this, Fixture, Source, SourceId)) { return false; }
	const auto Target = Fixture.MakeOwner();
	if (!SoulsRewardTests::Ready(*this, Target)) { return false; }
	TestTrue(TEXT("Source exactly AActor"), Source.Actor->GetClass() == AActor::StaticClass());
	TestTrue(TEXT("Target exactly AActor"), Target.Actor->GetClass() == AActor::StaticClass());
	TestFalse(TEXT("Reward component has no Tick"), Source.Rewards->PrimaryComponentTick.bCanEverTick);
	USoulsItemReward* Reward = SoulsRewardTests::Kill(*this, Source);
	if (!Reward) { return false; }
	FGuid Id;
	TestTrue(TEXT("No controller/Character needed"), Reward->Claim(Target.Items, Id) == ESoulsRewardClaimResult::Success);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsRewardKTest, "SoulsNew.Rewards.K.SourceTargetIsolation", SoulsRewardTests::Flags)
bool FSoulsRewardKTest::RunTest(const FString& Parameters)
{
	SoulsRewardTests::FFixture Fixture;
	SoulsRewardTests::FOwner Source;
	FGuid SourceId;
	if (!SoulsRewardTests::PrepareSource(*this, Fixture, Source, SourceId)) { return false; }
	const auto Target = Fixture.MakeOwner();
	if (!SoulsRewardTests::Ready(*this, Target)) { return false; }
	USoulsItemReward* Reward = SoulsRewardTests::Kill(*this, Source);
	if (!Reward) { return false; }
	FGuid TargetId;
	if (!TestTrue(TEXT("Claim"), Reward->Claim(Target.Items, TargetId) == ESoulsRewardClaimResult::Success)) { return false; }
	TestTrue(TEXT("Withdraw source"), Source.Items->RemoveItem(SourceId) == ESoulsItemResult::Success);
	TestTrue(TEXT("Change source base"), Source.Health->SetBaseMaxHealth(200.0f));
	TestEqual(TEXT("Source only"), Source.Health->GetMaxHealth(), 200.0f);
	TestEqual(TEXT("Target unaffected"), Target.Health->GetMaxHealth(), 125.0f);
	TestTrue(TEXT("Target still owns same definition"), Target.Items->GetGrantDefinition(TargetId) == Fixture.Definition.Get());
	TestTrue(TEXT("Withdraw target separately"), Target.Items->RemoveItem(TargetId) == ESoulsItemResult::Success);
	TestEqual(TEXT("Target restores independently"), Target.Health->GetMaxHealth(), 100.0f);
	TestTrue(TEXT("Reward remains consumed after item withdrawal"), Reward->GetState() == ESoulsItemRewardState::Claimed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsRewardLTest, "SoulsNew.Rewards.L.GenerationAndClaimReentrancy", SoulsRewardTests::Flags)
bool FSoulsRewardLTest::RunTest(const FString& Parameters)
{
	SoulsRewardTests::FFixture Fixture;
	SoulsRewardTests::FOwner Source;
	FGuid SourceId;
	if (!SoulsRewardTests::PrepareSource(*this, Fixture, Source, SourceId)) { return false; }
	const auto Target = Fixture.MakeOwner();
	if (!SoulsRewardTests::Ready(*this, Target)) { return false; }
	int32 Generated = 0, ItemGrants = 0, Claims = 0;
	Target.Items->ItemGranted().AddLambda([this, &Source, &Target, &ItemGrants](USoulsItemComponent*, FGuid, FName)
	{
		++ItemGrants;
		USoulsItemReward* Reward = Source.Rewards->GetReward();
		TestTrue(TEXT("Not marked claimed before GrantItem completes"), Reward->GetState() == ESoulsItemRewardState::Available);
		FGuid Nested;
		TestTrue(TEXT("Reentrant claim while granting is Busy"), Reward->Claim(Target.Items, Nested) == ESoulsRewardClaimResult::Busy);
		TestFalse(TEXT("Nested output invalid"), Nested.IsValid());
	});
	Source.Rewards->RewardGenerated().AddLambda([this, &Source, &SourceId, &Target, &Generated, &Claims](USoulsRewardComponent* Component, USoulsItemReward* Reward)
	{
		++Generated;
		Source.Health->Death().Broadcast(Source.Health);
		TestTrue(TEXT("Reentrant configure blocked"), Component->ConfigureSourceGrant(SourceId) == ESoulsRewardSourceResult::Busy);
		TestTrue(TEXT("Reentrant rearm blocked"), Component->Rearm() == ESoulsRewardSourceResult::Busy);
		Reward->RewardClaimed().AddLambda([this, &Claims, &Target](USoulsItemReward* Value, USoulsItemComponent*, FGuid)
		{
			++Claims;
			FGuid Nested;
			TestTrue(TEXT("Reentrant claim from claimed event blocked"), Value->Claim(Target.Items, Nested) == ESoulsRewardClaimResult::Busy);
		});
		FGuid Id;
		TestTrue(TEXT("Generation observer may claim confirmed reward"), Reward->Claim(Target.Items, Id) == ESoulsRewardClaimResult::Success);
	});
	if (!SoulsRewardTests::Kill(*this, Source)) { return false; }
	TestEqual(TEXT("One reward"), Generated, 1);
	TestEqual(TEXT("One target grant"), ItemGrants, 1);
	TestEqual(TEXT("One claim"), Claims, 1);
	TestEqual(TEXT("One effect"), Target.Health->GetMaxHealth(), 125.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsRewardEndToEndTest, "SoulsNew.Rewards.EndToEnd", SoulsRewardTests::Flags)
bool FSoulsRewardEndToEndTest::RunTest(const FString& Parameters)
{
	// One transient definition, two generic Actors, no assets, physical reward or special classes.
	SoulsRewardTests::FFixture Fixture;
	const auto Source = Fixture.MakeOwner(500.0f, true);
	if (!SoulsRewardTests::Ready(*this, Source) || !TestNotNull(TEXT("Rewards"), Source.Rewards)) { return false; }
	FGuid SourceGrant;
	TestTrue(TEXT("Grant same Vitality definition"), Source.Items->GrantItem(Fixture.Definition.Get(), SourceGrant) == ESoulsItemResult::Success);
	TestEqual(TEXT("Boss logical 500 -> 625"), Source.Health->GetMaxHealth(), 625.0f);
	TestTrue(TEXT("Configure chosen grant"), Source.Rewards->ConfigureSourceGrant(SourceGrant) == ESoulsRewardSourceResult::Success);
	Source.Health->TakeDamage(625.0f);
	USoulsItemReward* Reward = Source.Rewards->GetReward();
	if (!TestNotNull(TEXT("Logical reward"), Reward)) { return false; }
	TestTrue(TEXT("Available"), Reward->GetState() == ESoulsItemRewardState::Available);
	TestTrue(TEXT("Exact definition captured"), Reward->GetItemDefinition() == Fixture.Definition.Get());
	const auto Target = Fixture.MakeOwner(100.0f);
	if (!SoulsRewardTests::Ready(*this, Target)) { return false; }
	TestEqual(TEXT("Player logical base"), Target.Health->GetMaxHealth(), 100.0f);
	FGuid TargetGrant;
	TestTrue(TEXT("Claim"), Reward->Claim(Target.Items, TargetGrant) == ESoulsRewardClaimResult::Success);
	TestEqual(TEXT("Player logical 100 -> 125"), Target.Health->GetMaxHealth(), 125.0f);
	TestTrue(TEXT("Claimed"), Reward->GetState() == ESoulsItemRewardState::Claimed);
	TestTrue(TEXT("Exact definition in final grant"), Target.Items->GetGrantDefinition(TargetGrant) == Fixture.Definition.Get());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsRewardResetTest, "SoulsNew.Rewards.ResetNeedsExplicitRearm", SoulsRewardTests::Flags)
bool FSoulsRewardResetTest::RunTest(const FString& Parameters)
{
	SoulsRewardTests::FFixture Fixture;
	SoulsRewardTests::FOwner Source;
	FGuid SourceId;
	if (!SoulsRewardTests::PrepareSource(*this, Fixture, Source, SourceId)) { return false; }
	const auto Target = Fixture.MakeOwner();
	if (!SoulsRewardTests::Ready(*this, Target)) { return false; }
	int32 Generated = 0;
	Source.Rewards->RewardGenerated().AddLambda([&Generated](USoulsRewardComponent*, USoulsItemReward*) { ++Generated; });
	TStrongObjectPtr<USoulsItemReward> First(SoulsRewardTests::Kill(*this, Source));
	if (!First.IsValid()) { return false; }
	FGuid TargetId;
	TestTrue(TEXT("Claim first"), First->Claim(Target.Items, TargetId) == ESoulsRewardClaimResult::Success);
	TestTrue(TEXT("Cannot rearm while dead"), Source.Rewards->Rearm() == ESoulsRewardSourceResult::OwnerDead);
	TestTrue(TEXT("Reset Health"), Source.Health->ResetHealthForNewLife());
	Source.Health->TakeDamage(10000.0f);
	TestEqual(TEXT("Reset alone generates nothing new"), Generated, 1);
	TestTrue(TEXT("Original reward preserved"), Source.Rewards->GetReward() == First.Get());
	TestTrue(TEXT("Reset again"), Source.Health->ResetHealthForNewLife());
	TestTrue(TEXT("Explicit rearm"), Source.Rewards->Rearm() == ESoulsRewardSourceResult::Success);
	TestNull(TEXT("New cycle has no reward yet"), Source.Rewards->GetReward());
	TestTrue(TEXT("Repeated rearm while armed is harmless"), Source.Rewards->Rearm() == ESoulsRewardSourceResult::Success);
	USoulsItemReward* Second = SoulsRewardTests::Kill(*this, Source);
	if (!Second) { return false; }
	TestEqual(TEXT("Two explicitly armed deaths"), Generated, 2);
	TestTrue(TEXT("Fresh RewardId"), Second->GetRewardId() != First->GetRewardId());
	TestTrue(TEXT("New reward available"), Second->GetState() == ESoulsItemRewardState::Available);
	TestTrue(TEXT("Old reward remains claimed"), First->GetState() == ESoulsItemRewardState::Claimed);
	TestTrue(TEXT("Same definition across cycles"), Second->GetItemDefinition() == First->GetItemDefinition());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsRewardPendingTest, "SoulsNew.Rewards.RearmPreservesPendingReward", SoulsRewardTests::Flags)
bool FSoulsRewardPendingTest::RunTest(const FString& Parameters)
{
	SoulsRewardTests::FFixture Fixture;
	SoulsRewardTests::FOwner Source;
	FGuid SourceId;
	if (!SoulsRewardTests::PrepareSource(*this, Fixture, Source, SourceId)) { return false; }
	USoulsItemReward* Reward = SoulsRewardTests::Kill(*this, Source);
	if (!Reward) { return false; }
	TestTrue(TEXT("Reset"), Source.Health->ResetHealthForNewLife());
	TestTrue(TEXT("Unclaimed reward prevents loss on rearm"), Source.Rewards->Rearm() == ESoulsRewardSourceResult::PendingReward);
	TestTrue(TEXT("Pending reward retained"), Source.Rewards->GetReward() == Reward);
	TestTrue(TEXT("Pending reward still available"), Reward->GetState() == ESoulsItemRewardState::Available);
	Source.Health->TakeDamage(10000.0f);
	TestTrue(TEXT("No replacement after rejected rearm"), Source.Rewards->GetReward() == Reward);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsRewardGCTest, "SoulsNew.Rewards.DefinitionGCLifetime", SoulsRewardTests::Flags)
bool FSoulsRewardGCTest::RunTest(const FString& Parameters)
{
	SoulsRewardTests::FFixture Fixture;
	SoulsRewardTests::FOwner Source;
	FGuid SourceId;
	if (!SoulsRewardTests::PrepareSource(*this, Fixture, Source, SourceId)) { return false; }
	TWeakObjectPtr<USoulsItemDefinition> WeakDefinition(Fixture.Definition.Get());
	TStrongObjectPtr<USoulsItemReward> Reward(SoulsRewardTests::Kill(*this, Source));
	if (!Reward.IsValid()) { return false; }
	TestTrue(TEXT("Withdraw original grant"), Source.Items->RemoveItem(SourceId) == ESoulsItemResult::Success);
	Fixture.Definition.Reset();
	TestTrue(TEXT("Destroy source"), Source.Actor->Destroy());
	CollectGarbage(RF_NoFlags);
	TestTrue(TEXT("Standalone reward retains definition"), WeakDefinition.IsValid());
	TestTrue(TEXT("Exact retained instance"), Reward->GetItemDefinition() == WeakDefinition.Get());
	Reward.Reset();
	CollectGarbage(RF_NoFlags);
	TestFalse(TEXT("No reference leak after reward released"), WeakDefinition.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsRewardDestroyTest, "SoulsNew.Rewards.DestroyDoesNotGenerate", SoulsRewardTests::Flags)
bool FSoulsRewardDestroyTest::RunTest(const FString& Parameters)
{
	SoulsRewardTests::FFixture Fixture;
	SoulsRewardTests::FOwner Source;
	FGuid SourceId;
	if (!SoulsRewardTests::PrepareSource(*this, Fixture, Source, SourceId)) { return false; }
	int32 Generated = 0;
	Source.Rewards->RewardGenerated().AddLambda([&Generated](USoulsRewardComponent*, USoulsItemReward*) { ++Generated; });
	Source.Rewards->DestroyComponent();
	Source.Health->TakeDamage(10000.0f);
	TestEqual(TEXT("Destroyed component unsubscribes from Death"), Generated, 0);
	TestTrue(TEXT("Ended component unavailable"), Source.Rewards->ConfigureSourceGrant(SourceId) == ESoulsRewardSourceResult::Unavailable);
	SoulsRewardTests::FOwner Other;
	FGuid OtherId;
	if (!SoulsRewardTests::PrepareSource(*this, Fixture, Other, OtherId)) { return false; }
	Other.Rewards->RewardGenerated().AddLambda([&Generated](USoulsRewardComponent*, USoulsItemReward*) { ++Generated; });
	TestTrue(TEXT("Destroy living actor"), Other.Actor->Destroy());
	TestEqual(TEXT("Actor teardown is not defeat"), Generated, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsRewardInvalidTest, "SoulsNew.Rewards.InvalidOrLostSourceGrant", SoulsRewardTests::Flags)
bool FSoulsRewardInvalidTest::RunTest(const FString& Parameters)
{
	SoulsRewardTests::FFixture Fixture;
	TStrongObjectPtr<USoulsRewardComponent> Ownerless(NewObject<USoulsRewardComponent>());
	TestTrue(TEXT("Owner required"), Ownerless->ConfigureSourceGrant(FGuid::NewGuid()) == ESoulsRewardSourceResult::InvalidOwner);
	const auto Incompatible = Fixture.MakeOwner(500.0f, true, false);
	if (!TestNotNull(TEXT("Incompatible reward component"), Incompatible.Rewards)) { return false; }
	TestTrue(TEXT("Health required"), Incompatible.Rewards->ConfigureSourceGrant(FGuid::NewGuid()) == ESoulsRewardSourceResult::IncompatibleOwner);
	SoulsRewardTests::FOwner Source;
	FGuid Id;
	if (!SoulsRewardTests::PrepareSource(*this, Fixture, Source, Id)) { return false; }
	TestTrue(TEXT("Invalid selection rejected"), Source.Rewards->ConfigureSourceGrant(FGuid()) == ESoulsRewardSourceResult::InvalidGrant);
	TestEqual(TEXT("Failed configuration preserves previous selection"), Source.Rewards->GetSourceGrantId(), Id);
	TestTrue(TEXT("Withdraw before death"), Source.Items->RemoveItem(Id) == ESoulsItemResult::Success);
	Source.Health->TakeDamage(10000.0f);
	TestNull(TEXT("No invented reward for missing grant"), Source.Rewards->GetReward());
	TestTrue(TEXT("Failure reported"), Source.Rewards->GetLastGenerationResult() == ESoulsRewardSourceResult::InvalidGrant);
	Source.Health->Death().Broadcast(Source.Health);
	TestNull(TEXT("No repeated generation"), Source.Rewards->GetReward());
	TestTrue(TEXT("Dead configuration rejected"), Source.Rewards->ConfigureSourceGrant(Id) == ESoulsRewardSourceResult::OwnerDead);
	TestTrue(TEXT("Reset"), Source.Health->ResetHealthForNewLife());
	FGuid NewId;
	TestTrue(TEXT("Fresh source grant"), Source.Items->GrantItem(Fixture.Definition.Get(), NewId) == ESoulsItemResult::Success);
	TestTrue(TEXT("Configure new selection"), Source.Rewards->ConfigureSourceGrant(NewId) == ESoulsRewardSourceResult::Success);
	TestTrue(TEXT("Explicit rearm after failure"), Source.Rewards->Rearm() == ESoulsRewardSourceResult::Success);
	if (!SoulsRewardTests::Kill(*this, Source)) { return false; }
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsRewardTargetLossTest, "SoulsNew.Rewards.TargetDestroyedDuringGrant", SoulsRewardTests::Flags)
bool FSoulsRewardTargetLossTest::RunTest(const FString& Parameters)
{
	SoulsRewardTests::FFixture Fixture;
	SoulsRewardTests::FOwner Source;
	FGuid SourceId;
	if (!SoulsRewardTests::PrepareSource(*this, Fixture, Source, SourceId)) { return false; }
	USoulsItemReward* Reward = SoulsRewardTests::Kill(*this, Source);
	if (!Reward) { return false; }
	const auto LostTarget = Fixture.MakeOwner();
	const auto EarlyLostTarget = Fixture.MakeOwner();
	const auto Target = Fixture.MakeOwner();
	if (!SoulsRewardTests::Ready(*this, LostTarget) || !SoulsRewardTests::Ready(*this, EarlyLostTarget)
		|| !SoulsRewardTests::Ready(*this, Target)) { return false; }
	int32 Claimed = 0;
	Reward->RewardClaimed().AddLambda([&Claimed](USoulsItemReward*, USoulsItemComponent*, FGuid) { ++Claimed; });
	LostTarget.Items->ItemGranted().AddLambda([](USoulsItemComponent* Items, FGuid, FName) { Items->DestroyComponent(); });
	FGuid Id;
	TestTrue(TEXT("Post-grant destruction detected"), Reward->Claim(LostTarget.Items, Id) == ESoulsRewardClaimResult::GrantLost);
	TestEqual(TEXT("No orphan effect"), LostTarget.Health->GetMaxHealth(), 100.0f);
	TestFalse(TEXT("No successful ID"), Id.IsValid());
	EarlyLostTarget.Health->MaxHealthChanged().AddLambda([&EarlyLostTarget](USoulsHealthComponent*, float, float)
	{
		if (EarlyLostTarget.Items->IsRegistered()) { EarlyLostTarget.Items->DestroyComponent(); }
	});
	TestTrue(TEXT("Early destruction is normal grant failure"), Reward->Claim(EarlyLostTarget.Items, Id) == ESoulsRewardClaimResult::GrantRejected);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("No early orphan effect"), EarlyLostTarget.Health->GetMaxHealth(), 100.0f);
	TestTrue(TEXT("Still available"), Reward->GetState() == ESoulsItemRewardState::Available);
	TestEqual(TEXT("No false claim events"), Claimed, 0);
	TestTrue(TEXT("Safe target retry"), Reward->Claim(Target.Items, Id) == ESoulsRewardClaimResult::Success);
	TestEqual(TEXT("One confirmed claim"), Claimed, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsRewardOutliveTest, "SoulsNew.Rewards.RewardOutlivesSource", SoulsRewardTests::Flags)
bool FSoulsRewardOutliveTest::RunTest(const FString& Parameters)
{
	SoulsRewardTests::FFixture Fixture;
	SoulsRewardTests::FOwner Source;
	FGuid SourceId;
	if (!SoulsRewardTests::PrepareSource(*this, Fixture, Source, SourceId)) { return false; }
	TStrongObjectPtr<USoulsItemReward> Reward(SoulsRewardTests::Kill(*this, Source));
	if (!Reward.IsValid()) { return false; }
	const auto Target = Fixture.MakeOwner();
	if (!SoulsRewardTests::Ready(*this, Target)) { return false; }
	TWeakObjectPtr<AActor> WeakSource(Source.Actor);
	TestTrue(TEXT("Destroy source actor"), Source.Actor->Destroy());
	CollectGarbage(RF_NoFlags);
	TestFalse(TEXT("Source no longer valid"), WeakSource.IsValid());
	TestTrue(TEXT("Reward definition survives"), Reward->GetItemDefinition() == Fixture.Definition.Get());
	FGuid TargetId;
	TestTrue(TEXT("Claim after source lifetime"), Reward->Claim(Target.Items, TargetId) == ESoulsRewardClaimResult::Success);
	TestEqual(TEXT("Target effect"), Target.Health->GetMaxHealth(), 125.0f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
