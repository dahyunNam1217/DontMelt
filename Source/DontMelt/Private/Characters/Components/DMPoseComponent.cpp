#include "Characters/Components/DMPoseComponent.h"

#include "Characters/DMJellyCharacterBase.h"
#include "Characters/DMPlayerCharacter.h"
#include "Characters/Components/DMCharacterStateComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

#pragma region Lifecycle

UDMPoseComponent::UDMPoseComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    SetIsReplicatedByDefault(true);
}

#pragma endregion

#pragma region Queries

EDMPoseType UDMPoseComponent::GetPoseType() const
{
    return PoseType;
}

bool UDMPoseComponent::IsPoseActive() const
{
    return PoseType != EDMPoseType::Default;
}

bool UDMPoseComponent::IsPoseMoving() const
{
    if (!IsPoseActive())
    {
        return false;
    }

    const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());

    if (OwnerCharacter == nullptr)
    {
        return false;
    }

    const UCharacterMovementComponent* MovementComponent = OwnerCharacter->GetCharacterMovement();

    if (MovementComponent == nullptr || !MovementComponent->IsMovingOnGround())
    {
        return false;
    }

    FVector HorizontalVelocity = OwnerCharacter->GetVelocity();

    HorizontalVelocity.Z = 0.0f;

    return !HorizontalVelocity.IsNearlyZero();
}

bool UDMPoseComponent::IsPoseLocked() const
{
    return bPoseLocked;
}

bool UDMPoseComponent::CanEnterPose(const EDMPoseType NewPose) const
{
    if (PoseType == NewPose)
    {
        return false;
    }

    switch (NewPose)
    {
    case EDMPoseType::Default:
    case EDMPoseType::Flat:
    case EDMPoseType::Crouched:
    case EDMPoseType::Star:
        return true;

    default:
        return false;
    }
}

bool UDMPoseComponent::IsActionAllowedByPose(const EDMActionRequest ActionRequest) const
{
    if (!bPoseLocked)
    {
        return true;
    }

    switch (ActionRequest)
    {
    case EDMActionRequest::Move:
    case EDMActionRequest::Jump:
    case EDMActionRequest::PoseChange:
        return true;

    case EDMActionRequest::Sprint:
    case EDMActionRequest::Attack:
    case EDMActionRequest::Defense:
    case EDMActionRequest::Interact:
    case EDMActionRequest::Skill:
    case EDMActionRequest::Emote:
        return false;

    default:
        return false;
    }
}

#pragma endregion

#pragma region Requests

void UDMPoseComponent::RequestSetPoseModeActive_Implementation(const bool bEnablePoseMode)
{
    AActor* OwnerActor = GetOwner();

    if (OwnerActor == nullptr || !OwnerActor->HasAuthority())
    {
        return;
    }

    /*
     * Pose Mode 종료는 언제나 허용한다.
     *
     * 진입할 때만 CharacterState의 현재 Action/Status
     * 정책을 Server에서 검증한다.
     */
    if (bEnablePoseMode)
    {
        const ADMJellyCharacterBase* OwnerCharacter = Cast<ADMJellyCharacterBase>(OwnerActor);

        if (!IsValid(OwnerCharacter))
        {
            return;
        }

        const UDMCharacterStateComponent* StateComponent = OwnerCharacter->GetCharacterStateComponent();

        if (!IsValid(StateComponent) || !StateComponent->CanPerformAction(EDMActionRequest::PoseChange))
        {
            return;
        }
    }

    if (bPoseLocked == bEnablePoseMode)
    {
        return;
    }

    if (bEnablePoseMode)
    {
        bPoseLocked = true;

        SetPoseAuthoritative(EDMPoseType::Flat);

        ApplyPoseMovementRules();
    }
    else
    {
        /*
         * Pose Mode 종료 시 실제 Pose도
         * 반드시 Default로 복귀시킨다.
         */
        if (PoseType != EDMPoseType::Default)
        {
            SetPoseAuthoritative(EDMPoseType::Default);
        }

        bPoseLocked = false;
    }

    ApplyRotationPolicy();

    // RepNotify가 호출되지 않는 서버 인스턴스에도 알린다.
    PosePresentationChangedDelegate.Broadcast();
}

