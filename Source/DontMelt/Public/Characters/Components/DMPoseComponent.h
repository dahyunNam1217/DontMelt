#pragma once

#include "CoreMinimal.h"
#include "Characters/DMCharacterTypes.h"
#include "Components/ActorComponent.h"
#include "DMPoseComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FDMOnPoseTypeChanged, EDMPoseType, EDMPoseType);

/**
 * 실제 자세 또는 자세 모드 승인 상태가 변경되면
 * 로컬 HUD가 현재 상태를 다시 읽도록 알린다.
 *
 * 기존 서버 퀘스트 이벤트 Delegate와는 별개다.
 */
DECLARE_MULTICAST_DELEGATE(FDMOnPosePresentationChanged);

UCLASS(ClassGroup = (DontMelt), meta = (BlueprintSpawnableComponent))
class DONTMELT_API UDMPoseComponent : public UActorComponent
{
    GENERATED_BODY()

public:
#pragma region Public API

    UDMPoseComponent();

    FDMOnPoseTypeChanged& OnPoseTypeChanged()
    {
        return PoseTypeChangedDelegate;
    }

    /** 자세 표시값이 변경되었을 때 UI에서 구독한다. */
    FDMOnPosePresentationChanged& OnPosePresentationChanged()
    {
        return PosePresentationChangedDelegate;
    }

    UFUNCTION(BlueprintPure, Category = "DontMelt|Pose")
    EDMPoseType GetPoseType() const;

    /**
     * Default가 아닌 실제 Pose가 적용 중인지 반환한다.
     *
     * Pose Input Mode 활성 여부와는 다르며,
     * Pose Mode에서도 PoseType은 Default일 수 있다.
     */
    UFUNCTION(BlueprintPure, Category = "DontMelt|Pose")
    bool IsPoseActive() const;

    UFUNCTION(BlueprintPure, Category = "DontMelt|Pose")
    bool IsPoseMoving() const;

    UFUNCTION(BlueprintPure, Category = "DontMelt|Pose")
    bool IsPoseLocked() const;

    UFUNCTION(BlueprintPure, Category = "DontMelt|Pose")
    bool CanEnterPose(EDMPoseType NewPose) const;

    UFUNCTION(BlueprintPure, Category = "DontMelt|Pose")
    bool IsActionAllowedByPose(EDMActionRequest ActionRequest) const;

#pragma endregion

#pragma region Requests

    UFUNCTION(Server, Reliable)
    void RequestSetPoseModeActive(bool bEnablePoseMode);

    UFUNCTION(Server, Reliable)
    void RequestCyclePose();

#pragma endregion

protected:
#pragma region Replication

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion

private:
#pragma region Replication Callbacks

    UFUNCTION()
    void OnRep_PoseType();

    UFUNCTION()
    void OnRep_PoseLocked();

#pragma endregion

#pragma region Internal Helpers

    void SetPoseAuthoritative(EDMPoseType NewPose);

    void ApplyPoseMovementRules();

    void ApplyRotationPolicy();

#pragma endregion

#pragma region Runtime State

    UPROPERTY(
        ReplicatedUsing = OnRep_PoseType,
        VisibleInstanceOnly,
        BlueprintReadOnly,
        Category = "DontMelt|Pose",
        meta = (AllowPrivateAccess = "true")
    )
    EDMPoseType PoseType = EDMPoseType::Default;

    UPROPERTY(
        ReplicatedUsing = OnRep_PoseLocked,
        VisibleInstanceOnly,
        BlueprintReadOnly,
        Category = "DontMelt|Pose",
        meta = (AllowPrivateAccess = "true")
    )
    bool bPoseLocked = false;

#pragma endregion

#pragma region Delegates

    FDMOnPoseTypeChanged PoseTypeChangedDelegate;

    /** 네트워크 양쪽에서 사용하는 로컬 표시 알림 */
    FDMOnPosePresentationChanged PosePresentationChangedDelegate;

#pragma endregion
};