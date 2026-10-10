#if WITH_DEV_AUTOMATION_TESTS

#include "Attributes/SoulsHealthComponent.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

namespace SoulsDamageTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter;
	TStrongObjectPtr<USoulsHealthComponent> Health() { return TStrongObjectPtr<USoulsHealthComponent>(NewObject<USoulsHealthComponent>()); }
	FSoulsDamageContext Context(float Amount)
	{
		FSoulsDamageContext Result;
		Result.DamageAmount = Amount;
		Result.AttackId = TEXT("Attack.Light");
		Result.AttackInstanceId = FGuid::NewGuid();
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsContextDamage, "SoulsNew.DamageContext.A.ContextDamage", SoulsDamageTests::Flags)
bool FSoulsContextDamage::RunTest(const FString&)
{
	auto H = SoulsDamageTests::Health();
	TestEqual(TEXT("Actual damage"), H->ApplyDamage(SoulsDamageTests::Context(25)), 25.0f);
	TestEqual(TEXT("100 -> 75"), H->GetCurrentHealth(), 75.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsContextPropagation, "SoulsNew.DamageContext.B.ContextPropagation", SoulsDamageTests::Flags)
bool FSoulsContextPropagation::RunTest(const FString&)
{
	auto H = SoulsDamageTests::Health();
	TStrongObjectPtr<AActor> Source(NewObject<AActor>());
	TStrongObjectPtr<AActor> Instigator(NewObject<AActor>());
	auto C = SoulsDamageTests::Context(25);
	C.SourceActor = Source.Get(); C.InstigatorActor = Instigator.Get();
	C.HitLocation = FVector(1, 2, 3); C.HitNormal = FVector::UpVector;
	const FSoulsDamageContext Expected = C;
	int32 Events = 0;
	H->HealthChanged().AddLambda([&](USoulsHealthComponent*, float, float) { C.DamageAmount = 999; });
	H->DamageReceived().AddLambda([&](USoulsHealthComponent* Receiver, const FSoulsDamageContext& Payload, float Applied, float Current)
	{
		++Events;
		TestTrue(TEXT("Receiver"), Receiver == H.Get());
		TestEqual(TEXT("Snapshot request"), Payload.DamageAmount, Expected.DamageAmount);
		TestTrue(TEXT("Source observer"), Payload.SourceActor == Source.Get());
		TestTrue(TEXT("Instigator observer"), Payload.InstigatorActor == Instigator.Get());
		TestEqual(TEXT("Attack ID"), Payload.AttackId, Expected.AttackId);
		TestEqual(TEXT("Causal ID"), Payload.AttackInstanceId, Expected.AttackInstanceId);
		TestEqual(TEXT("Location"), Payload.HitLocation, Expected.HitLocation);
		TestEqual(TEXT("Normal"), Payload.HitNormal, Expected.HitNormal);
		TestEqual(TEXT("Actual applied"), Applied, 25.0f);
		TestEqual(TEXT("Confirmed resulting health"), Current, 75.0f);
		TestEqual(TEXT("Reentrant damage rejected"), H->ApplyDamage(Expected), 0.0f);
		TestEqual(TEXT("Reentrant heal rejected"), H->Heal(1), 0.0f);
	});
	H->ApplyDamage(C);
	TestEqual(TEXT("Exactly one damage notification"), Events, 1);
	TestEqual(TEXT("Observers cannot alter committed health"), H->GetCurrentHealth(), 75.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsLegacyDamage, "SoulsNew.DamageContext.C.LegacyTakeDamage", SoulsDamageTests::Flags)
bool FSoulsLegacyDamage::RunTest(const FString&)
{
	auto H = SoulsDamageTests::Health();
	int32 Events = 0;
	H->DamageReceived().AddLambda([&](USoulsHealthComponent*, const FSoulsDamageContext& C, float Applied, float Current)
	{
		++Events;
		TestEqual(TEXT("Legacy request wrapped"), C.DamageAmount, 25.0f);
		TestTrue(TEXT("Legacy unspecified attack"), C.AttackId.IsNone());
		TestEqual(TEXT("Legacy actual"), Applied, 25.0f);
		TestEqual(TEXT("Legacy result"), Current, 75.0f);
	});
	TestEqual(TEXT("Legacy return"), H->TakeDamage(25), 25.0f);
	TestEqual(TEXT("Legacy notification"), Events, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsInvalidContextDamage, "SoulsNew.DamageContext.D.InvalidDamage", SoulsDamageTests::Flags)
bool FSoulsInvalidContextDamage::RunTest(const FString&)
{
	auto H = SoulsDamageTests::Health();
	int32 Events = 0;
	H->DamageReceived().AddLambda([&](USoulsHealthComponent*, const FSoulsDamageContext&, float, float) { ++Events; });
	for (float Amount : {0.0f, -25.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
	{
		TestEqual(TEXT("Rejected amount"), H->ApplyDamage(SoulsDamageTests::Context(Amount)), 0.0f);
	}
	TestEqual(TEXT("Unchanged"), H->GetCurrentHealth(), 100.0f);
	TestEqual(TEXT("No events"), Events, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsContextSingleDeath, "SoulsNew.DamageContext.E.DeathStillSingle", SoulsDamageTests::Flags)
bool FSoulsContextSingleDeath::RunTest(const FString&)
{
	auto H = SoulsDamageTests::Health();
	int32 Deaths = 0, Events = 0;
	H->Death().AddLambda([&](USoulsHealthComponent*) { ++Deaths; });
	H->DamageReceived().AddLambda([&](USoulsHealthComponent*, const FSoulsDamageContext&, float Applied, float Current)
	{
		++Events;
		TestEqual(TEXT("Overkill reports actual loss"), Applied, 100.0f);
		TestEqual(TEXT("Zero resulting health"), Current, 0.0f);
		TestTrue(TEXT("Dead already committed before observers"), H->IsDead());
		TestEqual(TEXT("Damage precedes death observer"), Deaths, 0);
	});
	TestEqual(TEXT("Lethal actual"), H->ApplyDamage(SoulsDamageTests::Context(1000)), 100.0f);
	TestEqual(TEXT("Dead target no-op"), H->ApplyDamage(SoulsDamageTests::Context(1000)), 0.0f);
	H->TakeDamage(1);
	TestEqual(TEXT("One damage"), Events, 1);
	TestEqual(TEXT("One death"), Deaths, 1);
	return true;
}

#endif
