// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsCharacter.h"
#include "PhysicsProjectile.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include <Components/StaticMeshComponent.h>
#include <PhysicsEngine/PhysicsHandleComponent.h>

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

APhysicsCharacter::APhysicsCharacter() {
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
		
	// Create a CameraComponent	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocation(FVector(-10.f, 0.f, 60.f)); // Position the camera
	FirstPersonCameraComponent->bUsePawnControlRotation = true;

	// Create a mesh component that will be used when being viewed from a '1st person' view (when controlling this pawn)
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetupAttachment(FirstPersonCameraComponent);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	Mesh1P->SetRelativeLocation(FVector(-30.f, 0.f, -150.f));

	m_PhysicsHandle = CreateDefaultSubobject<UPhysicsHandleComponent>(TEXT("PhysicsHandle"));
}

void APhysicsCharacter::BeginPlay() {
	Super::BeginPlay();
	
	m_CurrentStamina = m_MaxStamina;
}

void APhysicsCharacter::Tick(float DeltaSeconds) {
	Super::Tick(DeltaSeconds);

	if (m_bIsRunning) m_CurrentStamina -= m_StaminaDepletionRate * DeltaSeconds;
	else m_CurrentStamina += m_StaminaRecoveryRate * DeltaSeconds;

	m_CurrentStamina = FMath::Clamp(m_CurrentStamina, 0, m_MaxStamina);
	if (FMath::IsNearlyZero(m_CurrentStamina)) SetIsSprinting(false);

	FVector vForward = FirstPersonCameraComponent->GetForwardVector();
	FVector vStart = GetActorLocation() + FirstPersonCameraComponent->GetRelativeLocation();
	FVector vEnd = vStart + vForward * m_MaxGrabDistance;

	if (m_bIsGrabbing) m_PhysicsHandle->SetTargetLocation(vEnd);
	else {
		m_bCanGrab = (
			GetWorld()->LineTraceSingleByChannel(oHit, vStart, vEnd, ECC_Visibility) && 
			oHit.GetComponent() && 
			oHit.GetComponent()->Mobility == EComponentMobility::Movable
		);

		AActor* pHighlightedActor = oHit.GetActor();
		SetHighlightedMesh((pHighlightedActor && m_bCanGrab) ? pHighlightedActor->GetComponentByClass<UMeshComponent>() : nullptr);
	}
}

void APhysicsCharacter::NotifyControllerChanged() {
	Super::NotifyControllerChanged();

	// Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller)) {
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer())) {
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void APhysicsCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) {	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APhysicsCharacter::Move);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &APhysicsCharacter::Look);
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Triggered, this, &APhysicsCharacter::Sprint);
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &APhysicsCharacter::Sprint);
		EnhancedInputComponent->BindAction(PickUpAction, ETriggerEvent::Triggered, this, &APhysicsCharacter::GrabObject);
		EnhancedInputComponent->BindAction(PickUpAction, ETriggerEvent::Completed, this, &APhysicsCharacter::ReleaseObject);
	}
	else {
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void APhysicsCharacter::SetIsSprinting(bool NewIsSprinting) {
	if (UCharacterMovementComponent* pMovementComponent = GetCharacterMovement()) {
		pMovementComponent->MaxWalkSpeed = NewIsSprinting ? m_fSprintSpeed : m_fWalkSpeed;
		
		m_bIsRunning = NewIsSprinting && (GetVelocity().Length() > 0.f);
	}
}

void APhysicsCharacter::Move(const FInputActionValue& Value) {
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add movement 
		AddMovementInput(GetActorForwardVector(), MovementVector.Y);
		AddMovementInput(GetActorRightVector(), MovementVector.X);
	}
}

void APhysicsCharacter::Look(const FInputActionValue& Value) {
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void APhysicsCharacter::Sprint(const FInputActionValue& Value) {
	SetIsSprinting(Value.Get<bool>());
}

void APhysicsCharacter::GrabObject(const FInputActionValue& Value) {
	if (!m_bIsGrabbing && m_bCanGrab) {
		m_PhysicsHandle->GrabComponentAtLocationWithRotation(oHit.GetComponent(), oHit.BoneName, oHit.Location, oHit.GetActor()->GetActorRotation());
			
		m_bIsGrabbing = true;
	}
}

void APhysicsCharacter::ReleaseObject(const FInputActionValue& Value) {
	m_PhysicsHandle->ReleaseComponent();
	m_bIsGrabbing = false;
}

void APhysicsCharacter::SetHighlightedMesh(UMeshComponent* StaticMesh) {
	if(m_HighlightedMesh) m_HighlightedMesh->SetOverlayMaterial(nullptr);
	
	m_HighlightedMesh = StaticMesh;

	if (m_HighlightedMesh) m_HighlightedMesh->SetOverlayMaterial(m_HighlightMaterial);
}
