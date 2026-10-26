#include "Relic/DMRelic.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

ADMRelic::ADMRelic()
{
    PrimaryActorTick.bCanEverTick = false;

    bReplicates = true;
    SetReplicateMovement(true);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));

    SetRootComponent(SceneRoot);

    RelicMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RelicMesh"));

    RelicMesh->SetupAttachment(SceneRoot);

    RelicMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    InteractionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));

    InteractionBounds->SetupAttachment(SceneRoot);

    InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

    InteractionBounds->SetCollisionObjectType(ECC_WorldDynamic);

    InteractionBounds->SetCollisionResponseToAllChannels(ECR_Ignore);

    InteractionBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

    InteractionBounds->SetGenerateOverlapEvents(false);
}

bool ADMRelic::IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const
{
    /*
     * Registered / DepositProtected / Carried 상태는
     * 일반 Client가 월드 Actor 자체를 알 필요가 없다.
     *
     * PublicSpawn / Dropped일 때만
     * 일반 Actor relevancy 검사를 계속한다.
     */
    if (!ShouldBeVisibleInWorld())
    {
        return false;
    }

    return Super::IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
}

void ADMRelic::InitializeServer(
    const int32 InRelicId,
    const EDMRelicWorldState InInitialState,
    const int32 InRegisteredBaseSlot,
    const FTransform& InInitialTransform
)
{
    check(HasAuthority());

    RelicId = InRelicId;
    WorldState = InInitialState;
    RegisteredBaseSlot = InRegisteredBaseSlot;
    LastValidWorldTransform = InInitialTransform;

    RefreshWorldPresentation();
}

void ADMRelic::SetWorldState(const EDMRelicWorldState NewState)
{
    check(HasAuthority());

    if (WorldState == NewState)
    {
        return;
    }

    WorldState = NewState;

    RefreshWorldPresentation();

    /*
     * Public/Dropped ↔ 숨김 상태 변경 후
     * relevancy 재평가를 빠르게 유도한다.
     */
    ForceNetUpdate();
}

void ADMRelic::SetRegisteredBaseSlot(const int32 NewBaseSlot)
{
    check(HasAuthority());

    RegisteredBaseSlot = NewBaseSlot;
}

void ADMRelic::SetLastValidWorldTransform(const FTransform& NewTransform)
{
    check(HasAuthority());

    LastValidWorldTransform = NewTransform;
}

bool ADMRelic::ShouldBeVisibleInWorld() const
{
    return WorldState == EDMRelicWorldState::PublicSpawn || WorldState == EDMRelicWorldState::Dropped;
}

void ADMRelic::RefreshWorldPresentation()
{
    const bool bVisible = ShouldBeVisibleInWorld();

    SetActorHiddenInGame(!bVisible);

    /*
     * Server:
     * Registered / Carried 상태의 숨겨진 Relic은
     * Interaction Trace를 막아서는 안 된다.
     *
     * Client:
     * Public/Dropped Relic Actor가 relevancy로 생성되면
     * Constructor의 Query collision을 사용한다.
     */
    if (HasAuthority() && InteractionBounds != nullptr)
    {
        InteractionBounds->SetCollisionEnabled(
            bVisible ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision
        );
    }
}