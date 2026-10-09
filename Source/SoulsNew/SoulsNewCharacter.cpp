// Copyright Epic Games, Inc. All Rights Reserved.

#include "SoulsNewCharacter.h"
#include "Attributes/SoulsHealthComponent.h"
#include "Items/SoulsItemComponent.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "SoulsNew.h"

ASoulsNewCharacter::ASoulsNewCharacter()
{
	SoulsHealth = CreateDefaultSubobject<USoulsHealthComponent>(TEXT("SoulsHealth"));
	SoulsItems = CreateDefaultSubobject<USoulsItemComponent>(TEXT("SoulsItems"));
	// Health's existing default is 100; inherited Blueprint defaults have no override for these new subobjects.
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void ASoulsNewCharacter::BeginPlay()
{
	Super::BeginPlay();
#if !UE_BUILD_SHIPPING
	UE_LOG(LogSoulsNew, Display, TEXT("[SoulsPrototype] Player initialized: %s Health=%g/%g Base=%g Grants=%d"),
		*GetName(), SoulsHealth->GetCurrentHealth(), SoulsHealth->GetMaxHealth(), SoulsHealth->GetBaseMaxHealth(), SoulsItems->GetGrantCount());
	PrototypeGrantLogHandle = SoulsItems->ItemGranted().AddUObject(this, &ASoulsNewCharacter::LogPrototypeItemGrant);
#endif
}

void ASoulsNewCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
#if !UE_BUILD_SHIPPING
	SoulsItems->ItemGranted().Remove(PrototypeGrantLogHandle);
#endif
	Super::EndPlay(EndPlayReason);
}

#if !UE_BUILD_SHIPPING
void ASoulsNewCharacter::LogPrototypeItemGrant(USoulsItemComponent* Items, FGuid GrantId, FName ItemId)
{
	UE_LOG(LogSoulsNew, Display, TEXT("[SoulsPrototype] Player received %s Grant=%s MaxHealth=%g Base=%g"),
		*ItemId.ToString(), *GrantId.ToString(), SoulsHealth->GetMaxHealth(), SoulsHealth->GetBaseMaxHealth());
}
#endif

void ASoulsNewCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ASoulsNewCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ASoulsNewCharacter::Look);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ASoulsNewCharacter::Look);
	}
	else
	{
		UE_LOG(LogSoulsNew, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void ASoulsNewCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void ASoulsNewCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void ASoulsNewCharacter::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void ASoulsNewCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void ASoulsNewCharacter::DoJumpStart()
{
	// signal the character to jump
	Jump();
}

void ASoulsNewCharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}
