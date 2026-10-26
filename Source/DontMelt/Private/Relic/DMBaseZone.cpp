#include "Relic/DMBaseZone.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Net/UnrealNetwork.h"

ADMBaseZone::ADMBaseZone()
{
    PrimaryActorTick.bCanEverTick = false;

    bReplicates = true;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));

    SetRootComponent(SceneRoot);

    BaseBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("BaseBounds"));

    BaseBounds->SetupAttachment(SceneRoot);

    // 현재는 영역 정의만 한다.
    BaseBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADMBaseZone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ADMBaseZone, ActivityState);
}

void ADMBaseZone::SetActivityState(const EDMBaseActivityState NewState)
{
    check(HasAuthority());

    if (ActivityState == NewState)
    {
        return;
    }

    ActivityState = NewState;

    OnRep_ActivityState();
}

void ADMBaseZone::OnRep_ActivityState()
{
    // 복제 시 별도 Gameplay/UI 처리는 없다.
}