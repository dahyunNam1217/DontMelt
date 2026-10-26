#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Characters/DMCharacterTypes.h"
#include "DMJellyAnimInstance.generated.h"

class ADMJellyCharacterBase;
class UDMPoseComponent;

/**
 * 젤리 표현의 튜닝값.
 *
 * Gameplay 규칙이 아닌 시각 표현용 수치이며,
 * Animation Blueprint의 Class Defaults에서 조정한다.
 */
USTRUCT(BlueprintType)
struct FDMJellyMotionTuning
{
    GENERATED_BODY()

    /** 몸통·머리·팔 감쇠 스프링의 고유 진동수(Hz). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spring", meta = (ClampMin = "0.1"))
    float SpringFrequency = 1.8f;

    /** 감쇠 비율. 1보다 작을수록 더 많이 출렁인다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spring", meta = (ClampMin = "0.0"))
    float SpringDampingRatio = 0.55f;

    /** 수치 안정성을 위한 스프링 적분 최대 시간 간격(초). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spring", meta = (ClampMin = "0.001"))
    float SpringMaxStepSeconds = 1.0f / 120.0f;

    /** 관성 최대 반응으로 보는 수평 가속도(cm/s²). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inertia", meta = (ClampMin = "1.0"))
    float ReferenceAcceleration = 2000.0f;

    /** 가속도 관성값을 따라가는 보간 속도. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inertia", meta = (ClampMin = "0.0"))
    float InertiaInterpSpeed = 8.0f;

    /** 전방 가속 시 뒤로 젖혀지는 최대 각도. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inertia")
    float AccelerationLeanAngle = 8.0f;

    /** 급정지 시 앞으로 쏠리는 최대 각도. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inertia")
    float DecelerationLeanAngle = 15.0f;

    /** 좌우 가속 시 반대 방향으로 기우는 최대 각도. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inertia")
    float SideLeanAngle = 11.0f;

    /** 걷기 속도에서의 전방 기울기 각도. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion")
    float WalkForwardLean = 5.0f;

    /** 달리기 속도에서의 전방 기울기 각도. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion")
    float SprintForwardLean = 12.0f;

    /** Idle 흔들림 한 주기의 길이(초). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion", meta = (ClampMin = "0.1"))
    float IdlePeriodSeconds = 1.8f;

    /** 걷기 중 보행 흔들림 주파수(Hz). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion", meta = (ClampMin = "0.0"))
    float WalkGaitFrequency = 1.5f;

    /** 달리기 중 보행 흔들림 주파수(Hz). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion", meta = (ClampMin = "0.0"))
    float SprintGaitFrequency = 2.7f;

    /** 걷기 중 몸통 좌우 흔들림 각도. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion")
    float WalkGaitRoll = 4.0f;

    /** 달리기 중 몸통 좌우 흔들림 각도. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion")
    float SprintGaitRoll = 5.0f;

    /** 이동 중 회전할 때 상체가 늦게 따라오는 최대 각도. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion")
    float TurnLagAngle = 10.0f;

    /** 회전 지연이 최대가 되는 회전 속도(deg/s). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion", meta = (ClampMin = "1.0"))
    float TurnLagReferenceRate = 540.0f;

    /** 걷기 중 양팔 교대 흔들림 각도. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion")
    float WalkArmSwing = 5.0f;

    /** 달리기 중 양팔 교대 흔들림 각도. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion")
    float SprintArmSwing = 10.0f;

    /** 착지 반동을 재생하기 위한 최소 체공 시간(초). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landing", meta = (ClampMin = "0.0"))
    float LandingMinAirborneSeconds = 0.12f;

    /** 착지 반동을 재생하기 위한 최소 낙하 속도(cm/s). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landing", meta = (ClampMin = "0.0"))
    float LandingMinImpactSpeed = 240.0f;

    /** 착지 반동이 최대가 되는 낙하 속도(cm/s). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landing", meta = (ClampMin = "0.0"))
    float LandingMaxImpactSpeed = 900.0f;

    /** 최소 착지 압축량. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landing")
    float LandingMinAmplitude = 4.0f;

    /** 최대 착지 압축량. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landing")
    float LandingMaxAmplitude = 9.0f;

    /** 착지 압축·반동 재생 시간(초). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landing", meta = (ClampMin = "0.0"))
    float LandingDuration = 0.55f;

    /** 착지 반동 진동수(Hz). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landing", meta = (ClampMin = "0.0"))
    float LandingFrequency = 2.5f;

    /** 착지 반동 감쇠 속도. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landing", meta = (ClampMin = "0.0"))
    float LandingDecay = 3.8f;

    /** 몸통 Pitch 최대 각도. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limits", meta = (ClampMin = "0.0"))
    float BodyMaxPitch = 20.0f;

    /** 몸통 Roll 최대 각도. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limits", meta = (ClampMin = "0.0"))
    float BodyMaxRoll = 17.0f;

    /** 몸통 Yaw 최대 각도. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limits", meta = (ClampMin = "0.0"))
    float BodyMaxYaw = 10.0f;

    /** 머리 Pitch·Roll 최대 각도. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limits", meta = (ClampMin = "0.0"))
    float HeadMaxAngle = 7.0f;

    /** 팔 공통 관성 Pitch·Roll 최대 각도. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limits", meta = (ClampMin = "0.0"))
    float ArmMaxAngle = 12.0f;
};

/**
 * Jelly Character 공통 Animation Instance.
 *
 * Gameplay 상태를 직접 소유하지 않고
 * Character와 Component의 원본 데이터를 읽어
 * Animation Blueprint용 표현값만 계산한다.
 */
UCLASS()
class DONTMELT_API UDMJellyAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    virtual void NativeInitializeAnimation() override;

    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
    /** 젤리 흔들림·관성·착지 반동 튜닝값. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DontMelt|Animation|Jelly")
    FDMJellyMotionTuning JellyTuning;

    /**
     * Z축을 제외한 수평 이동 속도.
     */
    UPROPERTY(BlueprintReadOnly, Category = "DontMelt|Animation")
    float GroundSpeed = 0.0f;

