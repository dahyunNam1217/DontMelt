#include "Relic/DMRelicSpawnPoint.h"

#include "Components/SceneComponent.h"

ADMRelicSpawnPoint::ADMRelicSpawnPoint()
{
    PrimaryActorTick.bCanEverTick = false;

    bReplicates = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));

    SetRootComponent(SceneRoot);
}