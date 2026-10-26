#include "Framework/Match/DMRespawnPoint.h"

ADMRespawnPoint::ADMRespawnPoint(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = false;

    bReplicates = false;
}