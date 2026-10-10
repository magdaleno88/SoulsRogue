#if WITH_DEV_AUTOMATION_TESTS

#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "SoulsNewCharacter.h"
#include "SoulsNewPlayerController.h"
#include "UObject/UnrealType.h"

namespace SoulsCombatInputTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter;
	UInputAction* Action() { return LoadObject<UInputAction>(nullptr, TEXT("/Game/SoulsNew/Input/IA_LightAttack.IA_LightAttack")); }
	UInputMappingContext* Mapping() { return LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/SoulsNew/Input/IMC_SoulsCombat.IMC_SoulsCombat")); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsCombatActionLoads, "SoulsNew.CombatInput.R.LightAttackInputAssetLoads", SoulsCombatInputTests::Flags)
bool FSoulsCombatActionLoads::RunTest(const FString&)
{
	auto* A = SoulsCombatInputTests::Action(); if (!TestNotNull(TEXT("Real action"), A)) { return false; }
	TestTrue(TEXT("Boolean button"), A->ValueType == EInputActionValueType::Boolean);
	auto* Class = LoadClass<ASoulsNewCharacter>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
	if (!TestNotNull(TEXT("Real player class"), Class)) { return false; }
	TestTrue(TEXT("Real player retains exact action"), Class->GetDefaultObject<ASoulsNewCharacter>()->GetLightAttackAction() == A);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsCombatMappingLoads, "SoulsNew.CombatInput.S.CombatMappingLoads", SoulsCombatInputTests::Flags)
bool FSoulsCombatMappingLoads::RunTest(const FString&)
{
	auto* M = SoulsCombatInputTests::Mapping(); if (!TestNotNull(TEXT("Real mapping"), M)) { return false; }
	auto* Class = LoadClass<ASoulsNewPlayerController>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.BP_ThirdPersonPlayerController_C"));
	if (!TestNotNull(TEXT("Real controller class"), Class)) { return false; }
	TestTrue(TEXT("Real controller retains exact context"), Class->GetDefaultObject<ASoulsNewPlayerController>()->GetSoulsCombatMappingContext() == M);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsLeftMouseMapping, "SoulsNew.CombatInput.T.LeftMouseMapped", SoulsCombatInputTests::Flags)
bool FSoulsLeftMouseMapping::RunTest(const FString&)
{
	auto* M = SoulsCombatInputTests::Mapping(); auto* A = SoulsCombatInputTests::Action();
	if (!TestNotNull(TEXT("Context"), M) || !TestNotNull(TEXT("Action"), A)) { return false; }
	int32 Matches = 0;
	for (const auto& Key : M->GetMappings()) { if (Key.Action == A && Key.Key == EKeys::LeftMouseButton) { ++Matches; } }
	TestEqual(TEXT("One exact Left Mouse mapping"), Matches, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsExistingInput, "SoulsNew.CombatInput.U.ExistingInputStillValid", SoulsCombatInputTests::Flags)
bool FSoulsExistingInput::RunTest(const FString&)
{
	auto* Class = LoadClass<ASoulsNewCharacter>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
	auto* M = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Input/IMC_Default.IMC_Default"));
	auto* Mouse = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Input/IMC_MouseLook.IMC_MouseLook"));
	if (!TestNotNull(TEXT("Player"), Class) || !TestNotNull(TEXT("Default context"), M) || !TestNotNull(TEXT("Mouse context"), Mouse)) { return false; }
	const UObject* CDO = Class->GetDefaultObject();
	for (const FName Property : {FName(TEXT("MoveAction")), FName(TEXT("LookAction")), FName(TEXT("JumpAction")), FName(TEXT("MouseLookAction"))})
	{
		const auto* P = FindFProperty<FObjectPropertyBase>(Class, Property);
		if (!TestNotNull(TEXT("Existing input property"), P)) { return false; }
		const auto* A = Cast<UInputAction>(P->GetObjectPropertyValue_InContainer(CDO));
		if (!TestNotNull(TEXT("Existing configured action"), A)) { return false; }
		const bool bJump = Property == TEXT("JumpAction");
		TestTrue(TEXT("Existing action value type"), A->ValueType == (bJump ? EInputActionValueType::Boolean : EInputActionValueType::Axis2D));
		bool bFound = false;
		for (const auto& Key : M->GetMappings()) { bFound |= Key.Action == A; }
		for (const auto& Key : Mouse->GetMappings()) { bFound |= Key.Action == A; }
		TestTrue(TEXT("Existing action mapped"), bFound);
	}
	return true;
}

#endif