void UDMPoseComponent::RequestCyclePose_Implementation()
{
    const ADMJellyCharacterBase* OwnerCharacter = GetOwner<const ADMJellyCharacterBase>();

    if (!IsValid(OwnerCharacter) || !OwnerCharacter->HasAuthority() || !bPoseLocked)
    {
        return;
    }

    const UDMCharacterStateComponent* StateComponent = OwnerCharacter->GetCharacterStateComponent();

    if (!IsValid(StateComponent) || !StateComponent->CanPerformAction(EDMActionRequest::PoseChange))
    {
        return;
    }

    EDMPoseType NextPose = EDMPoseType::Default;

    switch (PoseType)
    {
    case EDMPoseType::Flat:
        NextPose = EDMPoseType::Crouched;
        break;

    case EDMPoseType::Crouched:
        NextPose = EDMPoseType::Star;
        break;

    case EDMPoseType::Star:
        NextPose = EDMPoseType::Flat;
        break;

    default:
        return;
    }

    SetPoseAuthoritative(NextPose);
}

#pragma endregion

#pragma region Replication

void UDMPoseComponent::OnRep_PoseType()
{
    ApplyPoseMovementRules();

    // 복제된 자세를 받은 클라이언트의 HUD만 갱신한다.
    PosePresentationChangedDelegate.Broadcast();
}

void UDMPoseComponent::OnRep_PoseLocked()
{
    if (bPoseLocked)
    {
        ApplyPoseMovementRules();
    }

    ApplyRotationPolicy();

    // 자세 모드 승인·해제 상태를 UI에 반영한다.
    PosePresentationChangedDelegate.Broadcast();
}

void UDMPoseComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(UDMPoseComponent, PoseType);

    DOREPLIFETIME(UDMPoseComponent, bPoseLocked);
}

#pragma endregion

#pragma region Internal Helpers

void UDMPoseComponent::SetPoseAuthoritative(const EDMPoseType NewPose)
{
    if (PoseType == NewPose)
    {
        return;
    }

    const EDMPoseType OldPose = PoseType;

    PoseType = NewPose;

    /*
     * OnRep does not execute on the authority instance.
     */
    ApplyPoseMovementRules();

    AActor* OwnerActor = GetOwner();

    if (OwnerActor != nullptr && OwnerActor->HasAuthority())
    {
        PoseTypeChangedDelegate.Broadcast(OldPose, PoseType);
    }

    // 실제 자세 변경을 표시 HUD에 전달한다.
    // 퀘스트 이벤트용 PoseTypeChangedDelegate는 변경하지 않는다.
    PosePresentationChangedDelegate.Broadcast();
}

void UDMPoseComponent::ApplyPoseMovementRules()
{
    if (!bPoseLocked)
    {
        return;
    }

    ADMJellyCharacterBase* OwnerCharacter = Cast<ADMJellyCharacterBase>(GetOwner());

    if (OwnerCharacter == nullptr)
    {
        return;
    }

    // Pose Mode에서는 Sprint 이동 의도를 유지하지 않는다.
    OwnerCharacter->ClearSprintRequest();
}

void UDMPoseComponent::ApplyRotationPolicy()
{
    ADMPlayerCharacter* OwnerPlayer = Cast<ADMPlayerCharacter>(GetOwner());

    if (OwnerPlayer == nullptr)
    {
        return;
    }

    /*
     * Camera View와 Pose Lock을 모두 알고 있는
     * PlayerCharacter가 최종 회전 정책을 결정한다.
     */
    OwnerPlayer->RefreshRotationPolicy();
}

#pragma endregion