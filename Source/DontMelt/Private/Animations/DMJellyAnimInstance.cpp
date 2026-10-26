#include "Animations/DMJellyAnimInstance.h"

#include "Characters/Components/DMCharacterMovementComponent.h"
#include "Characters/Components/DMJellyPhysicsComponent.h"
#include "Characters/Components/DMPoseComponent.h"
#include "Characters/Components/DMCharacterStateComponent.h"
#include "Characters/DMJellyCharacterBase.h"

#include "Framework/Match/DMMatchPlayerState.h"

#include "Core/DMGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"

void UDMJellyAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();

    CacheOwnerReferences();

    // 애니메이션 재초기화 시 이전 이동 혼합값을 제거한다.
    NormalizedLocomotionSpeed = 0.0f;

    PreviousHorizontalVelocity = FVector::ZeroVector;
    bHasPreviousHorizontalVelocity = false;

    JellySpeedAlpha = 0.0f;
    JellyForwardInertia = 0.0f;
    JellySideInertia = 0.0f;
    JellyPresentationAlpha = 0.0f;

    // 애니메이션 재초기화 시 이전 흔들림을 제거한다.
    ResetJellyBodyMotion();
}

void UDMJellyAnimInstance::NativeUpdateAnimation(const float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    /*
     * Animation Preview 또는 초기화 순서에 따라
     * 처음에는 Owner가 없을 수 있으므로 유효하지 않을 때만 다시 캐시한다.
     */
    if (!IsValid(CachedCharacter))
    {
        CacheOwnerReferences();
    }

    if (!IsValid(CachedCharacter))
    {
        GroundSpeed = 0.0f;
        NormalizedLocomotionSpeed = 0.0f;
        bIsFalling = false;
        bShouldMove = false;
        bIsSprinting = false;

        PoseType = EDMPoseType::Default;

        bIsPoseMoving = false;

        // Owner가 사라지면 이전 관성값을 보존하지 않는다.
        JellySpeedAlpha = 0.0f;
        JellyForwardInertia = 0.0f;
        JellySideInertia = 0.0f;
        JellyPresentationAlpha = 0.0f;

        PreviousHorizontalVelocity = FVector::ZeroVector;
        bHasPreviousHorizontalVelocity = false;

        // 캐릭터 참조가 유효하지 않으면 몸통 상태도 초기화한다.
        ResetJellyBodyMotion();

        return;
    }

    FVector HorizontalVelocity = CachedCharacter->GetVelocity();

    HorizontalVelocity.Z = 0.0f;

    GroundSpeed = HorizontalVelocity.Size();

    // 플레이어와 NPC가 가진 실제 이동 속도 설정을 사용한다.
    const float WalkReferenceSpeed = FMath::Max(CachedCharacter->GetWalkSpeed(), 1.0f);

    const float SprintReferenceSpeed = FMath::Max(CachedCharacter->GetSprintSpeed(), WalkReferenceSpeed + 1.0f);

    if (GroundSpeed <= WalkReferenceSpeed)
    {
        // Idle(0.0)부터 Walk(0.5)까지 연속적으로 혼합한다.
        NormalizedLocomotionSpeed = 0.5f * FMath::Clamp(GroundSpeed / WalkReferenceSpeed, 0.0f, 1.0f);
    }
    else
    {
        // Walk(0.5)부터 Sprint(1.0)까지 연속적으로 혼합한다.
        NormalizedLocomotionSpeed = 0.5f
            + 0.5f
                * FMath::Clamp(
                    (GroundSpeed - WalkReferenceSpeed) / (SprintReferenceSpeed - WalkReferenceSpeed), 0.0f, 1.0f
                );
    }

    const UCharacterMovementComponent* MovementComponent = CachedCharacter->GetCharacterMovement();

    bIsFalling = MovementComponent != nullptr && MovementComponent->IsFalling();

    /*
     * 아주 작은 네트워크/물리 잔여 속도로
     * Idle과 Move 표현이 흔들리지 않도록 최소 오차를 둔다.
     */
    bShouldMove = !FMath::IsNearlyZero(GroundSpeed, 1.0f);

    if (IsValid(CachedPoseComponent))
    {
        PoseType = CachedPoseComponent->GetPoseType();

        bIsPoseMoving = CachedPoseComponent->IsPoseMoving();
    }
    else
    {
        PoseType = EDMPoseType::Default;

        bIsPoseMoving = false;
    }

    const UDMCharacterMovementComponent* DMMovementComponent = Cast<UDMCharacterMovementComponent>(MovementComponent);

    bool bSprintPresentation = false;

    if (DMMovementComponent != nullptr)
    {
        if (CachedCharacter->IsLocallyControlled() || CachedCharacter->HasAuthority())
        {
            /*
             * Owning Client는 Prediction에 사용되는 Sprint 의도를,
             * Authority는 SavedMove에서 복원된 Sprint 의도를 사용한다.
             */
            bSprintPresentation = DMMovementComponent->IsSprintRequested();
        }
        else
        {
            /*
             * Simulated Proxy에는 Sprint Custom Flag를
             * 별도 Replication하지 않으므로 실제 복제 속도에서 표현을 계산한다.
             */
            bSprintPresentation = GroundSpeed > CachedCharacter->GetWalkSpeed() + 1.0f;
        }
    }

    bIsSprinting = bShouldMove && !bIsFalling && PoseType == EDMPoseType::Default && bSprintPresentation;

    // --------------------------------------------------
    // 큰 위치 불연속 및 프레임 지연 감지
    // --------------------------------------------------

    const FVector CurrentWorldLocation = CachedCharacter->GetActorLocation();

    bool bResetForDiscontinuity = false;

    if (bHasPreviousJellyWorldLocation)
    {
        // 점프·낙하 중 정상적인 수직 이동을
        // 순간이동으로 잘못 판단하지 않도록 한다.
        const float CurrentSpeed = CachedCharacter->GetVelocity().Size();

        const float ReferenceSpeed = FMath::Max(CurrentSpeed, PreviousHorizontalVelocity.Size());

        const float ExpectedTravelDistance = ReferenceSpeed * FMath::Clamp(DeltaSeconds, 0.0f, 0.25f);

        // 200cm는 초기 테스트용 위치 불연속 허용 거리다.
        // 정상 이동으로 예상되는 거리에는 추가 여유를 둔다.
        const float AllowedDistance = FMath::Max(200.0f, ExpectedTravelDistance * 2.5f + 80.0f);

        const double ActualDistanceSquared = FVector::DistSquared(CurrentWorldLocation, PreviousJellyWorldLocation);

        const bool bLargePositionChange = ActualDistanceSquared > FMath::Square(static_cast<double>(AllowedDistance));

        // 긴 갱신 중단 시에는 오래된 Spring 상태를
        // 새 위치에서 계속 사용하지 않는다.
        const bool bInvalidUpdateInterval = DeltaSeconds > 0.25f;

        bResetForDiscontinuity = bLargePositionChange || bInvalidUpdateInterval;
    }

    if (bResetForDiscontinuity)
    {
        // Gameplay 이동에는 손대지 않고
        // 시각적 관성 상태만 초기화한다.
        ResetJellyBodyMotion();

        JellyForwardInertia = 0.0f;
        JellySideInertia = 0.0f;

        PreviousHorizontalVelocity = HorizontalVelocity;

        bHasPreviousHorizontalVelocity = false;
    }

    // 초기화가 발생한 프레임에도
    // 현재 위치를 새로운 비교 기준으로 저장한다.
    PreviousJellyWorldLocation = CurrentWorldLocation;
    bHasPreviousJellyWorldLocation = true;

    // 위치 불연속을 감지한 프레임에는
    // 이전 시간 간격으로 Spring을 적분하지 않는다.
    const float JellyUpdateDeltaSeconds = bResetForDiscontinuity ? 0.0f : DeltaSeconds;

    UpdateJellyPresentation(HorizontalVelocity, JellyUpdateDeltaSeconds);

    UpdateJellyBodyMotion(HorizontalVelocity, JellyUpdateDeltaSeconds);
}

