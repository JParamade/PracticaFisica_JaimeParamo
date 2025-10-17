// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsWeaponComponent.h"
#include "PhysicsCharacter.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Animation/AnimInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Components/PrimitiveComponent.h"
#include "PhysicsProjectile.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include <Components/SphereComponent.h>
#include <Camera/CameraComponent.h>
#include "GameFramework/DamageType.h"

// Sets default values for this component's properties
UPhysicsWeaponComponent::UPhysicsWeaponComponent()
{
  // Default offset from the character location for projectiles to spawn
  MuzzleOffset = FVector(100.0f, 0.0f, 10.0f);
  m_ImpulseStrength = 10000.f;
}

void UPhysicsWeaponComponent::BeginPlay()
{
  Super::BeginPlay();

  m_lActorsToIgnore = { Character };
}


void UPhysicsWeaponComponent::Fire()
{
  if (Character == nullptr || Character->GetController() == nullptr)
  {
    return;
  }

  // Try and play the sound if specified
  if (FireSound != nullptr)
  {
    UGameplayStatics::PlaySoundAtLocation(this, FireSound, Character->GetActorLocation());
  }

  // Try and play a firing animation if specified
  if (FireAnimation != nullptr)
  {
    // Get the animation object for the arms mesh
    UAnimInstance* AnimInstance = Character->GetMesh1P()->GetAnimInstance();
    if (AnimInstance != nullptr)
    {
      AnimInstance->Montage_Play(FireAnimation, 1.f);
    }
  }
}

void UPhysicsWeaponComponent::ApplyDamage(AActor* _pOtherActor, FHitResult _oHitInfo, APhysicsProjectile* _pProjectile) {
  if (!_pOtherActor || !m_WeaponDamageType) return;
  
  UPrimitiveComponent* pHitComponent = _oHitInfo.GetComponent();

  AActor* pDamageEmitter = nullptr;
  if (_pProjectile) pDamageEmitter = _pProjectile;
  else pDamageEmitter = Character;

  TSubclassOf<UDamageType> DamageTypeClass = m_WeaponDamageType->m_DamageType;

  switch (m_WeaponDamageType->m_ImpulseType) {
  case EImpulseType::RAY:
    UGameplayStatics::ApplyPointDamage(_pOtherActor, m_WeaponDamageType->m_Damage, _oHitInfo.ImpactNormal * -1.0f, _oHitInfo, Character->GetController(), pDamageEmitter, DamageTypeClass);
    break;
  case EImpulseType::POINT:
    UGameplayStatics::ApplyPointDamage(_pOtherActor, m_WeaponDamageType->m_Damage, _oHitInfo.ImpactNormal * -1.0f, _oHitInfo, Character->GetController(), pDamageEmitter, DamageTypeClass);
    break;
  case EImpulseType::RADIAL:
    if (_pProjectile) UGameplayStatics::ApplyRadialDamage(GetWorld(), m_WeaponDamageType->m_Damage, _pProjectile->GetActorLocation(), _pProjectile->m_Radius, DamageTypeClass, m_lActorsToIgnore, _pProjectile, Character->GetController());
    break;
  default:
    UGameplayStatics::ApplyDamage(_pOtherActor, m_WeaponDamageType->m_Damage, Character->GetController(), Character, DamageTypeClass);
    break;
  }

  if (pHitComponent && pHitComponent->IsSimulatingPhysics()) {
    switch (m_WeaponDamageType->m_ImpulseType) {
    case EImpulseType::RAY:
      pHitComponent->AddImpulseAtLocation(_oHitInfo.ImpactNormal * -1.0f * m_ImpulseStrength, _oHitInfo.ImpactPoint, _oHitInfo.BoneName);
      break;
    case EImpulseType::POINT:
      if (_pProjectile) pHitComponent->AddImpulseAtLocation(_pProjectile->GetVelocity().GetSafeNormal() * m_ImpulseStrength, _oHitInfo.ImpactPoint, _oHitInfo.BoneName);
      break;
    }
  }

  if (m_WeaponDamageType->m_ImpulseType == EImpulseType::RADIAL && _pProjectile) {
    TArray<AActor*> lOverlappedActors;

    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(EObjectTypeQuery::ObjectTypeQuery4);

    m_lActorsToIgnore.Add(_pProjectile);

    bool bHasOverlapped = UKismetSystemLibrary::SphereOverlapActors(
      GetWorld(),
      _pProjectile->GetActorLocation(),
      _pProjectile->m_Radius,
      ObjectTypes,
      AActor::StaticClass(),
      m_lActorsToIgnore,
      lOverlappedActors
    );

    if (bHasOverlapped) {
      for (AActor* pOverlappedActor : lOverlappedActors) {
        TArray<UPrimitiveComponent*> lPrimitiveComponents;
        pOverlappedActor->GetComponents<UPrimitiveComponent>(lPrimitiveComponents);

        for (UPrimitiveComponent* pOverlappedComponent : lPrimitiveComponents) {
          if (pOverlappedComponent && pOverlappedComponent->IsSimulatingPhysics()) {
            pOverlappedComponent->AddRadialForce(
              _pProjectile->GetActorLocation(),
              _pProjectile->m_Radius,
              m_ImpulseStrength,
              ERadialImpulseFalloff::RIF_Linear,
              true
            );
          }
        }
      }
    }
  }
}

bool UPhysicsWeaponComponent::AttachWeapon(APhysicsCharacter* TargetCharacter)
{
  Character = TargetCharacter;

  // Check that the character is valid, and has no weapon component yet
  if (Character == nullptr || Character->GetInstanceComponents().FindItemByClass<UPhysicsWeaponComponent>())
  {
    return false;
  }

  // Attach the weapon to the First Person Character
  FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
  AttachToComponent(Character->GetMesh1P(), AttachmentRules, FName(TEXT("GripPoint")));

  // Set up action bindings
  if (APlayerController* PlayerController = Cast<APlayerController>(Character->GetController()))
  {
    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
    {
      // Set the priority of the mapping to 1, so that it overrides the Jump action with the Fire action when using touch input
      Subsystem->AddMappingContext(FireMappingContext, 1);
    }

    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerController->InputComponent))
    {
      // Fire
      EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Triggered, this, &UPhysicsWeaponComponent::Fire);
    }
  }

  return true;
}

void UPhysicsWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
  // ensure we have a character owner
  if (Character != nullptr)
  {
    // remove the input mapping context from the Player Controller
    if (APlayerController* PlayerController = Cast<APlayerController>(Character->GetController()))
    {
      if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
      {
        Subsystem->RemoveMappingContext(FireMappingContext);
      }
    }
  }

  // maintain the EndPlay call chain
  Super::EndPlay(EndPlayReason);
}