    /**
     * 기본 이동 애니메이션 혼합값.
     *
     * 0.0 = Idle
     * 0.5 = Walk
     * 1.0 = Sprint
     *
     * 캐릭터별 WalkSpeed/SprintSpeed를 기준으로
     * 실제 수평 속도를 정규화한 표현 데이터다.
     */
    UPROPERTY(BlueprintReadOnly, Category = "DontMelt|Animation")
    float NormalizedLocomotionSpeed = 0.0f;

    /**
     * CharacterMovement가 Falling 상태인지 여부.
     */
    UPROPERTY(BlueprintReadOnly, Category = "DontMelt|Animation")
    bool bIsFalling = false;

    /**
     * 수평 이동 중인지 나타내는 Animation 표현값.
     *
     * Gameplay State가 아니며 Velocity에서 계산한다.
     */
    UPROPERTY(BlueprintReadOnly, Category = "DontMelt|Animation")
    bool bShouldMove = false;

    /**
     * Sprint Animation 표현값.
     *
     * Authority와 Owning Client는 CharacterMovement의
     * Sprint 요청 상태를 사용하고,
     * Simulated Proxy는 복제된 실제 이동 속도에서 계산한다.
     */
    UPROPERTY(BlueprintReadOnly, Category = "DontMelt|Animation")
    bool bIsSprinting = false;

    /**
     * 현재 Pose 표현값.
     *
     * 원본 상태는 UDMPoseComponent가 소유한다.
     */
    UPROPERTY(BlueprintReadOnly, Category = "DontMelt|Animation")
    EDMPoseType PoseType = EDMPoseType::Default;

    /**
     * Pose 상태에서 실제 이동 중인지 나타내는 표현값.
     *
     * 별도 Gameplay State를 저장하지 않고
     * PoseComponent가 현재 상태에서 계산한다.
     */
    UPROPERTY(BlueprintReadOnly, Category = "DontMelt|Animation")
    bool bIsPoseMoving = false;

    // 젤리 움직임의 속도 기반 강도.
    // 기존 SprintSpeed를 기준으로 0~1 사이로 정규화한다.
    UPROPERTY(BlueprintReadOnly, Category = "DontMelt|Animation|Jelly")
    float JellySpeedAlpha = 0.0f;

    // 전후 방향 가속·감속으로부터 계산한 시각적 관성.
    UPROPERTY(BlueprintReadOnly, Category = "DontMelt|Animation|Jelly")
    float JellyForwardInertia = 0.0f;

    // 좌우 방향 가속·감속으로부터 계산한 시각적 관성.
    UPROPERTY(BlueprintReadOnly, Category = "DontMelt|Animation|Jelly")
    float JellySideInertia = 0.0f;

    // 일반 젤리 애니메이션의 최종 적용 가중치.
    UPROPERTY(BlueprintReadOnly, Category = "DontMelt|Animation|Jelly")
    float JellyPresentationAlpha = 0.0f;

    // Spine1에 적용할 추가 회전값.
    UPROPERTY(BlueprintReadOnly, Category = "DontMelt|Animation|Jelly")
    FRotator JellySpine1Rotation = FRotator::ZeroRotator;

    // Spine2에 적용할 추가 회전값.
    UPROPERTY(BlueprintReadOnly, Category = "DontMelt|Animation|Jelly")
    FRotator JellySpine2Rotation = FRotator::ZeroRotator;

    // Ribcage에 적용할 추가 회전값.
    UPROPERTY(BlueprintReadOnly, Category = "DontMelt|Animation|Jelly")
    FRotator JellyRibcageRotation = FRotator::ZeroRotator;

    // 착지 충격으로 발생하는 흉부 압축 및 반동.
    // Gameplay 위치나 충돌 캡슐에는 영향을 주지 않는다.
    UPROPERTY(BlueprintReadOnly, Category = "DontMelt|Animation|Jelly")
    FVector JellyLandingRibcageOffset = FVector::ZeroVector;

