#if WITH_DEV_AUTOMATION_TESTS

#include "Attributes/SoulsHealthComponent.h"
#include "Characters/SoulsPrototypeBoss.h"
#include "Combat/SoulsMeleeCombatComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Items/SoulsItemDefinition.h"
#include "Misc/AutomationTest.h"
#include "SoulsNewCharacter.h"

#include "Tests/SoulsMeleeTestFixture.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsPlayerCombat, "SoulsNew.Melee.F.PlayerHasCombatComponent", SoulsMeleeTests::Flags)
bool FSoulsPlayerCombat::RunTest(const FString&)
{
	SoulsMeleeTests::FFixture F; auto* P = F.Player();
	if (!TestNotNull(TEXT("Real player"), P)) { return false; }
	TArray<USoulsMeleeCombatComponent*> C; P->GetComponents(C);
	TestEqual(TEXT("Exactly one combat"), C.Num(), 1);
	TestTrue(TEXT("Native subobject"), P->GetSoulsMeleeCombat()->HasAnyFlags(RF_DefaultSubObject));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsFrontTarget, "SoulsNew.Melee.G.TargetInFront", SoulsMeleeTests::Flags)
bool FSoulsFrontTarget::RunTest(const FString&)
{
	SoulsMeleeTests::FFixture F; auto* P = F.Player(); auto* T = F.Target();
	TestTrue(TEXT("Attack admitted"), F.Attack(P->GetSoulsMeleeCombat()));
	TestEqual(TEXT("Front loses exactly 100"), T->FindComponentByClass<USoulsHealthComponent>()->GetCurrentHealth(), 200.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsBossAttack, "SoulsNew.Melee.H.PrototypeBossDamage", SoulsMeleeTests::Flags)
bool FSoulsBossAttack::RunTest(const FString&)
{
	SoulsMeleeTests::FFixture F; auto* P = F.Player();
	FActorSpawnParameters Params; Params.bDeferConstruction = true;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* B = F.World->SpawnActor<ASoulsPrototypeBoss>(FVector(175, 0, 0), FRotator::ZeroRotator, Params);
	B->SetPrototypeItemDefinition(LoadObject<USoulsItemDefinition>(nullptr, TEXT("/Game/SoulsNew/Items/DA_Item_Vitality.DA_Item_Vitality")));
	B->FinishSpawning(FTransform(FVector(175, 0, 0)));
	TestEqual(TEXT("Boss starts 625"), B->GetSoulsHealth()->GetCurrentHealth(), 625.0f);
	TestTrue(TEXT("Same input API"), F.Attack(P->GetSoulsMeleeCombat()));
	TestEqual(TEXT("625 -> 525"), B->GetSoulsHealth()->GetCurrentHealth(), 525.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsAttackMiss, "SoulsNew.Melee.I.Miss", SoulsMeleeTests::Flags)
bool FSoulsAttackMiss::RunTest(const FString&)
{
	SoulsMeleeTests::FFixture F; auto* T = F.Target(FVector(1000, 0, 0));
	TestTrue(TEXT("Miss still admitted"), F.Attack(F.Combat(F.Actor())));
	TestEqual(TEXT("Out of range intact"), T->FindComponentByClass<USoulsHealthComponent>()->GetCurrentHealth(), 300.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsBehindTarget, "SoulsNew.Melee.J.TargetBehind", SoulsMeleeTests::Flags)
bool FSoulsBehindTarget::RunTest(const FString&)
{
	SoulsMeleeTests::FFixture F; auto* T = F.Target(FVector(-150, 0, 0));
	F.Attack(F.Combat(F.Actor()));
	TestEqual(TEXT("Behind intact"), T->FindComponentByClass<USoulsHealthComponent>()->GetCurrentHealth(), 300.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsAttackSelf, "SoulsNew.Melee.K.SelfIgnored", SoulsMeleeTests::Flags)
bool FSoulsAttackSelf::RunTest(const FString&)
{
	SoulsMeleeTests::FFixture F; auto* P = F.Player();
	F.Attack(P->GetSoulsMeleeCombat());
	TestEqual(TEXT("Player intact"), P->GetSoulsHealth()->GetCurrentHealth(), 100.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsDuplicateHit, "SoulsNew.Melee.L.SingleHitPerAttack", SoulsMeleeTests::Flags)
bool FSoulsDuplicateHit::RunTest(const FString&)
{
	SoulsMeleeTests::FFixture F; auto* T = F.Target();
	F.Collision(T, TEXT("SecondBody"), FVector(10, 0, 0));
	FCollisionObjectQueryParams Objects(ECC_WorldDynamic); TArray<FHitResult> Hits;
	F.World->SweepMultiByObjectType(Hits, FVector(41,0,0), FVector(200,0,0), FQuat::Identity, Objects, FCollisionShape::MakeSphere(40));
	TestTrue(TEXT("Physics actually reports multiple components"), Hits.Num() >= 2);
	int32 Events = 0; auto* H = T->FindComponentByClass<USoulsHealthComponent>();
	H->DamageReceived().AddLambda([&](USoulsHealthComponent*, const FSoulsDamageContext&, float, float) { ++Events; });
	F.Attack(F.Combat(F.Actor()));
	TestEqual(TEXT("Only one application"), Events, 1);
	TestEqual(TEXT("Only 100 lost"), H->GetCurrentHealth(), 200.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsGenericAttack, "SoulsNew.Melee.M.GenericActor", SoulsMeleeTests::Flags)
bool FSoulsGenericAttack::RunTest(const FString&)
{
	SoulsMeleeTests::FFixture F; auto* A = F.Actor(); auto* T = F.Target();
	FSoulsDamageContext Observed; auto* H = T->FindComponentByClass<USoulsHealthComponent>();
	H->DamageReceived().AddLambda([&](USoulsHealthComponent*, const FSoulsDamageContext& C, float, float) { Observed = C; });
	TestTrue(TEXT("Both plain AActor"), A->GetClass() == AActor::StaticClass() && T->GetClass() == AActor::StaticClass());
	TestTrue(TEXT("No attacker health required"), F.Attack(F.Combat(A)));
	TestEqual(TEXT("Generic target damaged"), H->GetCurrentHealth(), 200.0f);
	TestTrue(TEXT("Source propagated"), Observed.SourceActor == A);
	TestEqual(TEXT("Light ID"), Observed.AttackId, FName(TEXT("Attack.Light")));
	TestTrue(TEXT("Causal ID exists"), Observed.AttackInstanceId.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsAttackCooldown, "SoulsNew.Melee.N.Cooldown", SoulsMeleeTests::Flags)
bool FSoulsAttackCooldown::RunTest(const FString&)
{
	SoulsMeleeTests::FFixture F; auto* C = F.Combat(F.Actor()); auto* T = F.Target();
	TestTrue(TEXT("First admitted"), F.Attack(C));
	TestFalse(TEXT("Immediate rejected"), F.Attack(C));
	TestEqual(TEXT("No additional damage"), T->FindComponentByClass<USoulsHealthComponent>()->GetCurrentHealth(), 200.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsCooldownExpires, "SoulsNew.Melee.O.CooldownExpires", SoulsMeleeTests::Flags)
bool FSoulsCooldownExpires::RunTest(const FString&)
{
	SoulsMeleeTests::FFixture F; auto* C = F.Combat(F.Actor()); auto* T = F.Target();
	F.Attack(C); F.Advance(0.25f);
	TestFalse(TEXT("Before expiry"), F.Attack(C)); F.Advance(0.31f);
	TestTrue(TEXT("Deterministic world time expires cooldown"), F.Attack(C));
	TestEqual(TEXT("Two hits"), T->FindComponentByClass<USoulsHealthComponent>()->GetCurrentHealth(), 100.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsDeadAttacker, "SoulsNew.Melee.P.DeadAttacker", SoulsMeleeTests::Flags)
bool FSoulsDeadAttacker::RunTest(const FString&)
{
	SoulsMeleeTests::FFixture F; auto* A = F.Actor(); auto* H = F.Health(A); auto* T = F.Target();
	H->TakeDamage(300);
	TestFalse(TEXT("Dead attacker rejected"), F.Attack(F.Combat(A)));
	TestEqual(TEXT("Target intact"), T->FindComponentByClass<USoulsHealthComponent>()->GetCurrentHealth(), 300.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsDeadTarget, "SoulsNew.Melee.Q.DeadTarget", SoulsMeleeTests::Flags)
bool FSoulsDeadTarget::RunTest(const FString&)
{
	SoulsMeleeTests::FFixture F; auto* T = F.Target(); auto* H = T->FindComponentByClass<USoulsHealthComponent>();
	int32 Deaths = 0, Events = 0;
	H->Death().AddLambda([&](USoulsHealthComponent*) { ++Deaths; }); H->TakeDamage(300);
	H->DamageReceived().AddLambda([&](USoulsHealthComponent*, const FSoulsDamageContext&, float, float) { ++Events; });
	F.Attack(F.Combat(F.Actor()));
	TestEqual(TEXT("Still zero"), H->GetCurrentHealth(), 0.0f);
	TestEqual(TEXT("No new death"), Deaths, 1); TestEqual(TEXT("No damage event"), Events, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsAmbiguousTarget, "SoulsNew.Melee.V.AmbiguousHealthRejected", SoulsMeleeTests::Flags)
bool FSoulsAmbiguousTarget::RunTest(const FString&)
{
	SoulsMeleeTests::FFixture F; auto* T = F.Target(); auto* First = T->FindComponentByClass<USoulsHealthComponent>(); auto* Second = F.Health(T);
	F.Attack(F.Combat(F.Actor()));
	TestEqual(TEXT("First intact"), First->GetCurrentHealth(), 300.0f); TestEqual(TEXT("Second intact"), Second->GetCurrentHealth(), 300.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsMultiTarget, "SoulsNew.Melee.W.MultiTargetContext", SoulsMeleeTests::Flags)
bool FSoulsMultiTarget::RunTest(const FString&)
{
	SoulsMeleeTests::FFixture F; auto* A = F.Target(FVector(130, -20, 0)); auto* B = F.Target(FVector(180, 20, 0));
	FGuid IdA, IdB; TArray<FString> Order;
	auto* HA = A->FindComponentByClass<USoulsHealthComponent>(); auto* HB = B->FindComponentByClass<USoulsHealthComponent>();
	HA->DamageReceived().AddLambda([&](USoulsHealthComponent*, const FSoulsDamageContext& C, float, float) { IdA = C.AttackInstanceId; Order.Add(A->GetPathName()); });
	HB->DamageReceived().AddLambda([&](USoulsHealthComponent*, const FSoulsDamageContext& C, float, float) { IdB = C.AttackInstanceId; Order.Add(B->GetPathName()); });
	F.Attack(F.Combat(F.Actor()));
	TestEqual(TEXT("A damaged"), HA->GetCurrentHealth(), 200.0f); TestEqual(TEXT("B damaged"), HB->GetCurrentHealth(), 200.0f);
	TestTrue(TEXT("One valid shared causal ID"), IdA.IsValid() && IdA == IdB);
	if (TestEqual(TEXT("Two observers"), Order.Num(), 2)) { TestTrue(TEXT("Ordinal target order"), Order[0].Compare(Order[1], ESearchCase::CaseSensitive) < 0); }
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsAttackReentry, "SoulsNew.Melee.X.AttackReentryRejected", SoulsMeleeTests::Flags)
bool FSoulsAttackReentry::RunTest(const FString&)
{
	SoulsMeleeTests::FFixture F; auto* C = F.Combat(F.Actor()); auto* T = F.Target();
	T->FindComponentByClass<USoulsHealthComponent>()->DamageReceived().AddLambda([&](USoulsHealthComponent*, const FSoulsDamageContext&, float, float) { TestFalse(TEXT("Recursive attack rejected"), C->TryLightAttack()); });
	TestTrue(TEXT("Outer admitted"), F.Attack(C));
	TestEqual(TEXT("Only outer damage"), T->FindComponentByClass<USoulsHealthComponent>()->GetCurrentHealth(), 200.0f);
	return true;
}

#endif
