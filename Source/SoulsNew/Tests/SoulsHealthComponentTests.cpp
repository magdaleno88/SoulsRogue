#include "Attributes/SoulsHealthComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include <limits>

namespace SoulsHealthTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext
		| EAutomationTestFlags::CommandletContext | EAutomationTestFlags::EngineFilter;

	TStrongObjectPtr<USoulsHealthComponent> MakeHealth(float Base = 100.0f)
	{
		TStrongObjectPtr<USoulsHealthComponent> Health(NewObject<USoulsHealthComponent>());
		Health->SetBaseMaxHealth(Base);
		return Health;
	}

	FSoulsAttributeModifier MakeModifier(uint32 Index = 1, float Magnitude = 0.25f,
		ESoulsAttributeModifierOperation Operation = ESoulsAttributeModifierOperation::AdditivePercent)
	{
		return { FGuid(Index, 0, 0, 1), Operation, Magnitude };
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsHealthBaseTest, "SoulsNew.Health.A.Base100", SoulsHealthTests::Flags)
bool FSoulsHealthBaseTest::RunTest(const FString& Parameters)
{
	auto Health = SoulsHealthTests::MakeHealth();
	TestEqual(TEXT("Base"), Health->GetBaseMaxHealth(), 100.0f);
	TestEqual(TEXT("Max"), Health->GetMaxHealth(), 100.0f);
	TestEqual(TEXT("Current"), Health->GetCurrentHealth(), 100.0f);
	TestEqual(TEXT("Full percentage"), Health->GetHealthPercentage(), 1.0f);
	TestFalse(TEXT("Starts alive"), Health->IsDead());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsHealthPercentTest, "SoulsNew.Health.B.Add25Percent", SoulsHealthTests::Flags)