    // 머리의 추가 회전값.
    UPROPERTY(BlueprintReadOnly, Category = "DontMelt|Animation|Jelly")
    FRotator JellyHeadRotation = FRotator::ZeroRotator;

    // 왼팔 위쪽 관절의 추가 회전값.
    UPROPERTY(BlueprintReadOnly, Category = "DontMelt|Animation|Jelly")
    FRotator JellyLeftUpperarmRotation = FRotator::ZeroRotator;

    // 오른팔 위쪽 관절의 추가 회전값.
    UPROPERTY(BlueprintReadOnly, Category = "DontMelt|Animation|Jelly")
    FRotator JellyRightUpperarmRotation = FRotator::ZeroRotator;

    // 왼쪽 아래팔의 추가 회전값.
    UPROPERTY(BlueprintReadOnly, Category = "DontMelt|Animation|Jelly")
    FRotator JellyLeftForearmRotation = FRotator::ZeroRotator;

    // 오른쪽 아래팔의 추가 회전값.
    UPROPERTY(BlueprintReadOnly, Category = "DontMelt|Animation|Jelly")
    FRotator JellyRightForearmRotation = FRotator::ZeroRotator;

private:
    // 원본 이동값에서 젤리 애니메이션 표현값만 계산한다.
    void UpdateJellyPresentation(const FVector& HorizontalVelocity, float DeltaSeconds);

    // 관성 계산에 필요한 이전 프레임 속도.
    // Gameplay 상태가 아닌 AnimInstance 내부의 임시 계산값이다.
    FVector PreviousHorizontalVelocity = FVector::ZeroVector;

    bool bHasPreviousHorizontalVelocity = false;

    // 네트워크 위치 보정이나 순간이동 여부를
    // 확인하기 위한 이전 애니메이션 갱신 위치.
    FVector PreviousJellyWorldLocation = FVector::ZeroVector;

    bool bHasPreviousJellyWorldLocation = false;

    // Idle과 보행 흔들림의 현재 위상.
    float JellyIdlePhase = 0.0f;
    float JellyGaitPhase = 0.0f;

    // 실제 몸통 Spring의 현재 각도.
    float JellyBodyPitch = 0.0f;
    float JellyBodyRoll = 0.0f;
    float JellyBodyYaw = 0.0f;

    // 실제 몸통 Spring의 각속도.
    float JellyPitchVelocity = 0.0f;
    float JellyRollVelocity = 0.0f;
    float JellyYawVelocity = 0.0f;

    // 이동 중 방향 전환 속도 계산용 이전 방향.
    float PreviousFacingYaw = 0.0f;
    bool bHasPreviousFacingYaw = false;

    // Idle, Walk, Sprint, 관성 및 Spring을 통합 계산한다.
    void UpdateJellyBodyMotion(const FVector& HorizontalVelocity, float DeltaSeconds);

    // 사망, 초기화, Owner 변경 시 잔여 관성을 제거한다.
    void ResetJellyBodyMotion();

    // 직전 애니메이션 갱신에서 공중 상태였는지 기록한다.
    bool bWasJellyFalling = false;

    // 현재 연속된 공중 체류시간.
    float JellyAirborneSeconds = 0.0f;

    // 착지 직전의 수직 속도를 보존한다.
    float PreviousAirVerticalSpeed = 0.0f;

    // 착지 후 경과시간. 1초는 비활성 상태를 의미한다.
    float JellyLandingElapsed = 1.0f;

    // 이번 착지 충격으로 발생한 최대 흔들림 크기.
    float JellyLandingAmplitude = 0.0f;

    // 현재 착지에 의한 흉부 이동량.
    float JellyLandingCompression = 0.0f;

    // 기존 공중 상태와 수직 속도로부터 시각적 반동을 계산한다.
    void UpdateJellyAirMotion(float DeltaSeconds);

    // 머리의 독립적인 보조 스프링 상태.
    float JellyHeadPitch = 0.0f;
    float JellyHeadRoll = 0.0f;

    float JellyHeadPitchVelocity = 0.0f;
    float JellyHeadRollVelocity = 0.0f;

    // 양팔의 공통 관성 스프링 상태.
    // 좌우 보행 흔들림은 별도로 분리한다.
    float JellyArmPitch = 0.0f;
    float JellyArmRoll = 0.0f;

    float JellyArmPitchVelocity = 0.0f;
    float JellyArmRollVelocity = 0.0f;

    // 머리 및 양팔의 보조 관성 계산.
    void UpdateJellySecondaryMotion(float DeltaSeconds);

    // 이전 프레임의 보조 물리 상태 초기화.
    void ResetJellySecondaryMotion();

    // 각도와 각속도를 사용하는 감쇠 스프링 계산.
    void AdvanceJellySpring(
        float TargetAngle, float DeltaSeconds, float MaxAbsAngle, float& InOutAngle, float& InOutVelocity
    ) const;

    void CacheOwnerReferences();

    UPROPERTY(Transient)
    TObjectPtr<ADMJellyCharacterBase> CachedCharacter;

    UPROPERTY(Transient)
    TObjectPtr<UDMPoseComponent> CachedPoseComponent;
};