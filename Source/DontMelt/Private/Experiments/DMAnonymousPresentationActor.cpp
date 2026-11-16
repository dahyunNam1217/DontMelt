#include "Experiments/DMAnonymousPresentationActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ADMAnonymousPresentationActor::ADMAnonymousPresentationActor()
{
    bReplicates = true;
    bAlwaysRelevant = false;
    bOnlyRelevantToOwner = false;
    bNetUseOwnerRelevancy = false;
    SetReplicateMovement(false);
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PublicBody"));
    SetRootComponent(Body);
    Body->SetMobility(EComponentMobility::Movable);
    Body->SetIsReplicated(false);
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Body->SetGenerateOverlapEvents(false);
    Body->SetRelativeScale3D(FVector(0.6f, 0.6f, 1.8f));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Mesh.Succeeded())
    {
        Body->SetStaticMesh(Mesh.Object);
    }
}

void ADMAnonymousPresentationActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ADMAnonymousPresentationActor, PublicState);
}

bool ADMAnonymousPresentationActor::IsNetRelevantFor(
    const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation
) const
{
    if (ExcludedViewer.IsValid() && RealViewer == ExcludedViewer.Get())
    {
        return false;
    }
    return Super::IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
}

void ADMAnonymousPresentationActor::SetExcludedViewer(APlayerController* Controller)
{
    if (!HasAuthority())
    {
        return;
    }
    ExcludedViewer = Controller;
    // Listen Host에는 실제 Pawn도 있으므로 공유 Actor의 숨김 플래그를 변경하지 않는다.
    // 서버 로컬 메시만 숨기고 다른 연결에 보내는 상태에는 포함하지 않는다.
    Body->SetVisibility(!IsValid(Controller) || !Controller->IsLocalController());
}

void ADMAnonymousPresentationActor::SetPublicState(const FDMAnonymousPresentationState& State)
{
    if (!HasAuthority() || State.Location.ContainsNaN() || State.Rotation.ContainsNaN()
        || State.Velocity.ContainsNaN() || !FMath::IsFinite(State.SampleServerTime))
    {
        return;
    }
    PublicState = State;
    SetActorLocationAndRotation(State.Location, State.Rotation, false, nullptr, ETeleportType::TeleportPhysics);
    ForceNetUpdate();
}

void ADMAnonymousPresentationActor::OnRep_PublicState()
{
    if (!bReceivedState)
    {
        SetActorLocationAndRotation(PublicState.Location, PublicState.Rotation);
        bReceivedState = true;
    }
    // 원격 표현의 보간에만 사용한다. 게임 판정이나 소스 검색을 수행하지 않는다.
    SetActorTickEnabled(true);
}

void ADMAnonymousPresentationActor::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (HasAuthority() || !bReceivedState)
    {
        return;
    }
    const bool bSnap = FVector::DistSquared(GetActorLocation(), PublicState.Location) > FMath::Square(1000.0f);
    const FVector Location = bSnap ? FVector(PublicState.Location)
        : FMath::VInterpTo(GetActorLocation(), PublicState.Location, DeltaSeconds, 12.0f);
    const FRotator Rotation = bSnap ? PublicState.Rotation
        : FMath::RInterpTo(GetActorRotation(), PublicState.Rotation, DeltaSeconds, 12.0f);
    SetActorLocationAndRotation(Location, Rotation);
    if (Location.Equals(PublicState.Location, 0.5f) && Rotation.Equals(PublicState.Rotation, 0.2f))
    {
        SetActorLocationAndRotation(PublicState.Location, PublicState.Rotation);
        SetActorTickEnabled(false);
    }
}
