// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapons/HitscanWeaponComponent.h"
#include <Kismet/KismetSystemLibrary.h>
#include <Kismet/GameplayStatics.h>
#include "PhysicsCharacter.h"
#include "PhysicsWeaponComponent.h"
#include <Camera/CameraComponent.h>
#include <Components/SphereComponent.h>

void UHitscanWeaponComponent::Fire()
{
	Super::Fire();

  if (!GetOwner()) return;

  FVector vStart = GetComponentLocation();
  FVector vForwardVector = Character->FirstPersonCameraComponent->GetForwardVector();
  FVector vEnd = vStart + (vForwardVector * m_fRange);

  FHitResult oHitResult;
  FCollisionQueryParams oParams;
  if (GetWorld()->LineTraceSingleByChannel(oHitResult, vStart, vEnd, ECC_Visibility, oParams)) {
    AActor* pOtherActor = oHitResult.GetActor();
    ApplyDamage(pOtherActor, oHitResult);
    onHitscanImpact.Broadcast(pOtherActor, oHitResult.ImpactPoint, vForwardVector);
  }
}
