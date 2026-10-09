#include "Items/SoulsItemComponent.h"
#include "Items/SoulsItemDefinition.h"
#include "Attributes/SoulsHealthComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Async/TaskGraphInterfaces.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Serialization/ObjectWriter.h"
#include "UObject/GarbageCollection.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

namespace SoulsItemTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext
		| EAutomationTestFlags::CommandletContext | EAutomationTestFlags::EngineFilter;

	FSoulsItemEffect Effect(float Magnitude = 0.25f,
		ESoulsAttributeModifierOperation Operation = ESoulsAttributeModifierOperation::AdditivePercent)
	{
		FSoulsItemEffect Value;
		Value.Attribute = ESoulsItemAttribute::MaxHealth;
		Value.Operation = Operation;
		Value.Magnitude = Magnitude;
		return Value;
	}

	FSoulsItemDefinitionData VitalityData()
	{
		FSoulsItemDefinitionData Data;
		Data.ItemId = TEXT("Item.Vitality");
		Data.DisplayName = FText::FromString(TEXT("Vitalidad"));
		Data.Description = FText::FromString(TEXT("+25% MaxHealth"));
		Data.Effects.Add(Effect());
		return Data;
	}

	struct FOwner
	{
		AActor* Actor = nullptr;
		USoulsHealthComponent* Health = nullptr;
		USoulsItemComponent* Items = nullptr;
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
		}

		~FFixture() { if (World) { World->DestroyWorld(false); } }

		FOwner MakeOwner(float Base = 100.0f, bool bWithHealth = true)
		{
			FOwner Owner;
			Owner.Actor = World ? World->SpawnActor<AActor>() : nullptr;
			if (!Owner.Actor) { return Owner; }
			if (bWithHealth)
			{
				Owner.Health = NewObject<USoulsHealthComponent>(Owner.Actor);
				Owner.Actor->AddInstanceComponent(Owner.Health);
				Owner.Health->SetBaseMaxHealth(Base);
				Owner.Health->RegisterComponent();
			}
			Owner.Items = NewObject<USoulsItemComponent>(Owner.Actor);
			Owner.Actor->AddInstanceComponent(Owner.Items);
			Owner.Items->RegisterComponent();
			return Owner;
		}
	};

	bool Ready(FAutomationTestBase& Test, const FOwner& Owner, bool bWithHealth = true)
	{
		return Test.TestNotNull(TEXT("Actor"), Owner.Actor) && Test.TestNotNull(TEXT("Items"), Owner.Items)
			&& (!bWithHealth || Test.TestNotNull(TEXT("Health"), Owner.Health));
	}

	bool Granted(FAutomationTestBase& Test, const FOwner& Owner, USoulsItemDefinition* Definition, FGuid& Id)
	{
		const auto Result = Owner.Items->GrantItem(Definition, Id);
		return Test.TestTrue(TEXT("Grant succeeds"), Result == ESoulsItemResult::Success)
			&& Test.TestTrue(TEXT("Stable valid grant ID"), Id.IsValid());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsItemATest, "SoulsNew.Items.A.Owner100", SoulsItemTests::Flags)