bool FSoulsHealthPercentTest::RunTest(const FString& Parameters)
{
	auto Health = SoulsHealthTests::MakeHealth();
	TestTrue(TEXT("Add contribution"), Health->AddMaxHealthModifier(SoulsHealthTests::MakeModifier()));
	TestEqual(TEXT("Base remains unchanged"), Health->GetBaseMaxHealth(), 100.0f);
	TestEqual(TEXT("Max is 125"), Health->GetMaxHealth(), 125.0f);
	TestEqual(TEXT("Full health preserved"), Health->GetCurrentHealth(), 125.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsHealthRemoveTest, "SoulsNew.Health.C.Remove25Percent", SoulsHealthTests::Flags)
bool FSoulsHealthRemoveTest::RunTest(const FString& Parameters)
{
	auto Health = SoulsHealthTests::MakeHealth();
	const auto Modifier = SoulsHealthTests::MakeModifier();
	TestTrue(TEXT("Add"), Health->AddMaxHealthModifier(Modifier));
	TestTrue(TEXT("Remove by stable ID"), Health->RemoveMaxHealthModifier(Modifier.Id));
	TestEqual(TEXT("Max returns to base"), Health->GetMaxHealth(), 100.0f);
	TestEqual(TEXT("Full current follows max"), Health->GetCurrentHealth(), 100.0f);
	TestFalse(TEXT("Repeated removal is a no-op"), Health->RemoveMaxHealthModifier(Modifier.Id));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsHealthBossScaleTest, "SoulsNew.Health.D.Base500Plus25Percent", SoulsHealthTests::Flags)
bool FSoulsHealthBossScaleTest::RunTest(const FString& Parameters)
{
	auto Health = SoulsHealthTests::MakeHealth(500.0f);
	TestTrue(TEXT("Same contribution API"), Health->AddMaxHealthModifier(SoulsHealthTests::MakeModifier()));
	TestEqual(TEXT("Base stays 500"), Health->GetBaseMaxHealth(), 500.0f);
	TestEqual(TEXT("Max is 625"), Health->GetMaxHealth(), 625.0f);
	TestEqual(TEXT("Current is 625"), Health->GetCurrentHealth(), 625.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsHealthPreserveTest, "SoulsNew.Health.E.PreservePercentage", SoulsHealthTests::Flags)
bool FSoulsHealthPreserveTest::RunTest(const FString& Parameters)
{
	auto Health = SoulsHealthTests::MakeHealth();
	const auto Modifier = SoulsHealthTests::MakeModifier();
	TestEqual(TEXT("Damage to half health"), Health->TakeDamage(50.0f), 50.0f);
	TestTrue(TEXT("Add"), Health->AddMaxHealthModifier(Modifier));
	TestEqual(TEXT("Max"), Health->GetMaxHealth(), 125.0f);
	TestEqual(TEXT("Half of new max"), Health->GetCurrentHealth(), 62.5f);
	TestEqual(TEXT("Percentage"), Health->GetHealthPercentage(), 0.5f);
	TestTrue(TEXT("Remove"), Health->RemoveMaxHealthModifier(Modifier.Id));
	TestEqual(TEXT("Downscaling also preserves percentage"), Health->GetCurrentHealth(), 50.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsHealthDamageFloorTest, "SoulsNew.Health.F.DamageFloor", SoulsHealthTests::Flags)
bool FSoulsHealthDamageFloorTest::RunTest(const FString& Parameters)
{
	auto Health = SoulsHealthTests::MakeHealth();
	TestEqual(TEXT("Actual damage limited to available HP"), Health->TakeDamage(500.0f), 100.0f);
	TestEqual(TEXT("HP never negative"), Health->GetCurrentHealth(), 0.0f);
	TestTrue(TEXT("Lethal damage sets dead"), Health->IsDead());
	TestEqual(TEXT("Damage at zero is zero"), Health->TakeDamage(500.0f), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsHealthHealingCapTest, "SoulsNew.Health.G.HealingCap", SoulsHealthTests::Flags)
bool FSoulsHealthHealingCapTest::RunTest(const FString& Parameters)
{
	auto Health = SoulsHealthTests::MakeHealth();
	Health->TakeDamage(70.0f);
	TestEqual(TEXT("Actual healing only fills missing HP"), Health->Heal(1000.0f), 70.0f);
	TestEqual(TEXT("Cannot overheal"), Health->GetCurrentHealth(), 100.0f);
	TestEqual(TEXT("Healing full health is a no-op"), Health->Heal(1.0f), 0.0f);
	Health->TakeDamage(100.0f);
	TestEqual(TEXT("Healing cannot resurrect"), Health->Heal(1000.0f), 0.0f);
	TestEqual(TEXT("Dead stays at zero"), Health->GetCurrentHealth(), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsHealthSingleDeathTest, "SoulsNew.Health.H.SingleDeath", SoulsHealthTests::Flags)
bool FSoulsHealthSingleDeathTest::RunTest(const FString& Parameters)
{
	auto Health = SoulsHealthTests::MakeHealth();
	int32 DeathCount = 0;
	Health->Death().AddLambda([&DeathCount](USoulsHealthComponent*) { ++DeathCount; });
	Health->TakeDamage(100.0f);
	Health->TakeDamage(1.0f);
	Health->TakeDamage(1000.0f);
	TestEqual(TEXT("One death per life"), DeathCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsHealthIdsTest, "SoulsNew.Health.ModifierIds", SoulsHealthTests::Flags)
bool FSoulsHealthIdsTest::RunTest(const FString& Parameters)
{
	auto Health = SoulsHealthTests::MakeHealth();
	const auto Modifier = SoulsHealthTests::MakeModifier();
	TestTrue(TEXT("First ID accepted"), Health->AddMaxHealthModifier(Modifier));
	TestFalse(TEXT("Duplicate ID rejected"), Health->AddMaxHealthModifier(Modifier));
	auto ChangedDuplicate = Modifier;
	ChangedDuplicate.Magnitude = 1.0f;
	TestFalse(TEXT("Same ID cannot silently replace values"), Health->AddMaxHealthModifier(ChangedDuplicate));
	FSoulsAttributeModifier Found;
	TestTrue(TEXT("Find by ID"), Health->FindMaxHealthModifier(Modifier.Id, Found));
	TestEqual(TEXT("Original magnitude preserved"), Found.Magnitude, 0.25f);
	Found.Magnitude = 99.0f;
	TestEqual(TEXT("Returned copy cannot mutate aggregate"), Health->GetMaxHealth(), 125.0f);
	TestTrue(TEXT("Different ID can contribute independently"), Health->AddMaxHealthModifier(SoulsHealthTests::MakeModifier(2)));
	TestEqual(TEXT("Two additive contributions"), Health->GetMaxHealth(), 150.0f);
	TestTrue(TEXT("Remove only first"), Health->RemoveMaxHealthModifier(Modifier.Id));
	TestEqual(TEXT("Second remains active"), Health->GetMaxHealth(), 125.0f);
	TestFalse(TEXT("Removed ID cannot be found"), Health->FindMaxHealthModifier(Modifier.Id, Found));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsHealthOrderTest, "SoulsNew.Health.DeterministicModifierOrder", SoulsHealthTests::Flags)
bool FSoulsHealthOrderTest::RunTest(const FString& Parameters)
{
	auto Forward = SoulsHealthTests::MakeHealth();
	auto Reverse = SoulsHealthTests::MakeHealth();
	const TArray<FSoulsAttributeModifier> Modifiers = {
		SoulsHealthTests::MakeModifier(1, 20.0f, ESoulsAttributeModifierOperation::Flat),
		SoulsHealthTests::MakeModifier(2, 0.25f),
		SoulsHealthTests::MakeModifier(3, 0.25f),
		SoulsHealthTests::MakeModifier(4, 2.0f, ESoulsAttributeModifierOperation::Multiplicative),
		SoulsHealthTests::MakeModifier(5, 0.5f, ESoulsAttributeModifierOperation::Multiplicative)
	};
	for (const auto& Modifier : Modifiers)
	{
		TestTrue(TEXT("Forward insertion"), Forward->AddMaxHealthModifier(Modifier));
	}
	for (int32 Index = Modifiers.Num() - 1; Index >= 0; --Index)
	{
		TestTrue(TEXT("Reverse insertion"), Reverse->AddMaxHealthModifier(Modifiers[Index]));
	}
	TestEqual(TEXT("(100+20)*(1+0.25+0.25)*2*0.5"), Forward->GetMaxHealth(), 180.0f);
	TestTrue(TEXT("Exactly identical result for identical ID set"), Forward->GetMaxHealth() == Reverse->GetMaxHealth());
	TestTrue(TEXT("Base update accepted"), Forward->SetBaseMaxHealth(200.0f));
	TestEqual(TEXT("Recomputed from new base"), Forward->GetMaxHealth(), 330.0f);

	// Cancellation-sensitive sums would differ if aggregated in insertion order, even in double.
	auto CancellationForward = SoulsHealthTests::MakeHealth();
	auto CancellationReverse = SoulsHealthTests::MakeHealth();
	const TArray<FSoulsAttributeModifier> Cancellation = {
		SoulsHealthTests::MakeModifier(1, 1.0e20f, ESoulsAttributeModifierOperation::Flat),
		SoulsHealthTests::MakeModifier(2, -1.0e20f, ESoulsAttributeModifierOperation::Flat),
		SoulsHealthTests::MakeModifier(3, 3.0f, ESoulsAttributeModifierOperation::Flat)
	};
	for (const auto& Modifier : Cancellation) { TestTrue(TEXT("Cancellation forward"), CancellationForward->AddMaxHealthModifier(Modifier)); }
	for (int32 Index = Cancellation.Num() - 1; Index >= 0; --Index) { TestTrue(TEXT("Cancellation reverse"), CancellationReverse->AddMaxHealthModifier(Cancellation[Index])); }
	TestEqual(TEXT("Sorted cancellation result"), CancellationForward->GetMaxHealth(), 103.0f);
	TestTrue(TEXT("Cancellation result independent of insertion order"), CancellationForward->GetMaxHealth() == CancellationReverse->GetMaxHealth());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsHealthValidationTest, "SoulsNew.Health.InvalidRequestsAreAtomic", SoulsHealthTests::Flags)
bool FSoulsHealthValidationTest::RunTest(const FString& Parameters)
{
	auto Health = SoulsHealthTests::MakeHealth();
	int32 Events = 0;
	Health->HealthChanged().AddLambda([&Events](USoulsHealthComponent*, float, float) { ++Events; });
	Health->MaxHealthChanged().AddLambda([&Events](USoulsHealthComponent*, float, float) { ++Events; });
	const float InvalidValues[] = { -1.0f, 0.0f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity() };
	for (float Value : InvalidValues)
	{
		TestFalse(TEXT("Invalid base rejected"), Health->SetBaseMaxHealth(Value));
		TestEqual(TEXT("Invalid damage is a no-op"), Health->TakeDamage(Value), 0.0f);
		TestEqual(TEXT("Invalid healing is a no-op"), Health->Heal(Value), 0.0f);
	}
	TestFalse(TEXT("Uninitialized ID rejected"), Health->AddMaxHealthModifier({ FGuid(), ESoulsAttributeModifierOperation::AdditivePercent, 0.25f }));
	TestFalse(TEXT("Non-finite contribution rejected"), Health->AddMaxHealthModifier(SoulsHealthTests::MakeModifier(1, std::numeric_limits<float>::quiet_NaN())));
	TestFalse(TEXT("Unknown operation rejected"), Health->AddMaxHealthModifier(SoulsHealthTests::MakeModifier(1, 1.0f, static_cast<ESoulsAttributeModifierOperation>(255))));
	TestFalse(TEXT("Zero multiplier rejected"), Health->AddMaxHealthModifier(SoulsHealthTests::MakeModifier(1, 0.0f, ESoulsAttributeModifierOperation::Multiplicative)));
	TestFalse(TEXT("Negative multiplier rejected"), Health->AddMaxHealthModifier(SoulsHealthTests::MakeModifier(1, -1.0f, ESoulsAttributeModifierOperation::Multiplicative)));
	const auto Overflow = SoulsHealthTests::MakeModifier(1, TNumericLimits<float>::Max());
	TestFalse(TEXT("Overflow rejected before committing"), Health->AddMaxHealthModifier(Overflow));
	FSoulsAttributeModifier Found;
	TestFalse(TEXT("Failed contribution is not retained"), Health->FindMaxHealthModifier(Overflow.Id, Found));
	TestEqual(TEXT("No events on rejected requests"), Events, 0);
	TestEqual(TEXT("Base unchanged"), Health->GetBaseMaxHealth(), 100.0f);
	TestEqual(TEXT("Current unchanged"), Health->GetCurrentHealth(), 100.0f);
	TestEqual(TEXT("Max unchanged"), Health->GetMaxHealth(), 100.0f);
	TestTrue(TEXT("ID reusable after failed add"), Health->AddMaxHealthModifier(SoulsHealthTests::MakeModifier()));
	TestFalse(TEXT("Base change that would overflow rejected"), Health->SetBaseMaxHealth(TNumericLimits<float>::Max()));
	TestEqual(TEXT("Failed base update leaves base unchanged"), Health->GetBaseMaxHealth(), 100.0f);
	TestEqual(TEXT("Failed base update leaves modifier active"), Health->GetMaxHealth(), 125.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsHealthLifeCycleTest, "SoulsNew.Health.ExplicitLifeCycle", SoulsHealthTests::Flags)
bool FSoulsHealthLifeCycleTest::RunTest(const FString& Parameters)
{
	auto Health = SoulsHealthTests::MakeHealth();
	int32 DeathCount = 0;
	Health->Death().AddLambda([&DeathCount](USoulsHealthComponent*) { ++DeathCount; });
	Health->TakeDamage(100.0f);
	TestTrue(TEXT("Dead can receive max modifiers without resurrection"), Health->AddMaxHealthModifier(SoulsHealthTests::MakeModifier()));
	TestEqual(TEXT("Dead maximum changed"), Health->GetMaxHealth(), 125.0f);
	TestEqual(TEXT("Dead current remains zero"), Health->GetCurrentHealth(), 0.0f);
	TestEqual(TEXT("Heal dead is zero"), Health->Heal(125.0f), 0.0f);
	TestEqual(TEXT("No extra death from recalc"), DeathCount, 1);
	TestTrue(TEXT("Explicit new life"), Health->ResetHealthForNewLife());
	TestFalse(TEXT("Alive again"), Health->IsDead());
	TestEqual(TEXT("Existing modifiers retained"), Health->GetCurrentHealth(), 125.0f);
	Health->TakeDamage(1000.0f);
	Health->TakeDamage(1000.0f);
	TestEqual(TEXT("Exactly one death in each of two lives"), DeathCount, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsHealthEventsTest, "SoulsNew.Health.EventsAndReentrancy", SoulsHealthTests::Flags)
bool FSoulsHealthEventsTest::RunTest(const FString& Parameters)
{
	auto Health = SoulsHealthTests::MakeHealth();
	Health->TakeDamage(50.0f);
	TArray<FString> EventOrder;
	Health->MaxHealthChanged().AddLambda([this, &EventOrder](USoulsHealthComponent* Component, float Old, float New)
	{
		EventOrder.Add(TEXT("Max"));
		TestEqual(TEXT("Old max payload"), Old, 100.0f);
		TestEqual(TEXT("New max payload"), New, 125.0f);
		TestEqual(TEXT("Current already committed in max callback"), Component->GetCurrentHealth(), 62.5f);
		TestEqual(TEXT("Reentrant heal rejected"), Component->Heal(100.0f), 0.0f);
		TestFalse(TEXT("Reentrant base change rejected"), Component->SetBaseMaxHealth(500.0f));
		TestFalse(TEXT("Reentrant modifier rejected"), Component->AddMaxHealthModifier(SoulsHealthTests::MakeModifier(2)));
		TestFalse(TEXT("Reentrant removal rejected"), Component->RemoveMaxHealthModifier(SoulsHealthTests::MakeModifier().Id));
		TestFalse(TEXT("Reentrant reset rejected"), Component->ResetHealthForNewLife());
	});
	Health->HealthChanged().AddLambda([this, &EventOrder](USoulsHealthComponent* Component, float Old, float New)
	{
		EventOrder.Add(TEXT("Health"));
		TestEqual(TEXT("Old current payload"), Old, 50.0f);
		TestEqual(TEXT("New current payload"), New, 62.5f);
		TestEqual(TEXT("Max already committed in health callback"), Component->GetMaxHealth(), 125.0f);
	});
	TestTrue(TEXT("Add"), Health->AddMaxHealthModifier(SoulsHealthTests::MakeModifier()));
	TestEqual(TEXT("Exactly two change events"), EventOrder.Num(), 2);
	if (EventOrder.Num() == 2)
	{
		TestEqual(TEXT("Max first"), EventOrder[0], FString(TEXT("Max")));
		TestEqual(TEXT("Health second"), EventOrder[1], FString(TEXT("Health")));
	}
	TestTrue(TEXT("No-op base setter succeeds"), Health->SetBaseMaxHealth(100.0f));
	TestEqual(TEXT("No notifications for unchanged values"), EventOrder.Num(), 2);
	Health->MaxHealthChanged().Clear();
	Health->HealthChanged().Clear();
	EventOrder.Reset();
	Health->HealthChanged().AddLambda([this, &EventOrder](USoulsHealthComponent* Component, float, float New)
	{
		EventOrder.Add(TEXT("Health"));
		TestEqual(TEXT("Lethal current payload"), New, 0.0f);
		TestTrue(TEXT("Death state already committed"), Component->IsDead());
		TestEqual(TEXT("Reentrant damage rejected"), Component->TakeDamage(1.0f), 0.0f);
	});
	Health->Death().AddLambda([this, &EventOrder](USoulsHealthComponent* Component)
	{
		EventOrder.Add(TEXT("Death"));
		TestEqual(TEXT("Death observers see zero HP"), Component->GetCurrentHealth(), 0.0f);
	});
	Health->TakeDamage(500.0f);
	TestEqual(TEXT("Health and death once"), EventOrder.Num(), 2);
	if (EventOrder.Num() == 2) { TestEqual(TEXT("Death after health"), EventOrder[1], FString(TEXT("Death"))); }
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsHealthRecalculationTest, "SoulsNew.Health.RecalculationAndMinimum", SoulsHealthTests::Flags)
bool FSoulsHealthRecalculationTest::RunTest(const FString& Parameters)
{
	auto Health = SoulsHealthTests::MakeHealth();
	Health->TakeDamage(50.0f);
	const auto Modifier = SoulsHealthTests::MakeModifier();
	for (int32 Index = 0; Index < 50; ++Index)
	{
		TestTrue(TEXT("Repeated grant"), Health->AddMaxHealthModifier(Modifier));
		TestTrue(TEXT("Repeated removal"), Health->RemoveMaxHealthModifier(Modifier.Id));
	}
	TestEqual(TEXT("No drift in base"), Health->GetBaseMaxHealth(), 100.0f);
	TestEqual(TEXT("No drift in maximum"), Health->GetMaxHealth(), 100.0f);
	TestEqual(TEXT("No drift in current"), Health->GetCurrentHealth(), 50.0f);
	TestTrue(TEXT("Changing base is supported"), Health->SetBaseMaxHealth(500.0f));
	TestEqual(TEXT("Base change preserves percentage"), Health->GetCurrentHealth(), 250.0f);
	TestTrue(TEXT("Negative flat supported"), Health->AddMaxHealthModifier(SoulsHealthTests::MakeModifier(1, -1000.0f, ESoulsAttributeModifierOperation::Flat)));
	TestEqual(TEXT("Maximum has positive floor"), Health->GetMaxHealth(), 1.0f);
	TestEqual(TEXT("Percentage at floor"), Health->GetCurrentHealth(), 0.5f);
	TestTrue(TEXT("Removing negative contribution restores base"), Health->RemoveMaxHealthModifier(Modifier.Id));
	TestEqual(TEXT("Restored max"), Health->GetMaxHealth(), 500.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsHealthActorTest, "SoulsNew.Health.GenericActorInitialization", SoulsHealthTests::Flags)
bool FSoulsHealthActorTest::RunTest(const FString& Parameters)
{
	// A transient world/Actor, never a Player/Boss template or a saved map/asset.
	const UWorld::InitializationValues Values = UWorld::InitializationValues()
		.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false)
		.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
	AActor* Owner = World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("Generic Actor"), Owner)) { World->DestroyWorld(false); return false; }
	USoulsHealthComponent* Health = NewObject<USoulsHealthComponent>(Owner);
	Owner->AddInstanceComponent(Health);
	// Simulate a serialized per-actor base setting, before component initialization.
	FFloatProperty* BaseProperty = FindFProperty<FFloatProperty>(USoulsHealthComponent::StaticClass(), TEXT("BaseMaxHealth"));
	if (!TestNotNull(TEXT("Configurable base property"), BaseProperty)) { World->DestroyWorld(false); return false; }
	BaseProperty->SetPropertyValue_InContainer(Health, 500.0f);
	Health->RegisterComponent();
	if (!Health->HasBeenInitialized()) { static_cast<UActorComponent*>(Health)->InitializeComponent(); }
	TestTrue(TEXT("Registered on a generic Actor"), Health->IsRegistered());
	TestTrue(TEXT("Initialized"), Health->HasBeenInitialized());
	TestTrue(TEXT("Owner has no Character requirement"), Health->GetOwner() == Owner);
	TestEqual(TEXT("Serialized base initializes max"), Health->GetMaxHealth(), 500.0f);
	TestEqual(TEXT("Serialized base initializes full current"), Health->GetCurrentHealth(), 500.0f);
	TestFalse(TEXT("No Tick"), Health->PrimaryComponentTick.bCanEverTick);
	TestTrue(TEXT("Generic Actor takes the same modifier"), Health->AddMaxHealthModifier(SoulsHealthTests::MakeModifier()));
	TestEqual(TEXT("Generic Actor maximum"), Health->GetMaxHealth(), 625.0f);
	World->DestroyWorld(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
