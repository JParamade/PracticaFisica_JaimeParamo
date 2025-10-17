// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsGameMode.h"
#include "PhysicsCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "BreakableTarget.h"
#include <Kismet/GameplayStatics.h>

APhysicsGameMode::APhysicsGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/Blueprints/BP_FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;
}

void APhysicsGameMode::BeginPlay()
{
	Super::BeginPlay();

	ABreakableTarget::OnTargetBroken.AddDynamic(this, &APhysicsGameMode::ReduceRemainingTargets);
	TArray<AActor*> BreakableTargets;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABreakableTarget::StaticClass(), BreakableTargets);
	m_TotalTargets = BreakableTargets.Num();
	m_RemainingTargets = m_TotalTargets;
}

void APhysicsGameMode::ReduceRemainingTargets(ABreakableTarget* BrokenTarget)
{
	if (m_RemainingTargets >= 1) {
		m_RemainingTargets--;
		OnTargetCountChange.Broadcast();
	}

	if (m_RemainingTargets <= 0) OnWinConditionMet.Broadcast();
}