bool FSoulsItemATest::RunTest(const FString& Parameters)
{
	SoulsItemTests::FFixture Fixture;
	const auto A = Fixture.MakeOwner();
	if (!SoulsItemTests::Ready(*this, A)) { return false; }
	FGuid Id;
	if (!SoulsItemTests::Granted(*this, A, Fixture.Definition.Get(), Id)) { return false; }
	TestEqual(TEXT("100 -> 125"), A.Health->GetMaxHealth(), 125.0f);
	TestEqual(TEXT("Full current"), A.Health->GetCurrentHealth(), 125.0f);
	TestEqual(TEXT("Base unchanged"), A.Health->GetBaseMaxHealth(), 100.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsItemBTest, "SoulsNew.Items.B.SameDefinition500", SoulsItemTests::Flags)
bool FSoulsItemBTest::RunTest(const FString& Parameters)
{
	SoulsItemTests::FFixture Fixture;
	const auto A = Fixture.MakeOwner();
	const auto B = Fixture.MakeOwner(500.0f);
	if (!SoulsItemTests::Ready(*this, A) || !SoulsItemTests::Ready(*this, B)) { return false; }
	FGuid AId, BId;
	if (!SoulsItemTests::Granted(*this, A, Fixture.Definition.Get(), AId)
		|| !SoulsItemTests::Granted(*this, B, Fixture.Definition.Get(), BId)) { return false; }
	TestEqual(TEXT("Same UObject definition, A"), A.Health->GetMaxHealth(), 125.0f);
	TestEqual(TEXT("Same UObject definition, B"), B.Health->GetMaxHealth(), 625.0f);
	TestEqual(TEXT("B base unchanged"), B.Health->GetBaseMaxHealth(), 500.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsItemCTest, "SoulsNew.Items.C.RemoveFrom100", SoulsItemTests::Flags)
bool FSoulsItemCTest::RunTest(const FString& Parameters)
{
	SoulsItemTests::FFixture Fixture;
	const auto A = Fixture.MakeOwner();
	if (!SoulsItemTests::Ready(*this, A)) { return false; }
	FGuid Id;
	if (!SoulsItemTests::Granted(*this, A, Fixture.Definition.Get(), Id)) { return false; }
	A.Health->TakeDamage(62.5f);
	TestTrue(TEXT("Remove succeeds"), A.Items->RemoveItem(Id) == ESoulsItemResult::Success);
	TestEqual(TEXT("125 -> 100"), A.Health->GetMaxHealth(), 100.0f);
	TestEqual(TEXT("PreservePercentage also on removal"), A.Health->GetCurrentHealth(), 50.0f);
	TestFalse(TEXT("No remaining grant"), A.Items->HasGrant(Id));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsItemDTest, "SoulsNew.Items.D.RemoveFrom500", SoulsItemTests::Flags)
bool FSoulsItemDTest::RunTest(const FString& Parameters)
{
	SoulsItemTests::FFixture Fixture;
	const auto B = Fixture.MakeOwner(500.0f);
	if (!SoulsItemTests::Ready(*this, B)) { return false; }
	FGuid Id;
	if (!SoulsItemTests::Granted(*this, B, Fixture.Definition.Get(), Id)) { return false; }
	TestTrue(TEXT("Remove succeeds"), B.Items->RemoveItem(Id) == ESoulsItemResult::Success);
	TestEqual(TEXT("625 -> 500"), B.Health->GetMaxHealth(), 500.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsItemETest, "SoulsNew.Items.E.IndependentRuntime", SoulsItemTests::Flags)
bool FSoulsItemETest::RunTest(const FString& Parameters)
{
	SoulsItemTests::FFixture Fixture;
	const auto A = Fixture.MakeOwner();
	const auto B = Fixture.MakeOwner(500.0f);
	if (!SoulsItemTests::Ready(*this, A) || !SoulsItemTests::Ready(*this, B)) { return false; }
	FGuid AId, BId;
	if (!SoulsItemTests::Granted(*this, A, Fixture.Definition.Get(), AId)
		|| !SoulsItemTests::Granted(*this, B, Fixture.Definition.Get(), BId)) { return false; }
	FSoulsItemGrantSnapshot ASnapshot, BSnapshot;
	TestTrue(TEXT("Find A"), A.Items->FindGrant(AId, ASnapshot));
	TestTrue(TEXT("Find B"), B.Items->FindGrant(BId, BSnapshot));
	TestTrue(TEXT("Grant IDs independent"), AId != BId);
	TestEqual(TEXT("A one modifier"), ASnapshot.ModifierIds.Num(), 1);
	TestEqual(TEXT("B one modifier"), BSnapshot.ModifierIds.Num(), 1);
	if (ASnapshot.ModifierIds.Num() == 1 && BSnapshot.ModifierIds.Num() == 1)
	{
		TestTrue(TEXT("Modifier IDs independent"), ASnapshot.ModifierIds[0] != BSnapshot.ModifierIds[0]);
		TestTrue(TEXT("Grant ID is not modifier ID"), AId != ASnapshot.ModifierIds[0]);
	}
	TestEqual(TEXT("Definition ID is logical"), ASnapshot.ItemId, FName(TEXT("Item.Vitality")));
	A.Health->TakeDamage(25.0f);
	TestEqual(TEXT("B current independent"), B.Health->GetCurrentHealth(), 625.0f);
	TestTrue(TEXT("Changing A base supported"), A.Health->SetBaseMaxHealth(200.0f));
	TestEqual(TEXT("A recalculates"), A.Health->GetMaxHealth(), 250.0f);
	TestEqual(TEXT("B unchanged"), B.Health->GetMaxHealth(), 625.0f);
	TestTrue(TEXT("Other owner's handle rejected"), A.Items->RemoveItem(BId) == ESoulsItemResult::InvalidGrant);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsItemFTest, "SoulsNew.Items.F.RemovalIsolation", SoulsItemTests::Flags)
bool FSoulsItemFTest::RunTest(const FString& Parameters)
{
	SoulsItemTests::FFixture Fixture;
	const auto A = Fixture.MakeOwner();
	const auto B = Fixture.MakeOwner(500.0f);
	if (!SoulsItemTests::Ready(*this, A) || !SoulsItemTests::Ready(*this, B)) { return false; }
	FGuid AId, BId;
	if (!SoulsItemTests::Granted(*this, A, Fixture.Definition.Get(), AId)
		|| !SoulsItemTests::Granted(*this, B, Fixture.Definition.Get(), BId)) { return false; }
	TestTrue(TEXT("Remove A"), A.Items->RemoveItem(AId) == ESoulsItemResult::Success);
	TestEqual(TEXT("A back to 100"), A.Health->GetMaxHealth(), 100.0f);
	TestEqual(TEXT("B still 625"), B.Health->GetMaxHealth(), 625.0f);
	TestTrue(TEXT("B still possesses item"), B.Items->HasGrant(BId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsItemGTest, "SoulsNew.Items.G.RepeatedGrantRemove", SoulsItemTests::Flags)
bool FSoulsItemGTest::RunTest(const FString& Parameters)
{
	SoulsItemTests::FFixture Fixture;
	const auto A = Fixture.MakeOwner();
	if (!SoulsItemTests::Ready(*this, A)) { return false; }
	A.Health->TakeDamage(50.0f);
	const FSoulsAttributeModifier Foreign = { FGuid::NewGuid(), ESoulsAttributeModifierOperation::Flat, 10.0f };
	TestTrue(TEXT("Unrelated contribution"), A.Health->AddMaxHealthModifier(Foreign));
	for (int32 Index = 0; Index < 30; ++Index)
	{
		FGuid Id;
		if (!SoulsItemTests::Granted(*this, A, Fixture.Definition.Get(), Id)) { return false; }
		FSoulsItemGrantSnapshot Snapshot;
		TestTrue(TEXT("Snapshot"), A.Items->FindGrant(Id, Snapshot));
		TestTrue(TEXT("Remove"), A.Items->RemoveItem(Id) == ESoulsItemResult::Success);
		FSoulsAttributeModifier Found;
		for (const FGuid& ModifierId : Snapshot.ModifierIds)
		{
			TestFalse(TEXT("No orphan modifier"), A.Health->FindMaxHealthModifier(ModifierId, Found));
		}
		TestTrue(TEXT("Unrelated modifier preserved"), A.Health->FindMaxHealthModifier(Foreign.Id, Found));
		TestEqual(TEXT("No possessions"), A.Items->GetGrantCount(), 0);
		TestEqual(TEXT("No max drift"), A.Health->GetMaxHealth(), 110.0f);
		TestEqual(TEXT("No current drift"), A.Health->GetCurrentHealth(), 55.0f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsItemHTest, "SoulsNew.Items.H.IndependentDuplicateGrants", SoulsItemTests::Flags)
bool FSoulsItemHTest::RunTest(const FString& Parameters)
{
	SoulsItemTests::FFixture Fixture;
	const auto A = Fixture.MakeOwner();
	if (!SoulsItemTests::Ready(*this, A)) { return false; }
	FGuid First, Second;
	if (!SoulsItemTests::Granted(*this, A, Fixture.Definition.Get(), First)
		|| !SoulsItemTests::Granted(*this, A, Fixture.Definition.Get(), Second)) { return false; }
	TestTrue(TEXT("Different grant IDs"), First != Second);
	TestEqual(TEXT("Two additive contributions"), A.Health->GetMaxHealth(), 150.0f);
	TestTrue(TEXT("Remove first only"), A.Items->RemoveItem(First) == ESoulsItemResult::Success);
	TestTrue(TEXT("Second remains"), A.Items->HasGrant(Second));
	TestTrue(TEXT("Identity lookup remains"), A.Items->HasItem(Fixture.Definition->GetItemId()));
	TestEqual(TEXT("Second effect remains"), A.Health->GetMaxHealth(), 125.0f);
	TestTrue(TEXT("Repeated remove rejected"), A.Items->RemoveItem(First) == ESoulsItemResult::InvalidGrant);
	TestTrue(TEXT("Remove second"), A.Items->RemoveItem(Second) == ESoulsItemResult::Success);
	TestEqual(TEXT("Base restored"), A.Health->GetMaxHealth(), 100.0f);
	TestFalse(TEXT("No definition possessed"), A.Items->HasItem(Fixture.Definition->GetItemId()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsItemITest, "SoulsNew.Items.I.FailedGrantIsAtomic", SoulsItemTests::Flags)
bool FSoulsItemITest::RunTest(const FString& Parameters)
{
	SoulsItemTests::FFixture Fixture;
	const auto A = Fixture.MakeOwner();
	if (!SoulsItemTests::Ready(*this, A)) { return false; }
	A.Health->TakeDamage(50.0f);
	int32 Events = 0;
	A.Health->HealthChanged().AddLambda([&Events](USoulsHealthComponent*, float, float) { ++Events; });
	A.Health->MaxHealthChanged().AddLambda([&Events](USoulsHealthComponent*, float, float) { ++Events; });
	A.Items->ItemGranted().AddLambda([&Events](USoulsItemComponent*, FGuid, FName) { ++Events; });
	for (float Invalid : { std::numeric_limits<float>::quiet_NaN(), TNumericLimits<float>::Max() })
	{
		auto Data = SoulsItemTests::VitalityData();
		Data.Effects.Add(SoulsItemTests::Effect(Invalid));
		TStrongObjectPtr<USoulsItemDefinition> Definition(USoulsItemDefinition::CreateTransient(Data));
		FGuid Id = FGuid::NewGuid();
		TestTrue(TEXT("Later effect rejected"), A.Items->GrantItem(Definition.Get(), Id) == ESoulsItemResult::HealthRejected);
		TestFalse(TEXT("Failure clears output"), Id.IsValid());
		TestEqual(TEXT("No provisional grant"), A.Items->GetGrantCount(), 0);
		TestEqual(TEXT("Max unchanged exactly"), A.Health->GetMaxHealth(), 100.0f);
		TestEqual(TEXT("Current unchanged exactly"), A.Health->GetCurrentHealth(), 50.0f);
		TestEqual(TEXT("No partial/failure events"), Events, 0);
	}
	FGuid ValidId;
	if (!SoulsItemTests::Granted(*this, A, Fixture.Definition.Get(), ValidId)) { return false; }
	TestEqual(TEXT("Valid grant unaffected by failures"), A.Health->GetMaxHealth(), 125.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsItemJTest, "SoulsNew.Items.J.DefinitionUnchanged", SoulsItemTests::Flags)
bool FSoulsItemJTest::RunTest(const FString& Parameters)
{
	SoulsItemTests::FFixture Fixture;
	const auto A = Fixture.MakeOwner();
	const auto B = Fixture.MakeOwner(500.0f);
	if (!SoulsItemTests::Ready(*this, A) || !SoulsItemTests::Ready(*this, B)) { return false; }
	TArray<uint8> Before, After;
	FObjectWriter BeforeWriter(Fixture.Definition.Get(), Before);
	FGuid AId, BId;
	if (!SoulsItemTests::Granted(*this, A, Fixture.Definition.Get(), AId)
		|| !SoulsItemTests::Granted(*this, B, Fixture.Definition.Get(), BId)) { return false; }
	A.Health->TakeDamage(30.0f);
	TestTrue(TEXT("Remove A"), A.Items->RemoveItem(AId) == ESoulsItemResult::Success);
	FObjectWriter AfterWriter(Fixture.Definition.Get(), After);
	TestTrue(TEXT("Serialized definition unchanged byte for byte"), Before == After);
	TestEqual(TEXT("Primary ID uses logical identity"), Fixture.Definition->GetPrimaryAssetId().PrimaryAssetName, FName(TEXT("Item.Vitality")));
	auto Source = SoulsItemTests::VitalityData();
	TStrongObjectPtr<USoulsItemDefinition> Other(USoulsItemDefinition::CreateTransient(Source));
	Source.Effects[0].Magnitude = 9.0f;
	TestEqual(TEXT("Factory copies input data"), Other->GetData().Effects[0].Magnitude, 0.25f);
	TestTrue(TEXT("Two objects, same definition identity"), Other->GetPrimaryAssetId() == Fixture.Definition->GetPrimaryAssetId());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsItemKTest, "SoulsNew.Items.K.EventsAndReentrancy", SoulsItemTests::Flags)
bool FSoulsItemKTest::RunTest(const FString& Parameters)
{
	SoulsItemTests::FFixture Fixture;
	const auto A = Fixture.MakeOwner();
	if (!SoulsItemTests::Ready(*this, A)) { return false; }
	int32 Grants = 0, Removes = 0;
	TArray<FString> Order;
	A.Health->MaxHealthChanged().AddLambda([this, &Order, &A](USoulsHealthComponent*, float, float New)
	{
		Order.Add(TEXT("Health"));
		TestEqual(TEXT("Possession matches committed health"), A.Items->GetGrantCount(), New == 125.0f ? 1 : 0);
	});
	A.Items->ItemGranted().AddLambda([this, &Grants, &Order, &Fixture](USoulsItemComponent* Items, FGuid Id, FName ItemId)
	{
		++Grants;
		Order.Add(TEXT("Grant"));
		TestTrue(TEXT("Committed grant visible"), Items->HasGrant(Id));
		TestEqual(TEXT("Definition event identity"), ItemId, Fixture.Definition->GetItemId());
		TestTrue(TEXT("Nested remove rejected"), Items->RemoveItem(Id) == ESoulsItemResult::Busy);
		FGuid Nested;
		TestTrue(TEXT("Nested grant rejected"), Items->GrantItem(Fixture.Definition.Get(), Nested) == ESoulsItemResult::Busy);
	});
	A.Items->ItemRemoved().AddLambda([this, &Removes, &Order](USoulsItemComponent* Items, FGuid Id, FName)
	{
		++Removes;
		Order.Add(TEXT("Remove"));
		TestFalse(TEXT("Removed grant absent"), Items->HasGrant(Id));
		TestTrue(TEXT("Nested removal rejected"), Items->RemoveItem(Id) == ESoulsItemResult::Busy);
	});
	FGuid Id;
	if (!SoulsItemTests::Granted(*this, A, Fixture.Definition.Get(), Id)) { return false; }
	TestTrue(TEXT("Remove"), A.Items->RemoveItem(Id) == ESoulsItemResult::Success);
	TestTrue(TEXT("Duplicate removal fails"), A.Items->RemoveItem(Id) == ESoulsItemResult::InvalidGrant);
	FGuid Failed;
	TestTrue(TEXT("Null definition fails"), A.Items->GrantItem(nullptr, Failed) == ESoulsItemResult::InvalidDefinition);
	TestEqual(TEXT("Exactly one grant event"), Grants, 1);
	TestEqual(TEXT("Exactly one remove event"), Removes, 1);
	TestEqual(TEXT("Four notifications"), Order.Num(), 4);
	if (Order.Num() == 4)
	{
		TestEqual(TEXT("Health first on grant"), Order[0], FString(TEXT("Health")));
		TestEqual(TEXT("Grant after health"), Order[1], FString(TEXT("Grant")));
		TestEqual(TEXT("Health first on remove"), Order[2], FString(TEXT("Health")));
		TestEqual(TEXT("Remove after health"), Order[3], FString(TEXT("Remove")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsItemLTest, "SoulsNew.Items.L.GenericActor", SoulsItemTests::Flags)
bool FSoulsItemLTest::RunTest(const FString& Parameters)
{
	SoulsItemTests::FFixture Fixture;
	const auto Owner = Fixture.MakeOwner(500.0f);
	if (!SoulsItemTests::Ready(*this, Owner)) { return false; }
	TestTrue(TEXT("Exactly AActor, not a Character"), Owner.Actor->GetClass() == AActor::StaticClass());
	TestTrue(TEXT("Item component belongs to same Actor"), Owner.Items->GetOwner() == Owner.Health->GetOwner());
	TestFalse(TEXT("No Tick"), Owner.Items->PrimaryComponentTick.bCanEverTick);
	FGuid Id;
	if (!SoulsItemTests::Granted(*this, Owner, Fixture.Definition.Get(), Id)) { return false; }
	TestEqual(TEXT("Works without controller/UI"), Owner.Health->GetMaxHealth(), 625.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsItemMultiTest, "SoulsNew.Items.MultiEffectSingleCommit", SoulsItemTests::Flags)
bool FSoulsItemMultiTest::RunTest(const FString& Parameters)
{
	SoulsItemTests::FFixture Fixture;
	const auto A = Fixture.MakeOwner();
	if (!SoulsItemTests::Ready(*this, A)) { return false; }
	auto Data = SoulsItemTests::VitalityData();
	Data.Effects.Add(SoulsItemTests::Effect(-100.0f, ESoulsAttributeModifierOperation::Flat));
	Data.Effects.Add(SoulsItemTests::Effect(100.0f, ESoulsAttributeModifierOperation::Flat));
	TStrongObjectPtr<USoulsItemDefinition> Definition(USoulsItemDefinition::CreateTransient(Data));
	int32 MaxEvents = 0, Deaths = 0;
	A.Health->MaxHealthChanged().AddLambda([&MaxEvents](USoulsHealthComponent*, float, float) { ++MaxEvents; });
	A.Health->Death().AddLambda([&Deaths](USoulsHealthComponent*) { ++Deaths; });
	FGuid Id;
	if (!SoulsItemTests::Granted(*this, A, Definition.Get(), Id)) { return false; }
	TestEqual(TEXT("Full aggregate"), A.Health->GetMaxHealth(), 125.0f);
	TestEqual(TEXT("One grant recalculation event"), MaxEvents, 1);
	TestTrue(TEXT("All three withdrawn together"), A.Items->RemoveItem(Id) == ESoulsItemResult::Success);
	TestEqual(TEXT("Base restored"), A.Health->GetMaxHealth(), 100.0f);
	TestEqual(TEXT("One removal recalculation event"), MaxEvents, 2);
	TestEqual(TEXT("No intermediate death"), Deaths, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsItemRemovalFailureTest, "SoulsNew.Items.FailedRemovalIsAtomic", SoulsItemTests::Flags)
bool FSoulsItemRemovalFailureTest::RunTest(const FString& Parameters)
{
	SoulsItemTests::FFixture Fixture;
	const auto A = Fixture.MakeOwner();
	if (!SoulsItemTests::Ready(*this, A)) { return false; }
	auto Data = SoulsItemTests::VitalityData();
	Data.Effects = { SoulsItemTests::Effect(-99.0f, ESoulsAttributeModifierOperation::Flat),
		SoulsItemTests::Effect(0.001f, ESoulsAttributeModifierOperation::Multiplicative) };
	TStrongObjectPtr<USoulsItemDefinition> Definition(USoulsItemDefinition::CreateTransient(Data));
	FGuid Id;
	if (!SoulsItemTests::Granted(*this, A, Definition.Get(), Id)) { return false; }
	const FSoulsAttributeModifier Foreign = { FGuid::NewGuid(), ESoulsAttributeModifierOperation::Multiplicative, TNumericLimits<float>::Max() };
	TestTrue(TEXT("Compensated external multiplier fits"), A.Health->AddMaxHealthModifier(Foreign));
	const float Before = A.Health->GetMaxHealth();
	FSoulsItemGrantSnapshot Snapshot;
	TestTrue(TEXT("Snapshot"), A.Items->FindGrant(Id, Snapshot));
	int32 Events = 0;
	A.Health->MaxHealthChanged().AddLambda([&Events](USoulsHealthComponent*, float, float) { ++Events; });
	A.Items->ItemRemoved().AddLambda([&Events](USoulsItemComponent*, FGuid, FName) { ++Events; });
	TestTrue(TEXT("Overflowing removal fails"), A.Items->RemoveItem(Id) == ESoulsItemResult::HealthRejected);
	TestEqual(TEXT("Health unchanged"), A.Health->GetMaxHealth(), Before);
	TestTrue(TEXT("Possession restored"), A.Items->HasGrant(Id));
	TestEqual(TEXT("No partial removal events"), Events, 0);
	FSoulsAttributeModifier Found;
	for (const FGuid& ModifierId : Snapshot.ModifierIds) { TestTrue(TEXT("All original IDs retained"), A.Health->FindMaxHealthModifier(ModifierId, Found)); }
	TestTrue(TEXT("Remove problematic foreign multiplier"), A.Health->RemoveMaxHealthModifier(Foreign.Id));
	TestTrue(TEXT("Retry succeeds"), A.Items->RemoveItem(Id) == ESoulsItemResult::Success);
	TestEqual(TEXT("Restored base"), A.Health->GetMaxHealth(), 100.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsItemCompatibilityTest, "SoulsNew.Items.InvalidDefinitionAndOwner", SoulsItemTests::Flags)
bool FSoulsItemCompatibilityTest::RunTest(const FString& Parameters)
{
	SoulsItemTests::FFixture Fixture;
	const auto A = Fixture.MakeOwner();
	const auto NoHealth = Fixture.MakeOwner(100.0f, false);
	if (!SoulsItemTests::Ready(*this, A) || !SoulsItemTests::Ready(*this, NoHealth, false)) { return false; }
	FGuid Id;
	TestTrue(TEXT("Missing Health rejected"), NoHealth.Items->GrantItem(Fixture.Definition.Get(), Id) == ESoulsItemResult::MissingHealth);
	TStrongObjectPtr<USoulsItemComponent> Ownerless(NewObject<USoulsItemComponent>());
	TestTrue(TEXT("No Actor rejected"), Ownerless->GrantItem(Fixture.Definition.Get(), Id) == ESoulsItemResult::InvalidOwner);
	for (int32 Case = 0; Case < 5; ++Case)
	{
		auto Data = SoulsItemTests::VitalityData();
		if (Case == 0) { Data.ItemId = NAME_None; }
		if (Case == 1) { Data.DisplayName = FText::GetEmpty(); }
		if (Case == 2) { Data.Effects.Reset(); }
		if (Case == 3) { Data.Effects[0].Attribute = static_cast<ESoulsItemAttribute>(255); }
		if (Case == 4) { Data.Effects[0].Operation = static_cast<ESoulsAttributeModifierOperation>(255); }
		TStrongObjectPtr<USoulsItemDefinition> Invalid(USoulsItemDefinition::CreateTransient(Data));
		const ESoulsItemResult Expected = Case == 3 ? ESoulsItemResult::UnsupportedEffect
			: Case == 4 ? ESoulsItemResult::HealthRejected : ESoulsItemResult::InvalidDefinition;
		TestTrue(TEXT("Invalid schema rejected explicitly"), A.Items->GrantItem(Invalid.Get(), Id) == Expected);
		TestFalse(TEXT("No failure handle"), Id.IsValid());
	}
	USoulsHealthComponent* Extra = NewObject<USoulsHealthComponent>(A.Actor);
	A.Actor->AddInstanceComponent(Extra);
	Extra->RegisterComponent();
	TestTrue(TEXT("Ambiguous Health rejected"), A.Items->GrantItem(Fixture.Definition.Get(), Id) == ESoulsItemResult::AmbiguousHealth);
	TestEqual(TEXT("No possessions"), A.Items->GetGrantCount(), 0);
	TestEqual(TEXT("Original health untouched"), A.Health->GetMaxHealth(), 100.0f);
	TestEqual(TEXT("Other health untouched"), Extra->GetMaxHealth(), 100.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsItemHealthCallbackTest, "SoulsNew.Items.HealthCallbackRejectsMutation", SoulsItemTests::Flags)
bool FSoulsItemHealthCallbackTest::RunTest(const FString& Parameters)
{
	SoulsItemTests::FFixture Fixture;
	const auto A = Fixture.MakeOwner();
	if (!SoulsItemTests::Ready(*this, A)) { return false; }
	A.Health->MaxHealthChanged().AddLambda([this, &A, &Fixture](USoulsHealthComponent*, float, float)
	{
		FGuid Id;
		TestTrue(TEXT("Health callback cannot grant"), A.Items->GrantItem(Fixture.Definition.Get(), Id) == ESoulsItemResult::HealthRejected);
		TestFalse(TEXT("No output"), Id.IsValid());
	});
	TestTrue(TEXT("Base change"), A.Health->SetBaseMaxHealth(200.0f));
	TestEqual(TEXT("No partial possession"), A.Items->GetGrantCount(), 0);
	A.Health->MaxHealthChanged().Clear();
	FGuid Id;
	if (!SoulsItemTests::Granted(*this, A, Fixture.Definition.Get(), Id)) { return false; }
	A.Health->MaxHealthChanged().AddLambda([this, &A, &Id](USoulsHealthComponent*, float, float)
	{
		TestTrue(TEXT("Health callback cannot remove"), A.Items->RemoveItem(Id) == ESoulsItemResult::HealthRejected);
		TestTrue(TEXT("Possession restored after rejected remove"), A.Items->HasGrant(Id));
	});
	TestTrue(TEXT("Another base change"), A.Health->SetBaseMaxHealth(100.0f));
	TestEqual(TEXT("Modifier still present"), A.Health->GetMaxHealth(), 125.0f);
	A.Health->MaxHealthChanged().Clear();
	TestTrue(TEXT("Normal removal still works"), A.Items->RemoveItem(Id) == ESoulsItemResult::Success);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsItemGCTest, "SoulsNew.Items.DefinitionGCLifetime", SoulsItemTests::Flags)
bool FSoulsItemGCTest::RunTest(const FString& Parameters)
{
	SoulsItemTests::FFixture Fixture;
	const auto A = Fixture.MakeOwner();
	if (!SoulsItemTests::Ready(*this, A)) { return false; }
	USoulsItemDefinition* Definition = USoulsItemDefinition::CreateTransient(SoulsItemTests::VitalityData());
	TWeakObjectPtr<USoulsItemDefinition> WeakDefinition(Definition);
	FGuid Id;
	if (!SoulsItemTests::Granted(*this, A, Definition, Id)) { return false; }
	Definition = nullptr;
	CollectGarbage(RF_NoFlags);
	TestTrue(TEXT("Possession retains definition via reflected reference"), WeakDefinition.IsValid());
	TestTrue(TEXT("Removal"), A.Items->RemoveItem(Id) == ESoulsItemResult::Success);
	CollectGarbage(RF_NoFlags);
	TestFalse(TEXT("Removed grant does not retain definition"), WeakDefinition.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsItemTeardownTest, "SoulsNew.Items.ComponentTeardown", SoulsItemTests::Flags)
bool FSoulsItemTeardownTest::RunTest(const FString& Parameters)
{
	SoulsItemTests::FFixture Fixture;
	const auto A = Fixture.MakeOwner();
	if (!SoulsItemTests::Ready(*this, A)) { return false; }
	FGuid First, Second;
	if (!SoulsItemTests::Granted(*this, A, Fixture.Definition.Get(), First)
		|| !SoulsItemTests::Granted(*this, A, Fixture.Definition.Get(), Second)) { return false; }
	FSoulsItemGrantSnapshot Snapshot;
	TestTrue(TEXT("Find"), A.Items->FindGrant(First, Snapshot));
	int32 RemovedEvents = 0;
	A.Items->ItemRemoved().AddLambda([&RemovedEvents](USoulsItemComponent*, FGuid, FName) { ++RemovedEvents; });
	A.Items->DestroyComponent();
	TestEqual(TEXT("Destruction removes effects from surviving Health"), A.Health->GetMaxHealth(), 100.0f);
	FSoulsAttributeModifier Found;
	for (const FGuid& ModifierId : Snapshot.ModifierIds) { TestFalse(TEXT("No orphan"), A.Health->FindMaxHealthModifier(ModifierId, Found)); }
	TestEqual(TEXT("Teardown is not gameplay removal"), RemovedEvents, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsItemDeferredTeardownTest, "SoulsNew.Items.TeardownDuringHealthCallback", SoulsItemTests::Flags)
bool FSoulsItemDeferredTeardownTest::RunTest(const FString& Parameters)
{
	SoulsItemTests::FFixture Fixture;
	const auto A = Fixture.MakeOwner();
	if (!SoulsItemTests::Ready(*this, A)) { return false; }
	bool bDestroyed = false;
	int32 Grants = 0;
	A.Items->ItemGranted().AddLambda([&Grants](USoulsItemComponent*, FGuid, FName) { ++Grants; });
	A.Health->MaxHealthChanged().AddLambda([&bDestroyed, &A](USoulsHealthComponent*, float, float)
	{
		if (!bDestroyed) { bDestroyed = true; A.Items->DestroyComponent(); }
	});
	FGuid Id;
	TestTrue(TEXT("Teardown cancels grant"), A.Items->GrantItem(Fixture.Definition.Get(), Id) == ESoulsItemResult::Unavailable);
	TestFalse(TEXT("No successful handle"), Id.IsValid());
	TestEqual(TEXT("Cancelled grant rolled back before returning"), A.Health->GetMaxHealth(), 100.0f);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("Deferred cleanup restores surviving Health"), A.Health->GetMaxHealth(), 100.0f);
	TestEqual(TEXT("No successful grant event"), Grants, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsItemBatchTest, "SoulsNew.Items.HealthBatchAtomicity", SoulsItemTests::Flags)
bool FSoulsItemBatchTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<USoulsHealthComponent> Health(NewObject<USoulsHealthComponent>());
	const FSoulsAttributeModifier First = { FGuid::NewGuid(), ESoulsAttributeModifierOperation::AdditivePercent, 0.25f };
	TestTrue(TEXT("Original contribution"), Health->AddMaxHealthModifier(First));
	int32 Events = 0;
	Health->MaxHealthChanged().AddLambda([&Events](USoulsHealthComponent*, float, float) { ++Events; });
	const FSoulsAttributeModifier New = { FGuid::NewGuid(), ESoulsAttributeModifierOperation::Flat, 10.0f };
	const TArray<FSoulsAttributeModifier> DuplicateAdds = { New, New };
	TestFalse(TEXT("Duplicate in batch rejected"), Health->ApplyMaxHealthModifierChanges(DuplicateAdds, {}));
	FSoulsAttributeModifier Found;
	TestFalse(TEXT("Earlier candidate not committed"), Health->FindMaxHealthModifier(New.Id, Found));
	const TArray<FGuid> InvalidRemovals = { First.Id, FGuid::NewGuid() };
	TestFalse(TEXT("Missing later removal rejected"), Health->ApplyMaxHealthModifierChanges({}, InvalidRemovals));
	TestTrue(TEXT("Original retained"), Health->FindMaxHealthModifier(First.Id, Found));
	TestEqual(TEXT("No failure events"), Events, 0);
	TestEqual(TEXT("Max untouched"), Health->GetMaxHealth(), 125.0f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
