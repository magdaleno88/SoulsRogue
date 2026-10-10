// Copyright Epic Games, Inc. All Rights Reserved.


#include "SoulsNewPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "UObject/ConstructorHelpers.h"
#include "Blueprint/UserWidget.h"
#include "SoulsNew.h"
#include "Widgets/Input/SVirtualJoystick.h"

ASoulsNewPlayerController::ASoulsNewPlayerController()
{
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> CombatInput(TEXT("/Game/SoulsNew/Input/IMC_SoulsCombat.IMC_SoulsCombat"));
	SoulsCombatMappingContext = CombatInput.Object;
}

void ASoulsNewPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogSoulsNew, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void ASoulsNewPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			if (SoulsCombatMappingContext) { Subsystem->AddMappingContext(SoulsCombatMappingContext, 0); }
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
}

bool ASoulsNewPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
