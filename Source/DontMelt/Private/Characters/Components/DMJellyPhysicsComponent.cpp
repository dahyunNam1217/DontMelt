#include "Characters/Components/DMJellyPhysicsComponent.h"

#include "Characters/DMJellyCharacterBase.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/EngineTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogDMJellyPhysics, Log, All);

UDMJellyPhysicsComponent::UDMJellyPhysicsComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    SetIsReplicatedByDefault(true);
}

#pragma region Death Presentation

bool UDMJellyPhysicsComponent::StartDeathRagdollPresentation()
{
    ADMJellyCharacterBase* OwnerCharacter = Cast<ADMJellyCharacterBase>(GetOwner());

    if (OwnerCharacter == nullptr || !OwnerCharacter->HasAuthority())
    {
        return false;
    }

    /*
     * Gameplay Death 상태를 이 Component에서
     * 판단하거나 변경하지 않는다.
     *
     * GameMode가 Death Commit을 완료한 뒤
     * Presentation만 요청한다.
     */
    MulticastStartDeathRagdollPresentation();

    UE_LOG(LogDMJellyPhysics, Log, TEXT("[%s] Death ragdoll presentation requested."), *GetNameSafe(OwnerCharacter));

    return true;
}

void UDMJellyPhysicsComponent::MulticastStartDeathRagdollPresentation_Implementation()
{
    ApplyDeathRagdollPresentation();
}

void UDMJellyPhysicsComponent::ApplyDeathRagdollPresentation()
{
    ADMJellyCharacterBase* OwnerCharacter = Cast<ADMJellyCharacterBase>(GetOwner());

    if (OwnerCharacter == nullptr)
    {
        return;
    }

    /*
     * ==================================================
     * Actual Death Presentation Event
     * ==================================================
     *
     * Gameplay LifeState를 여기서 판단하지 않는다.
     *
     * 이 함수 자체가 Server의 실제 Death Commit 후
     * 호출되는 Death 전용 Multicast 경로이므로,
     * Local Presentation 시작 사실만 기록한다.
     */
    if (!bDeathPresentationActive)
    {
        bDeathPresentationActive = true;

        DeathPresentationStartedDelegate.Broadcast();
    }

    UCapsuleComponent* CapsuleComponent = OwnerCharacter->GetCapsuleComponent();

    USkeletalMeshComponent* MeshComponent = OwnerCharacter->GetMesh();

    /*
     * Dead Character의 Capsule은
     * Pawn Collision / Attack Query /
     * 이동 충돌에 더 이상 참여하면 안 된다.
     */
    if (CapsuleComponent != nullptr)
    {
        CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    if (!ensureMsgf(
            MeshComponent != nullptr,
            TEXT("[%s] Death ragdoll failed: SkeletalMeshComponent is null."),
            *GetNameSafe(OwnerCharacter)
        ))
    {
        return;
    }

    /*
     * 같은 Presentation 요청이
     * 중복 실행돼도 다시 설정하지 않는다.
     *
     * Gameplay State bool을 저장하는 것이 아니라
     * 현재 Mesh Physics 상태를 확인한다.
     */
    if (MeshComponent->IsSimulatingPhysics())
    {
        return;
    }

    /*
     * Ragdoll에는 Physics Asset이 필수.
     */
    if (!ensureMsgf(
            MeshComponent->GetPhysicsAsset() != nullptr,
            TEXT("[%s] Death ragdoll failed: Physics Asset is not assigned."),
            *GetNameSafe(OwnerCharacter)
        ))
    {
        /*
         * Physics Asset이 없어도
         * Dead Capsule은 이미 제거됐으므로
         * Gameplay 충돌에는 참여하지 않는다.
         */
        return;
    }

    /*
     * Corpse는 물리적으로 바닥과 충돌하지만
     * Gameplay Trace / 다른 Pawn을 막아서는 안 된다.
     *
     * 따라서 전체 Channel을 Ignore한 뒤
     * World Geometry만 Block한다.
     */
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    MeshComponent->SetCollisionObjectType(ECC_PhysicsBody);

    MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);

    MeshComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);

    MeshComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);

    /*
     * Visibility는 위의 AllChannels Ignore 상태라
     * 그대로 Ignore다.
     *
     * 따라서 Dead Ragdoll이
     * Combat LOS Trace를 막지 않는다.
     */

    MeshComponent->SetSimulatePhysics(true);

    MeshComponent->WakeAllRigidBodies();

    UE_LOG(LogDMJellyPhysics, Log, TEXT("[%s] Death ragdoll presentation started."), *GetNameSafe(OwnerCharacter));
}

#pragma endregion