void UDMJellyAnimInstance::UpdateJellyPresentation(const FVector& HorizontalVelocity, const float DeltaSeconds)
{
    if (!IsValid(CachedCharacter))
    {
        return;
    }

    // 최고 달리기 속도를 기준으로 정규화한다.
    const float ReferenceSpeed = FMath::Max(CachedCharacter->GetSprintSpeed(), 1.0f);

    JellySpeedAlpha = FMath::Clamp(GroundSpeed / ReferenceSpeed, 0.0f, 1.0f);

    // 비정상적으로 긴 애니메이션 갱신 간격에서는
    // 잘못된 가속도 계산을 방지한다.
    const bool bValidDeltaTime = DeltaSeconds > KINDA_SMALL_NUMBER && DeltaSeconds <= 0.25f;

    if (!bHasPreviousHorizontalVelocity || !bValidDeltaTime)
    {
        PreviousHorizontalVelocity = HorizontalVelocity;
        bHasPreviousHorizontalVelocity = true;

        JellyForwardInertia = 0.0f;
        JellySideInertia = 0.0f;
    }
    else
    {
        // 수평 속도 변화로부터 시각적 가속도를 구한다.
        const FVector Acceleration = (HorizontalVelocity - PreviousHorizontalVelocity) / DeltaSeconds;

        PreviousHorizontalVelocity = HorizontalVelocity;

        // 캐릭터가 바라보는 방향을 기준으로 성분을 분리한다.
        const FVector Forward = CachedCharacter->GetActorForwardVector();

        const FVector Right = CachedCharacter->GetActorRightVector();

        const float ReferenceAcceleration = JellyTuning.ReferenceAcceleration;

        const float TargetForward =
            FMath::Clamp(FVector::DotProduct(Acceleration, Forward) / ReferenceAcceleration, -1.0f, 1.0f);

        const float TargetSide =
            FMath::Clamp(FVector::DotProduct(Acceleration, Right) / ReferenceAcceleration, -1.0f, 1.0f);

        // 급격한 속도 변화와 네트워크 보정의 영향을 줄인다.
        const float InterpSpeed = JellyTuning.InertiaInterpSpeed;

        JellyForwardInertia = FMath::FInterpTo(JellyForwardInertia, TargetForward, DeltaSeconds, InterpSpeed);

        JellySideInertia = FMath::FInterpTo(JellySideInertia, TargetSide, DeltaSeconds, InterpSpeed);
    }

    // --------------------------------------------------
    // Gameplay 상태를 기반으로 표현 가중치를 계산한다.
    // --------------------------------------------------

    const UDMJellyPhysicsComponent* PhysicsComponent = CachedCharacter->GetJellyPhysicsComponent();

    const bool bDeathPresentation = PhysicsComponent != nullptr && PhysicsComponent->IsDeathPresentationActive();

    // LifeState는 PlayerState가 원본을 소유한다.
    // NPC에는 MatchPlayerState가 없으므로 적용하지 않는다.
    const ADMMatchPlayerState* MatchPlayerState = Cast<ADMMatchPlayerState>(CachedCharacter->GetPlayerState());

    const bool bPlayerNotAlive =
        MatchPlayerState != nullptr && MatchPlayerState->GetLifeState() != EDMPlayerLifeState::Alive;

    // ActionState와 Status는 기존 StateComponent에서 읽는다.
    const UDMCharacterStateComponent* StateComponent = CachedCharacter->GetCharacterStateComponent();

    float TargetAlpha = 1.0f;
    bool bStunned = false;

    if (StateComponent != nullptr)
    {
        // 실제 Gameplay Status가 아닌,
        // 모든 관찰자에게 복제되는 스턴 표현값을 읽는다.
        bStunned = StateComponent->IsStunPresentationActive();
        // 스턴 중에는 강한 몸통 흔들림을 제한한다.
        if (bStunned)
        {
            TargetAlpha = 0.20f;
        }

        switch (StateComponent->GetActionState())
        {
        case EDMActionState::Attack:
            TargetAlpha = FMath::Min(TargetAlpha, 0.25f);
            break;

        case EDMActionState::Defense:
            TargetAlpha = FMath::Min(TargetAlpha, 0.15f);
            break;

        case EDMActionState::Interact:
        case EDMActionState::Emote:
        case EDMActionState::FakeDeath:
        case EDMActionState::SkillCast:
            TargetAlpha = 0.0f;
            break;

        case EDMActionState::None:
        default:
            break;
        }
    }

    // 기존 특수 자세 애니메이션을 우선한다.
    if (PoseType != EDMPoseType::Default)
    {
        TargetAlpha = 0.0f;
    }

    const bool bMontagePlaying = IsAnyMontagePlaying();

    // 실제 사망 또는 비생존 상태에서는 즉시 차단한다.
    if (bDeathPresentation || bPlayerNotAlive)
    {
        JellyPresentationAlpha = 0.0f;

        JellyForwardInertia = 0.0f;
        JellySideInertia = 0.0f;

        PreviousHorizontalVelocity = FVector::ZeroVector;
        bHasPreviousHorizontalVelocity = false;

        return;
    }

    // 몽타주가 재생 중이면 본 변형을 즉시 차단한다.
    if (bMontagePlaying)
    {
        JellyPresentationAlpha = 0.0f;
        return;
    }

    // 스턴 시작 시 Velocity 급변으로 인해
    // 불필요한 급정지 반동이 발생하지 않게 한다.
    if (bStunned)
    {
        JellyForwardInertia = 0.0f;
        JellySideInertia = 0.0f;
        bHasPreviousHorizontalVelocity = false;
    }

    // 특수 자세 및 상태 전환은 부드럽게 처리한다.
    JellyPresentationAlpha =
        FMath::FInterpTo(JellyPresentationAlpha, TargetAlpha, FMath::Max(DeltaSeconds, 0.0f), 10.0f);
}

