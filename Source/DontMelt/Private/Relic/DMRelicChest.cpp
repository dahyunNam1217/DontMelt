#include "Relic/DMRelicChest.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

#include "Relic/DMBaseZone.h"

ADMRelicChest::ADMRelicChest()
{
    PrimaryActorTick.bCanEverTick = false;

    bReplicates = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));

    SetRootComponent(SceneRoot);

    ChestMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChestMesh"));

    ChestMesh->SetupAttachment(SceneRoot);

    InteractionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));

    InteractionBounds->SetupAttachment(SceneRoot);

    InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

    InteractionBounds->SetCollisionObjectType(ECC_WorldDynamic);

    InteractionBounds->SetCollisionResponseToAllChannels(ECR_Ignore);

    InteractionBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

    InteractionBounds->SetGenerateOverlapEvents(false

    );
}

bool ADMRelicChest::HasValidBaseBinding() const
{
    if (!IsValid(BaseZone))
    {
        return false;
    }

    return BaseZone->GetBaseSlot() == BaseSlot;
}

bool ADMRelicChest::IsBaseActive() const
{
    if (!HasValidBaseBinding())
    {
        return false;
    }

    return BaseZone->IsActiveBase();
}