#include "BreakableTarget.h"
#include <GeometryCollection/GeometryCollectionComponent.h>

// Sets default values
ABreakableTarget::ABreakableTarget()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	StaticMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMesh"));
	SetRootComponent(StaticMesh);

	GeometryCollection = CreateDefaultSubobject<UGeometryCollectionComponent>(TEXT("GeometryCollection"));
	GeometryCollection->SetupAttachment(StaticMesh);
	
	GeometryCollection->SetNotifyBreaks(true);
}

void ABreakableTarget::BeginPlay() {
	GeometryCollection->OnChaosBreakEvent.AddDynamic(this, &ABreakableTarget::GeometryCollectionBroken);
}

void ABreakableTarget::GeometryCollectionBroken(const FChaosBreakEvent& BreakEvent)
{
	if (!m_IsBroken) {
		OnTargetBroken.Broadcast(this);
		m_IsBroken = true;
	}
}

