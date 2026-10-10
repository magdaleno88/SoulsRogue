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

namespace SoulsMeleeTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	struct FFixture
	{
		UWorld* World;
		FFixture()
		{
			const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(true).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL()); World->BeginPlay(); World->GetWorldSettings()->NotifyBeginPlay();
		}
		~FFixture()
		{
			World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
		}
		AActor* Actor(FVector Location = FVector::ZeroVector)
		{
			FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			return World->SpawnActor<AActor>(AActor::StaticClass(), Location, FRotator::ZeroRotator, P);
		}
		USphereComponent* Collision(AActor* Actor, FName Name = TEXT("Body"), FVector Offset = FVector::ZeroVector)
		{
			auto* C = NewObject<USphereComponent>(Actor, Name);
			Actor->AddInstanceComponent(C);
			C->SetSphereRadius(20); C->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			C->SetCollisionObjectType(ECC_WorldDynamic); C->SetCollisionResponseToAllChannels(ECR_Block);
			if (Actor->GetRootComponent()) { C->SetupAttachment(Actor->GetRootComponent()); C->SetRelativeLocation(Offset); }
			else { Actor->SetRootComponent(C); C->SetWorldLocation(Actor->GetActorLocation()); }
			C->RegisterComponent(); return C;
		}
		USoulsHealthComponent* Health(AActor* Actor, float Base = 300)
		{
			auto* H = NewObject<USoulsHealthComponent>(Actor);
			Actor->AddInstanceComponent(H); H->RegisterComponent(); H->SetBaseMaxHealth(Base); return H;
		}
		AActor* Target(FVector Location = FVector(150, 0, 0))
		{
			auto* A = Actor(); Collision(A); A->SetActorLocation(Location); Health(A); return A;
		}
		USoulsMeleeCombatComponent* Combat(AActor* Actor)
		{
			auto* C = NewObject<USoulsMeleeCombatComponent>(Actor); Actor->AddInstanceComponent(C); C->RegisterComponent(); return C;
		}
		ASoulsNewCharacter* Player()
		{
			FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* Class = LoadClass<ASoulsNewCharacter>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
			return World->SpawnActor<ASoulsNewCharacter>(Class, FVector::ZeroVector, FRotator::ZeroRotator, P);
		}
		void Advance(float Seconds = 0.56f) { World->Tick(LEVELTICK_All, Seconds); }
	};
}

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
	TestTrue(TEXT("Attack admitted"), P->GetSoulsMeleeCombat()->TryLightAttack());
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
	TestTrue(TEXT("Same input API"), P->GetSoulsMeleeCombat()->TryLightAttack());
	TestEqual(TEXT("625 -> 525"), B->GetSoulsHealth()->GetCurrentHealth(), 525.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsAttackMiss, "SoulsNew.Melee.I.Miss", SoulsMeleeTests::Flags)
bool FSoulsAttackMiss::RunTest(const FString&)
{
	SoulsMeleeTests::FFixture F; auto* T = F.Target(FVector(1000, 0, 0));
	TestTrue(TEXT("Miss still admitted"), F.Combat(F.Actor())->TryLightAttack());
	TestEqual(TEXT("Out of range intact"), T->FindComponentByClass<USoulsHealthComponent>()->GetCurrentHealth(), 300.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsBehindTarget, "SoulsNew.Melee.J.TargetBehind", SoulsMeleeTests::Flags)
bool FSoulsBehindTarget::RunTest(const FString&)
{
	SoulsMeleeTests::FFixture F; auto* T = F.Target(FVector(-150, 0, 0));
	F.Combat(F.Actor())->TryLightAttack();
	TestEqual(TEXT("Behind intact"), T->FindComponentByClass<USoulsHealthComponent>()->GetCurrentHealth(), 300.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsAttackSelf, "SoulsNew.Melee.K.SelfIgnored", SoulsMeleeTests::Flags)
bool FSoulsAttackSelf::RunTest(const FString&)
{
	SoulsMeleeTests::FFixture F; auto* P = F.Player();
	P->GetSoulsMeleeCombat()->TryLightAttack();
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
	F.Combat(F.Actor())->TryLightAttack();
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
	TestTrue(TEXT("No attacker health required"), F.Combat(A)->TryLightAttack());
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
	TestTrue(TEXT("First admitted"), C->TryLightAttack());
	TestFalse(TEXT("Immediate rejected"), C->TryLightAttack());
	TestEqual(TEXT("No additional damage"), T->FindComponentByClass<USoulsHealthComponent>()->GetCurrentHealth(), 200.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsCooldownExpires, "SoulsNew.Melee.O.CooldownExpires", SoulsMeleeTests::Flags)
bool FSoulsCooldownExpires::RunTest(const FString&)
{
	SoulsMeleeTests::FFixture F; auto* C = F.Combat(F.Actor()); auto* T = F.Target();
	C->TryLightAttack(); F.Advance(0.25f);
	TestFalse(TEXT("Before expiry"), C->TryLightAttack()); F.Advance(0.31f);
	TestTrue(TEXT("Deterministic world time expires cooldown"), C->TryLightAttack());
	TestEqual(TEXT("Two hits"), T->FindComponentByClass<USoulsHealthComponent>()->GetCurrentHealth(), 100.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsDeadAttacker, "SoulsNew.Melee.P.DeadAttacker", SoulsMeleeTests::Flags)
bool FSoulsDeadAttacker::RunTest(const FString&)
{
	SoulsMeleeTests::FFixture F; auto* A = F.Actor(); auto* H = F.Health(A); auto* T = F.Target();
	H->TakeDamage(300);
	TestFalse(TEXT("Dead attacker rejected"), F.Combat(A)->TryLightAttack());
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
	F.Combat(F.Actor())->TryLightAttack();
	TestEqual(TEXT("Still zero"), H->GetCurrentHealth(), 0.0f);
	TestEqual(TEXT("No new death"), Deaths, 1); TestEqual(TEXT("No damage event"), Events, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsAmbiguousTarget, "SoulsNew.Melee.V.AmbiguousHealthRejected", SoulsMeleeTests::Flags)
bool FSoulsAmbiguousTarget::RunTest(const FString&)
{
	SoulsMeleeTests::FFixture F; auto* T = F.Target(); auto* First = T->FindComponentByClass<USoulsHealthComponent>(); auto* Second = F.Health(T);
	F.Combat(F.Actor())->TryLightAttack();
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
	F.Combat(F.Actor())->TryLightAttack();
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
	TestTrue(TEXT("Outer admitted"), C->TryLightAttack());
	TestEqual(TEXT("Only outer damage"), T->FindComponentByClass<USoulsHealthComponent>()->GetCurrentHealth(), 200.0f);
	return true;
}

#endif