void UDMJellyAnimInstance::AdvanceJellySpring(
    const float TargetAngle, const float DeltaSeconds, const float MaxAbsAngle, float& InOutAngle, float& InOutVelocity
) const
{
    if (DeltaSeconds <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    // 긴 프레임 중단 이후에는 오래된 각속도를 사용하지 않는다.
    if (DeltaSeconds > 0.10f)
    {
        InOutAngle = FMath::Clamp(TargetAngle, -MaxAbsAngle, MaxAbsAngle);

        InOutVelocity = 0.0f;
        return;
    }

    const float SpringFrequency = JellyTuning.SpringFrequency;
    const float DampingRatio = JellyTuning.SpringDampingRatio;

    const float AngularFrequency = 2.0f * PI * SpringFrequency;

    const float Stiffness = AngularFrequency * AngularFrequency;

    const float Damping = 2.0f * DampingRatio * AngularFrequency;

    // 높은 강성에서 프레임 간격이 커질 때 발생하는
    // 수치 불안정을 줄이기 위해 작은 시간 단계로 나눈다.
    const float MaxStepSeconds = FMath::Max(JellyTuning.SpringMaxStepSeconds, KINDA_SMALL_NUMBER);

    const int32 StepCount = FMath::Clamp(FMath::CeilToInt(DeltaSeconds / MaxStepSeconds), 1, 12);

    const float StepSeconds = DeltaSeconds / static_cast<float>(StepCount);

    for (int32 StepIndex = 0; StepIndex < StepCount; ++StepIndex)
    {
        // 목표 각도와의 차이를 이용해 복원 가속도를 구한다.
        const float SpringAcceleration = Stiffness * (TargetAngle - InOutAngle) - Damping * InOutVelocity;

        // Semi-implicit Euler 방식으로 적분한다.
        InOutVelocity += SpringAcceleration * StepSeconds;

        // 비정상적으로 큰 각속도를 제한한다.
        InOutVelocity = FMath::Clamp(InOutVelocity, -360.0f, 360.0f);

        InOutAngle += InOutVelocity * StepSeconds;

        // 최종 관절 회전 한계를 초과하지 않도록 보호한다.
        if (InOutAngle > MaxAbsAngle)
        {
            InOutAngle = MaxAbsAngle;

            if (InOutVelocity > 0.0f)
            {
                InOutVelocity = 0.0f;
            }
        }
        else if (InOutAngle < -MaxAbsAngle)
        {
            InOutAngle = -MaxAbsAngle;

            if (InOutVelocity < 0.0f)
            {
                InOutVelocity = 0.0f;
            }
        }
    }
}

void UDMJellyAnimInstance::ResetJellyBodyMotion()
{
    JellyIdlePhase = 0.0f;
    JellyGaitPhase = 0.0f;

    JellyBodyPitch = 0.0f;
    JellyBodyRoll = 0.0f;
    JellyBodyYaw = 0.0f;

    JellyPitchVelocity = 0.0f;
    JellyRollVelocity = 0.0f;
    JellyYawVelocity = 0.0f;

    PreviousFacingYaw = 0.0f;
    bHasPreviousFacingYaw = false;

    JellySpine1Rotation = FRotator::ZeroRotator;
    JellySpine2Rotation = FRotator::ZeroRotator;
    JellyRibcageRotation = FRotator::ZeroRotator;

    // 공중·착지 연출에 사용한 이전 상태를 제거한다.
    bWasJellyFalling = false;
    JellyAirborneSeconds = 0.0f;
    PreviousAirVerticalSpeed = 0.0f;

    JellyLandingElapsed = 1.0f;
    JellyLandingAmplitude = 0.0f;
    JellyLandingCompression = 0.0f;

    JellyLandingRibcageOffset = FVector::ZeroVector;

    // 머리와 양팔의 이전 스프링 상태도 함께 제거한다.
    ResetJellySecondaryMotion();

    // 새로운 위치에서 이전 프레임과 비교를 다시 시작한다.
    PreviousJellyWorldLocation = FVector::ZeroVector;
    bHasPreviousJellyWorldLocation = false;
}

void UDMJellyAnimInstance::ResetJellySecondaryMotion()
{
    // 머리의 회전 및 각속도 초기화.
    JellyHeadPitch = 0.0f;
    JellyHeadRoll = 0.0f;

    JellyHeadPitchVelocity = 0.0f;
    JellyHeadRollVelocity = 0.0f;

    // 양팔의 공통 관성 초기화.
    JellyArmPitch = 0.0f;
    JellyArmRoll = 0.0f;

    JellyArmPitchVelocity = 0.0f;
    JellyArmRollVelocity = 0.0f;

    // AnimGraph에 전달하는 출력값 초기화.
    JellyHeadRotation = FRotator::ZeroRotator;

    JellyLeftUpperarmRotation = FRotator::ZeroRotator;
    JellyRightUpperarmRotation = FRotator::ZeroRotator;

    JellyLeftForearmRotation = FRotator::ZeroRotator;
    JellyRightForearmRotation = FRotator::ZeroRotator;
}

void UDMJellyAnimInstance::UpdateJellyAirMotion(const float DeltaSeconds)
{
    if (!IsValid(CachedCharacter))
    {
        return;
    }

    // 애니메이션 갱신 중단이나 큰 프레임 지연으로 인한
    // 잘못된 착지 충격 계산을 방지한다.
    const bool bValidDeltaTime = DeltaSeconds > KINDA_SMALL_NUMBER && DeltaSeconds <= 0.10f;

    const float SafeDeltaSeconds = FMath::Clamp(DeltaSeconds, 0.0f, 0.05f);

    const float VerticalSpeed = CachedCharacter->GetVelocity().Z;

    // --------------------------------------------------
    // 공중 진입 및 공중 상태 갱신
    // --------------------------------------------------

    if (bIsFalling)
    {
        if (!bWasJellyFalling)
        {
            // 새로운 공중 구간이 시작되면
            // 이전 착지 반동을 종료한다.
            JellyAirborneSeconds = 0.0f;
            JellyLandingElapsed = 1.0f;
            JellyLandingAmplitude = 0.0f;
            JellyLandingCompression = 0.0f;
        }

        if (bValidDeltaTime)
        {
            JellyAirborneSeconds += SafeDeltaSeconds;
        }

        // 착지 시 참조할 마지막 공중 수직 속도.
        PreviousAirVerticalSpeed = VerticalSpeed;
    }
    // --------------------------------------------------
    // 공중에서 지상으로 전환된 순간
    // --------------------------------------------------
    else if (bWasJellyFalling)
    {
        const float ImpactSpeed = FMath::Max(-PreviousAirVerticalSpeed, 0.0f);

        const float MinAirborneSeconds = JellyTuning.LandingMinAirborneSeconds;
        const float MinImpactSpeed = JellyTuning.LandingMinImpactSpeed;

        const bool bValidLanding =
            bValidDeltaTime && JellyAirborneSeconds >= MinAirborneSeconds && ImpactSpeed >= MinImpactSpeed;

        if (bValidLanding)
        {
            // 충격이 클수록 상체가 더 깊게 눌리도록 한다.
            // 상한을 두어 심한 낙하에서도 메시 변형이
            // 과도하게 커지지 않도록 제한한다.
            JellyLandingAmplitude = FMath::GetMappedRangeValueClamped(
                FVector2D(MinImpactSpeed, JellyTuning.LandingMaxImpactSpeed),
                FVector2D(JellyTuning.LandingMinAmplitude, JellyTuning.LandingMaxAmplitude),
                ImpactSpeed
            );

            JellyLandingElapsed = 0.0f;
        }

        JellyAirborneSeconds = 0.0f;
        PreviousAirVerticalSpeed = 0.0f;
    }

    bWasJellyFalling = bIsFalling;

    // --------------------------------------------------
    // 착지 후 압축과 반동 재생
    // --------------------------------------------------

    JellyLandingCompression = 0.0f;

    const float LandingDuration = JellyTuning.LandingDuration;

    if (!bIsFalling && JellyLandingAmplitude > 0.0f && JellyLandingElapsed < LandingDuration)
    {
        JellyLandingElapsed += SafeDeltaSeconds;

        const float LandingFrequency = JellyTuning.LandingFrequency;
        const float LandingDecay = JellyTuning.LandingDecay;

        // 처음에는 아래로 눌리고, 이후 반대 방향으로
        // 작게 반동하며 점차 안정화된다.
        const float Oscillation = FMath::Sin(2.0f * PI * LandingFrequency * JellyLandingElapsed);

        const float Envelope = FMath::Exp(-LandingDecay * JellyLandingElapsed);

        JellyLandingCompression = -JellyLandingAmplitude * Envelope * Oscillation;

        if (JellyLandingElapsed >= LandingDuration)
        {
            JellyLandingCompression = 0.0f;
            JellyLandingAmplitude = 0.0f;
        }
    }
}

void UDMJellyAnimInstance::UpdateJellyBodyMotion(const FVector& HorizontalVelocity, const float DeltaSeconds)
{
    if (!IsValid(CachedCharacter))
    {
        ResetJellyBodyMotion();
        return;
    }

    // 기존 Death Ragdoll이 활성화된 상태에서는
    // 일반 젤리 애니메이션을 계산하지 않는다.
    const UDMJellyPhysicsComponent* PhysicsComponent = CachedCharacter->GetJellyPhysicsComponent();

    const ADMMatchPlayerState* MatchPlayerState = Cast<ADMMatchPlayerState>(CachedCharacter->GetPlayerState());

    const bool bPlayerNotAlive =
        MatchPlayerState != nullptr && MatchPlayerState->GetLifeState() != EDMPlayerLifeState::Alive;

    const bool bDeathPresentation = PhysicsComponent != nullptr && PhysicsComponent->IsDeathPresentationActive();

    // LifeState 확정 또는 실제 Ragdoll 진입 시
    // 이전 Spring 각도와 각속도를 전부 초기화한다.
    if (bPlayerNotAlive || bDeathPresentation)
    {
        ResetJellyBodyMotion();
        return;
    }

    // 비정상적인 DeltaTime이 위상 계산에 영향을 주지 않도록 한다.
    const float SafeDeltaSeconds = FMath::Clamp(DeltaSeconds, 0.0f, 0.05f);

    UpdateJellyAirMotion(DeltaSeconds);

    // --------------------------------------------------
    // Idle 및 Walk/Sprint 흔들림
    // --------------------------------------------------

    // Idle 상태의 느린 주기.
    const float IdlePeriodSeconds = FMath::Max(JellyTuning.IdlePeriodSeconds, KINDA_SMALL_NUMBER);

    JellyIdlePhase = FMath::Fmod(JellyIdlePhase + SafeDeltaSeconds * (2.0f * PI / IdlePeriodSeconds), 2.0f * PI);

    // 정지에 가까울수록 Idle 효과가 강해진다.
    const float IdleAlpha = FMath::Clamp(1.0f - GroundSpeed / 50.0f, 0.0f, 1.0f);

    // 이동 속도에 따라 보행 흔들림 주기를 조절한다.
    const float GaitFrequency =
        FMath::Lerp(JellyTuning.WalkGaitFrequency, JellyTuning.SprintGaitFrequency, JellySpeedAlpha);

    if (bShouldMove && !bIsFalling)
    {
        JellyGaitPhase = FMath::Fmod(JellyGaitPhase + SafeDeltaSeconds * 2.0f * PI * GaitFrequency, 2.0f * PI);
    }

    const float WalkSpeed = FMath::Max(CachedCharacter->GetWalkSpeed(), 1.0f);

    const float SprintSpeed = FMath::Max(CachedCharacter->GetSprintSpeed(), WalkSpeed + 1.0f);

    // 걷는 속도에서는 최대 5도,
    // 달리는 속도에서는 최대 12도까지 기울인다.
    float BaseForwardLean = 0.0f;

    if (GroundSpeed <= WalkSpeed)
    {
        BaseForwardLean = JellyTuning.WalkForwardLean * FMath::Clamp(GroundSpeed / WalkSpeed, 0.0f, 1.0f);
    }
    else
    {
        const float SprintRangeAlpha = FMath::Clamp((GroundSpeed - WalkSpeed) / (SprintSpeed - WalkSpeed), 0.0f, 1.0f);

        BaseForwardLean = FMath::Lerp(JellyTuning.WalkForwardLean, JellyTuning.SprintForwardLean, SprintRangeAlpha);
    }

    // 캐릭터의 실제 이동 방향을 기준으로 전방 성분을 구한다.
    // 옆으로 이동하거나 뒤로 이동할 때도 대응한다.
    const FVector MovementDirection = HorizontalVelocity.GetSafeNormal();

    const float ForwardAlignment = FVector::DotProduct(MovementDirection, CachedCharacter->GetActorForwardVector());

    BaseForwardLean *= ForwardAlignment;

    // Idle의 작은 전후 흔들림.
    const float IdlePitch = FMath::Sin(JellyIdlePhase) * 1.0f * IdleAlpha;

    const float IdleRoll = FMath::Sin(JellyIdlePhase + 1.2f) * 2.0f * IdleAlpha;

    // 속도가 증가하면 좌우 보행 흔들림도 커진다.
    const float SprintGaitAlpha = FMath::Clamp((GroundSpeed - WalkSpeed) / (SprintSpeed - WalkSpeed), 0.0f, 1.0f);

    const float GaitAmplitude = FMath::Lerp(JellyTuning.WalkGaitRoll, JellyTuning.SprintGaitRoll, SprintGaitAlpha);

    const float MovingAlpha = FMath::Clamp(GroundSpeed / 80.0f, 0.0f, 1.0f);

    const float GaitRoll = FMath::Sin(JellyGaitPhase) * GaitAmplitude * MovingAlpha;

    // --------------------------------------------------
    // 출발·급정지·방향 전환 관성
    // --------------------------------------------------

    // 전방 가속은 양수, 감속은 음수다.
    // 가속 시 뒤로 쏠리고 감속 시 앞으로 쏠리게 만든다.
    const float ForwardInertiaAngle = JellyForwardInertia < 0.0f
        ? -JellyForwardInertia * JellyTuning.DecelerationLeanAngle
        : -JellyForwardInertia * JellyTuning.AccelerationLeanAngle;

    // 좌우 가속도에 반대 방향으로 몸통을 기울인다.
    const float SideInertiaAngle = -JellySideInertia * JellyTuning.SideLeanAngle;

    // --------------------------------------------------
    // 이동 중 상체 회전 지연
    // --------------------------------------------------

    const float CurrentFacingYaw = CachedCharacter->GetActorRotation().Yaw;

    float TurnLagAngle = 0.0f;

    if (bHasPreviousFacingYaw)
    {
        const float DeltaYaw = FMath::FindDeltaAngleDegrees(PreviousFacingYaw, CurrentFacingYaw);

        // 제자리 카메라 회전과 갑작스러운 위치·회전 보정을
        // 일반적인 이동 중 회전 반응에서 제외한다.
        const bool bValidTurn = GroundSpeed >= 80.0f && !bIsFalling && DeltaSeconds > KINDA_SMALL_NUMBER
            && DeltaSeconds <= 0.10f && FMath::Abs(DeltaYaw) <= 60.0f;

        if (bValidTurn)
        {
            const float TurnRate = DeltaYaw / DeltaSeconds;

            TurnLagAngle = -JellyTuning.TurnLagAngle
                * FMath::Clamp(TurnRate / FMath::Max(JellyTuning.TurnLagReferenceRate, 1.0f), -1.0f, 1.0f);
        }
    }

    PreviousFacingYaw = CurrentFacingYaw;
    bHasPreviousFacingYaw = true;

    // --------------------------------------------------
    // 최종 목표 각도 합성
    // --------------------------------------------------

    float TargetPitch = 0.0f;
    float TargetRoll = 0.0f;
    float TargetYaw = 0.0f;

    // 지상 흔들림·관성·공중 반응을 하나의 목표 회전으로 통합한다.
    if (PoseType == EDMPoseType::Default)
    {
        if (bIsFalling)
        {
            // --------------------------------------------------
            // 점프 상승 및 공중 하강
            // --------------------------------------------------

            const float VerticalSpeed = CachedCharacter->GetVelocity().Z;

            // 상승 중에는 상체가 뒤로 젖혀진다.
            const float RisingAlpha = FMath::Clamp(VerticalSpeed / 450.0f, 0.0f, 1.0f);

            // 하강 중에는 상체가 조금 앞으로 기울어진다.
            const float FallingAlpha = FMath::Clamp(-VerticalSpeed / 700.0f, 0.0f, 1.0f);

            TargetPitch = -5.0f * RisingAlpha + 3.0f * FallingAlpha;

            // 공중에서는 과도한 좌우 움직임을 제한한다.
            TargetRoll = -JellySideInertia * 3.0f;

            TargetYaw = 0.0f;
        }
        else
        {
            // --------------------------------------------------
            // 지상 이동 및 관성
            // --------------------------------------------------

            TargetPitch = BaseForwardLean + IdlePitch + ForwardInertiaAngle;

            TargetRoll = IdleRoll + GaitRoll + SideInertiaAngle;

            TargetYaw = TurnLagAngle;

            // --------------------------------------------------
            // 착지 시 추가되는 상체 압축 반응
            // --------------------------------------------------

            // 압축량이 음수일 때 상체가 조금 더
            // 앞으로 숙여지는 시각적 반응을 만든다.
            TargetPitch += -JellyLandingCompression * 0.45f;
        }
    }

    TargetPitch = FMath::Clamp(TargetPitch, -JellyTuning.BodyMaxPitch, JellyTuning.BodyMaxPitch);

    TargetRoll = FMath::Clamp(TargetRoll, -JellyTuning.BodyMaxRoll, JellyTuning.BodyMaxRoll);

    TargetYaw = FMath::Clamp(TargetYaw, -JellyTuning.BodyMaxYaw, JellyTuning.BodyMaxYaw);

    // --------------------------------------------------
    // 실제 Spring 반동 적용
    // --------------------------------------------------

    AdvanceJellySpring(TargetPitch, DeltaSeconds, JellyTuning.BodyMaxPitch, JellyBodyPitch, JellyPitchVelocity);

    AdvanceJellySpring(TargetRoll, DeltaSeconds, JellyTuning.BodyMaxRoll, JellyBodyRoll, JellyRollVelocity);

    AdvanceJellySpring(TargetYaw, DeltaSeconds, JellyTuning.BodyMaxYaw, JellyBodyYaw, JellyYawVelocity);

    // --------------------------------------------------
    // 최종 몸통 회전 분배
    // --------------------------------------------------

    // FRotator 생성자 인자 순서: Pitch, Yaw, Roll.
    // Spine1은 전체 몸통의 기본 기울기를 담당한다.
    JellySpine1Rotation = FRotator(JellyBodyPitch * 0.55f, JellyBodyYaw * 0.20f, JellyBodyRoll * 0.35f);

    // Spine2는 상체 관성 반응을 담당한다.
    JellySpine2Rotation = FRotator(JellyBodyPitch * 0.30f, JellyBodyYaw * 0.35f, JellyBodyRoll * 0.45f);

    // Ribcage는 흉부의 잔여 흔들림을 담당한다.
    JellyRibcageRotation = FRotator(JellyBodyPitch * 0.15f, JellyBodyYaw * 0.45f, JellyBodyRoll * 0.20f);

    // 기본적으로 착지 Translation을 사용하지 않는다.
    JellyLandingRibcageOffset = FVector::ZeroVector;

    // 일반 자세의 지상 상태에서만 흉부 압축을 출력한다.
    // 기존 공격·방어 몽타주가 재생 중이면 추가하지 않는다.
    const bool bAllowLandingCompression = PoseType == EDMPoseType::Default && !bIsFalling && !IsAnyMontagePlaying();

    if (bAllowLandingCompression)
    {
        // Z축은 초기 후보이며 실제 Bone 로컬 축 검증이 필요하다.
        JellyLandingRibcageOffset = FVector(0.0f, 0.0f, JellyLandingCompression);
    }

    UpdateJellySecondaryMotion(DeltaSeconds);
}

void UDMJellyAnimInstance::UpdateJellySecondaryMotion(const float DeltaSeconds)
{
    if (!IsValid(CachedCharacter))
    {
        ResetJellySecondaryMotion();
        return;
    }

    // 특수 자세와 기존 몽타주를 우선한다.
    const bool bAllowSecondary = PoseType == EDMPoseType::Default && !IsAnyMontagePlaying();

    // 공중에서는 보조 흔들림을 약하게 적용한다.
    const float AirAlpha = bIsFalling ? 0.65f : 1.0f;

    // --------------------------------------------------
    // 1. 이동 상태별 강도 계산
    // --------------------------------------------------

    const float WalkSpeed = FMath::Max(CachedCharacter->GetWalkSpeed(), 1.0f);

    const float SprintSpeed = FMath::Max(CachedCharacter->GetSprintSpeed(), WalkSpeed + 1.0f);

    const float SprintAlpha = FMath::Clamp((GroundSpeed - WalkSpeed) / (SprintSpeed - WalkSpeed), 0.0f, 1.0f);

    const float MovingAlpha = FMath::Clamp(GroundSpeed / 80.0f, 0.0f, 1.0f);

    const float IdleAlpha = FMath::Clamp(1.0f - GroundSpeed / 50.0f, 0.0f, 1.0f);

    // --------------------------------------------------
    // 2. 머리의 목표 회전 계산
    // --------------------------------------------------

    float TargetHeadPitch = 0.0f;
    float TargetHeadRoll = 0.0f;

    if (bAllowSecondary)
    {
        // 몸통이 앞으로 움직일 때 머리는 반대 방향으로
        // 따라오도록 하여 관성 지연을 표현한다.
        TargetHeadPitch = -JellyBodyPitch * 0.45f;

        // 좌우 기울기에도 머리가 늦게 따라온다.
        TargetHeadRoll = -JellyBodyRoll * 0.50f;

        // 정지 중에는 작은 머리 끄덕임을 추가한다.
        TargetHeadPitch += FMath::Sin(JellyIdlePhase + 0.6f) * 1.2f * IdleAlpha;

        // 급격한 좌우 가속도에 추가 반응한다.
        TargetHeadRoll += JellySideInertia * 2.0f;

        // 착지 시 머리가 몸통과 다르게 움직이도록 한다.
        TargetHeadPitch += -JellyLandingCompression * 0.30f;

        TargetHeadPitch *= AirAlpha;
        TargetHeadRoll *= AirAlpha;
    }

    TargetHeadPitch = FMath::Clamp(TargetHeadPitch, -JellyTuning.HeadMaxAngle, JellyTuning.HeadMaxAngle);

    TargetHeadRoll = FMath::Clamp(TargetHeadRoll, -JellyTuning.HeadMaxAngle, JellyTuning.HeadMaxAngle);

    // 몸통 관성과 같은 감쇠 스프링을 재사용한다.
    AdvanceJellySpring(TargetHeadPitch, DeltaSeconds, JellyTuning.HeadMaxAngle, JellyHeadPitch, JellyHeadPitchVelocity);

    AdvanceJellySpring(TargetHeadRoll, DeltaSeconds, JellyTuning.HeadMaxAngle, JellyHeadRoll, JellyHeadRollVelocity);

    // --------------------------------------------------
    // 3. 양팔의 공통 목표 관성 계산
    // --------------------------------------------------

    float TargetArmPitch = 0.0f;
    float TargetArmRoll = 0.0f;

    if (bAllowSecondary)
    {
        // 몸통과 다른 속도로 팔이 따라오도록 한다.
        TargetArmPitch = -JellyBodyPitch * 0.30f - JellyForwardInertia * 4.0f;

        TargetArmRoll = -JellyBodyRoll * 0.35f - JellySideInertia * 5.0f;

        // 착지 시 팔에도 충격 반동을 전달한다.
        TargetArmPitch += -JellyLandingCompression * 0.35f;

        TargetArmPitch *= AirAlpha;
        TargetArmRoll *= AirAlpha;
    }

    TargetArmPitch = FMath::Clamp(TargetArmPitch, -JellyTuning.ArmMaxAngle, JellyTuning.ArmMaxAngle);

    TargetArmRoll = FMath::Clamp(TargetArmRoll, -JellyTuning.ArmMaxAngle, JellyTuning.ArmMaxAngle);

    AdvanceJellySpring(TargetArmPitch, DeltaSeconds, JellyTuning.ArmMaxAngle, JellyArmPitch, JellyArmPitchVelocity);

    AdvanceJellySpring(TargetArmRoll, DeltaSeconds, JellyTuning.ArmMaxAngle, JellyArmRoll, JellyArmRollVelocity);

    // --------------------------------------------------
    // 4. 걷기·달리기 중 양팔 교대 흔들림
    // --------------------------------------------------

    float GaitSwing = 0.0f;
    float IdleArmSwing = 0.0f;

    if (bAllowSecondary)
    {
        const float GaitAmplitude = FMath::Lerp(JellyTuning.WalkArmSwing, JellyTuning.SprintArmSwing, SprintAlpha);

        // 기존 보행 위상을 재사용한다.
        GaitSwing = FMath::Sin(JellyGaitPhase) * GaitAmplitude * MovingAlpha;

        // 공중에서는 큰 보행 흔들림을 억제한다.
        if (bIsFalling)
        {
            GaitSwing *= 0.25f;
        }

        // Idle에서는 양팔이 작게 반대 방향으로 흔들린다.
        IdleArmSwing = FMath::Sin(JellyIdlePhase) * 1.2f * IdleAlpha;
    }

    // --------------------------------------------------
    // 5. 머리의 최종 회전 출력
    // --------------------------------------------------

    const float HeadYaw = bAllowSecondary ? FMath::Clamp(-JellyBodyYaw * 0.30f, -3.0f, 3.0f) : 0.0f;

    JellyHeadRotation = FRotator(JellyHeadPitch, HeadYaw, JellyHeadRoll);

    // --------------------------------------------------
    // 6. 왼팔·오른팔을 서로 다르게 출력
    // --------------------------------------------------

    const float LeftArmPitch = FMath::Clamp(JellyArmPitch + GaitSwing + IdleArmSwing, -14.0f, 14.0f);

    const float RightArmPitch = FMath::Clamp(JellyArmPitch - GaitSwing - IdleArmSwing, -14.0f, 14.0f);

    const float LeftArmRoll = FMath::Clamp(JellyArmRoll, -12.0f, 12.0f);

    const float RightArmRoll = FMath::Clamp(-JellyArmRoll, -12.0f, 12.0f);

    JellyLeftUpperarmRotation = FRotator(LeftArmPitch, 0.0f, LeftArmRoll);

    JellyRightUpperarmRotation = FRotator(RightArmPitch, 0.0f, RightArmRoll);

    // --------------------------------------------------
    // 7. 아래팔에 더 작은 2차 반응 적용
    // --------------------------------------------------

    JellyLeftForearmRotation =
        FRotator(FMath::Clamp(LeftArmPitch * 0.35f, -6.0f, 6.0f), 0.0f, FMath::Clamp(LeftArmRoll * 0.35f, -5.0f, 5.0f));

    JellyRightForearmRotation = FRotator(
        FMath::Clamp(RightArmPitch * 0.35f, -6.0f, 6.0f), 0.0f, FMath::Clamp(RightArmRoll * 0.35f, -5.0f, 5.0f)
    );
}

void UDMJellyAnimInstance::CacheOwnerReferences()
{
    CachedCharacter = Cast<ADMJellyCharacterBase>(TryGetPawnOwner());

    CachedPoseComponent = nullptr;

    if (!IsValid(CachedCharacter))
    {
        return;
    }

    /*
     * PoseComponent는 Player에만 존재할 수 있으므로
     * Owner를 캐시할 때 한 번 조회한다.
     */
    CachedPoseComponent = CachedCharacter->FindComponentByClass<UDMPoseComponent>();
}