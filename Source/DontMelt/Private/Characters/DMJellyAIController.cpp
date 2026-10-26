#include "Characters/DMJellyAIController.h"

#include "Characters/DMNPCCharacter.h"
#include "Characters/Components/DMCharacterMovementComponent.h"
#include "Characters/Components/DMCharacterStateComponent.h"
#include "Characters/Components/DMPoseComponent.h"
#include "Data/DMNPCBehaviorTypes.h"
#include "Framework/Match/DMMatchGameMode.h"

#include "Relic/DMRelic.h"
#include "Relic/DMRelicChest.h"
#include "Relic/DMRelicManager.h"

#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogDMJellyAIController, Log, All);

namespace DMJellyAIControllerPrivate
{
    /*
     * Population 생성 직후 20~40 NPC가
     * 같은 Frame에 Path 요청을 몰아서 보내지 않도록
     * 최초 행동 시작 시간을 분산한다.
     */
    constexpr float InitialDelayMin = 0.15f;
    constexpr float InitialDelayMax = 1.00f;

    /*
     * 이동 완료 직후 바로 다음 Path 요청이 발생하지 않도록
     * 짧은 간격을 둔다.
     */
    constexpr float PostMoveDelayMin = 0.15f;
    constexpr float PostMoveDelayMax = 0.40f;

    /*
     * ACharacter::Jump()의 입력을
     * 다음 Frame 이후 해제하기 위한 짧은 시간.
     */
    constexpr float JumpReleaseDelay = 0.10f;

    /*
     * Jump 행동 후 다음 행동까지의 최소 간격.
     */
    constexpr float JumpBehaviorDelay = 0.60f;

    /*
     * Nav 목적지 탐색 실패 시
     * 매 Frame 재시도하지 않는다.
     */
    constexpr float NavigationRetryDelay = 0.50f;

    /*
     * Scream Presentation이 먼저 보인 뒤
     * Flee가 시작되도록 하는 짧은 Lead-in.
     *
     * 동시에 기존 Nav Abort Callback이
     * 정리될 시간도 확보한다.
     */
    constexpr float ScreamLeadInDuration = 3.0f;

    /*
     * 직선 반대 방향 목적지를
     * NavMesh에 투영할 수 없을 때의
     * Fallback 후보 수.
     */
    constexpr int32 FleeFallbackSampleCount = 4;

    /*
     * 기존 공용 Emote Presentation 경로에서
     * Scream을 구분하는 Presentation ID.
     */
    const FName ScreamEmoteId = TEXT("Scream");
    const FName LaughEmoteId = TEXT("Laugh");

    enum class EBasicBehavior : uint8
    {
        Walk,
        Idle,
        Sprint,
        Jump,
        RelicApproach
    };
}

#pragma region Lifecycle

ADMJellyAIController::ADMJellyAIController()
{
    PrimaryActorTick.bCanEverTick = false;
}

void ADMJellyAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    if (!HasAuthority())
    {
        UE_LOG(LogDMJellyAIController, Error, TEXT("OnPossess rejected: NPC AIController must run on server."));

        return;
    }

    const ADMNPCCharacter* NPCCharacter = Cast<ADMNPCCharacter>(InPawn);

    if (!ensureMsgf(
            IsValid(NPCCharacter),
            TEXT("OnPossess failed: possessed Pawn is not ADMNPCCharacter. Pawn=%s"),
            *GetNameSafe(InPawn)
        ))
    {
        return;
    }

    TransitionToBehaviorState(EDMNPCBehaviorState::Idle);

    if (!ResolveBehaviorProfile())
    {
        UE_LOG(
            LogDMJellyAIController,
            Error,
            TEXT("NPC behavior initialization failed. NPC=%s"),
            *GetNameSafe(NPCCharacter)
        );

        return;
    }

    const float InitialDelay = FMath::FRandRange(
        DMJellyAIControllerPrivate::InitialDelayMin,

        DMJellyAIControllerPrivate::InitialDelayMax
    );

    ScheduleNextBehavior(InitialDelay);

    UE_LOG(
        LogDMJellyAIController,
        Verbose,
        TEXT("NPC possessed successfully. Controller=%s, NPC=%s"),
        *GetNameSafe(this),
        *GetNameSafe(NPCCharacter)
    );
}

void ADMJellyAIController::OnUnPossess()
{
    /*
     * StopMovement()로 발생할 수 있는 Callback이
     * 새로운 행동을 예약하지 못하도록
     * Profile을 먼저 비활성화한다.
     */
    BehaviorProfile = nullptr;

    GetWorldTimerManager().ClearTimer(BehaviorTimerHandle);

    GetWorldTimerManager().ClearTimer(JumpReleaseTimerHandle);

    GetWorldTimerManager().ClearTimer(ReactionTimerHandle);

    PendingFleeThreatOrigin = FVector::ZeroVector;

    PendingFleeDistance = 0.0f;

    CurrentRelicApproachTarget.Reset();

    ADMNPCCharacter* NPCCharacter = Cast<ADMNPCCharacter>(GetPawn());

    if (IsValid(NPCCharacter))
    {
        NPCCharacter->StopJumping();

        NPCCharacter->ClearSprintRequest();
    }

    TransitionToBehaviorState(EDMNPCBehaviorState::Idle);

    StopMovement();

    Super::OnUnPossess();
}

#pragma endregion

#pragma region Wrong Target Reaction

void ADMJellyAIController::HandleWrongTargetReaction(const FVector& ThreatOrigin, const float FleeDistance)
{
    if (!HasAuthority() || ThreatOrigin.ContainsNaN() || FleeDistance <= 0.0f || BehaviorProfile == nullptr)
    {
        return;
    }

    ADMNPCCharacter* NPCCharacter = Cast<ADMNPCCharacter>(GetPawn());

    if (!IsValid(NPCCharacter))
    {
        return;
    }

    /*
     * 이미 오인공격 위협에 반응 중이었다면
     * 짧은 시간에 느낌표를 반복 생성하지 않는다.
     *
     * 기존 AI 반응 재시작과 Threat 갱신은 유지한다.
     */
    const bool bWasAlreadyReacting = CurrentBehaviorState == EDMNPCBehaviorState::Reacting
        || CurrentBehaviorState == EDMNPCBehaviorState::FleeMoving;

    /*
     * 기존 자율 행동 Timer를 모두 취소한다.
     */
    GetWorldTimerManager().ClearTimer(BehaviorTimerHandle);

    GetWorldTimerManager().ClearTimer(JumpReleaseTimerHandle);

    GetWorldTimerManager().ClearTimer(ReactionTimerHandle);

    CurrentRelicApproachTarget.Reset();

    PendingFleeThreatOrigin = ThreatOrigin;

    PendingFleeDistance = FleeDistance;

    /*
     * StopMovement()로 발생하는
     * 기존 MoveCompleted Callback이
     * 일반 행동 완료로 처리되지 않도록
     * 먼저 Reacting 상태로 전환한다.
     */
    TransitionToBehaviorState(EDMNPCBehaviorState::Reacting);

    StopMovement();

    NPCCharacter->StopJumping();

    NPCCharacter->ClearSprintRequest();

    // 새로운 오인공격 반응이 시작될 때만 느낌표를 표시한다.
    if (!bWasAlreadyReacting)
    {
        NPCCharacter->MulticastPlayAlertEffect();
    }

    // 기존 공용 Emote 경로에서 비명 애니메이션과 사운드를 한 번만 요청한다.
    NPCCharacter->RequestEmote(DMJellyAIControllerPrivate::ScreamEmoteId);

    /*
     * 바로 Path 요청을 넣지 않고
     * 짧은 Lead-in 후 Flee를 시작한다.
     */
    GetWorldTimerManager().SetTimer(
        ReactionTimerHandle,
        this,
        &ThisClass::StartPendingFleeReaction,
        DMJellyAIControllerPrivate::ScreamLeadInDuration,
        false
    );
}

void ADMJellyAIController::StartPendingFleeReaction()
{
    if (!HasAuthority() || CurrentBehaviorState != EDMNPCBehaviorState::Reacting || BehaviorProfile == nullptr)
    {
        return;
    }

    const FVector ThreatOrigin = PendingFleeThreatOrigin;

    const float FleeDistance = PendingFleeDistance;

    PendingFleeThreatOrigin = FVector::ZeroVector;

    PendingFleeDistance = 0.0f;

    FVector FleeDestination = FVector::ZeroVector;

    /*
     * Navigation 함수는 Idle에서만
     * 새 Move를 시작하므로
     * 기존 Reaction 준비 상태를 종료한다.
     */
    TransitionToBehaviorState(EDMNPCBehaviorState::Idle);

    if (!TryFindFleeDestination(ThreatOrigin, FleeDistance, FleeDestination))
    {
        ScheduleNextBehavior(DMJellyAIControllerPrivate::NavigationRetryDelay);

        return;
    }

    const bool bFleeStarted = TryStartNavigationMove(
        FleeDestination, BehaviorProfile->MoveAcceptanceRadius, true, EDMNPCBehaviorState::FleeMoving
    );

    if (!bFleeStarted || CurrentBehaviorState != EDMNPCBehaviorState::FleeMoving)
    {
        ScheduleNextBehavior(DMJellyAIControllerPrivate::NavigationRetryDelay);
    }
}

#pragma endregion

#pragma region Panic Reaction

void ADMJellyAIController::HandlePanicReaction()
{
    if (!HasAuthority() || BehaviorProfile == nullptr)
    {
        return;
    }

    ADMNPCCharacter* NPCCharacter = Cast<ADMNPCCharacter>(GetPawn());

    if (!IsValid(NPCCharacter))
    {
        return;
    }

    /*
     * 직접 공격받아 반응 중인 NPC의
     * 구체적인 Threat Reaction이
     * Global Panic보다 우선한다.
     */
    if (CurrentBehaviorState == EDMNPCBehaviorState::Reacting
        || CurrentBehaviorState == EDMNPCBehaviorState::FleeMoving)
    {
        return;
    }

    /*
     * 동일 Panic Event가 중복 전달되어도
     * 다시 Timer를 시작하지 않는다.
     */
    if (CurrentBehaviorState == EDMNPCBehaviorState::PanicWaiting
        || CurrentBehaviorState == EDMNPCBehaviorState::PanicMoving)
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(BehaviorTimerHandle);

    GetWorldTimerManager().ClearTimer(JumpReleaseTimerHandle);

    GetWorldTimerManager().ClearTimer(ReactionTimerHandle);

    CurrentRelicApproachTarget.Reset();

    PendingFleeThreatOrigin = FVector::ZeroVector;

    PendingFleeDistance = 0.0f;

    /*
     * StopMovement() Callback이 기존 행동 완료로
     * 처리되지 않게 State를 먼저 변경한다.
     */
    TransitionToBehaviorState(EDMNPCBehaviorState::PanicWaiting);

    StopMovement();

    NPCCharacter->StopJumping();

    NPCCharacter->ClearSprintRequest();

    const float PanicDelay = FMath::FRandRange(BehaviorProfile->PanicDelayMin, BehaviorProfile->PanicDelayMax);

    GetWorldTimerManager().SetTimer(
        ReactionTimerHandle, this, &ThisClass::StartPendingPanicReaction, PanicDelay, false
    );
}

void ADMJellyAIController::StartPendingPanicReaction()
{
    if (!HasAuthority() || BehaviorProfile == nullptr || CurrentBehaviorState != EDMNPCBehaviorState::PanicWaiting)
    {
        return;
    }

    ADMNPCCharacter* NPCCharacter = Cast<ADMNPCCharacter>(GetPawn());

    if (!IsValid(NPCCharacter))
    {
        return;
    }

    // 실제 패닉 반응 시작 시점에만 느낌표를 표시한다.
    NPCCharacter->MulticastPlayAlertEffect();

    /*
     * 일부 NPC만 Scream.
     */
    if (FMath::FRand() < BehaviorProfile->PanicScreamChance)
    {
        NPCCharacter->RequestEmote(DMJellyAIControllerPrivate::ScreamEmoteId);
    }

    /*
     * 일부 NPC만 Jump.
     */
    if (NPCCharacter->CanJump() && FMath::FRand() < BehaviorProfile->PanicJumpChance)
    {
        NPCCharacter->Jump();

        GetWorldTimerManager().ClearTimer(JumpReleaseTimerHandle);

        GetWorldTimerManager().SetTimer(
            JumpReleaseTimerHandle, this, &ThisClass::ReleaseJump, DMJellyAIControllerPrivate::JumpReleaseDelay, false
        );
    }

    FVector PanicDestination = FVector::ZeroVector;

    if (!TryFindRoamDestination(PanicDestination))
    {
        ScheduleNextBehavior(DMJellyAIControllerPrivate::NavigationRetryDelay);

        return;
    }

    /*
     * TryStartNavigationMove는 Idle 상태에서만
     * 새로운 Move를 허용한다.
     */
    TransitionToBehaviorState(EDMNPCBehaviorState::Idle);

    const bool bPanicMoveStarted = TryStartNavigationMove(
        PanicDestination, BehaviorProfile->MoveAcceptanceRadius, true, EDMNPCBehaviorState::PanicMoving
    );

    if (!bPanicMoveStarted || CurrentBehaviorState != EDMNPCBehaviorState::PanicMoving)
    {
        ScheduleNextBehavior(DMJellyAIControllerPrivate::NavigationRetryDelay);
    }
}

#pragma endregion

#pragma region Death Mocking Reaction

bool ADMJellyAIController::TryStartDeathMockingReaction(const FVector& DeathLocation, const float DurationSeconds)
{
    if (!HasAuthority() || BehaviorProfile == nullptr || DeathLocation.ContainsNaN() || DurationSeconds <= 0.0f)
    {
        return false;
    }

    ADMNPCCharacter* NPCCharacter = Cast<ADMNPCCharacter>(GetPawn());
    if (!IsValid(NPCCharacter))
    {
        return false;
    }

    // 기존 행동·자세 정책에서 감정표현이 차단된 NPC는 후보에서 제외한다.
    const UDMCharacterStateComponent* StateComponent = NPCCharacter->GetCharacterStateComponent();
    if (!IsValid(StateComponent) || !StateComponent->CanPerformAction(EDMActionRequest::Emote))
    {
        return false;
    }

    const UDMPoseComponent* PoseComponent = NPCCharacter->FindComponentByClass<UDMPoseComponent>();
    if (IsValid(PoseComponent) && !PoseComponent->IsActionAllowedByPose(EDMActionRequest::Emote))
    {
        return false;
    }

    // 위협 도주·패닉 반응을 우선한다. 비웃음 중 추가 사망은 무시한다.
    switch (CurrentBehaviorState)
    {
    case EDMNPCBehaviorState::Reacting:
    case EDMNPCBehaviorState::FleeMoving:
    case EDMNPCBehaviorState::PanicWaiting:
    case EDMNPCBehaviorState::PanicMoving:
    case EDMNPCBehaviorState::DeathMocking:
        return false;

    default:
        break;
    }

    const FVector ToDeath = DeathLocation - NPCCharacter->GetActorLocation();
    const FVector HorizontalDirection(ToDeath.X, ToDeath.Y, 0.0f);

    // 이전 행동의 Timer·이동 완료 Callback이 새 행동을 덮어쓰지 않도록 먼저 전환한다.
    TransitionToBehaviorState(EDMNPCBehaviorState::DeathMocking);

    GetWorldTimerManager().ClearTimer(BehaviorTimerHandle);
    GetWorldTimerManager().ClearTimer(JumpReleaseTimerHandle);
    GetWorldTimerManager().ClearTimer(ReactionTimerHandle);

    CurrentRelicApproachTarget.Reset();
    PendingFleeThreatOrigin = FVector::ZeroVector;
    PendingFleeDistance = 0.0f;

    StopMovement();
    NPCCharacter->StopJumping();
    NPCCharacter->ClearSprintRequest();

    // 남아 있는 속도로 캐릭터가 미끄러지며 회전하는 것을 방지한다.
    if (UCharacterMovementComponent* MovementComponent = NPCCharacter->GetCharacterMovement())
    {
        MovementComponent->StopMovementImmediately();
    }

    // 머리 본 별도 추적 없이 서버에서 몸체 방향을 한 번만 확정한다.
    if (!HorizontalDirection.IsNearlyZero())
    {
        const FRotator LookRotation(0.0f, HorizontalDirection.Rotation().Yaw, 0.0f);
        NPCCharacter->SetActorRotation(LookRotation, ETeleportType::TeleportPhysics);
        NPCCharacter->ForceNetUpdate();
    }

    // 기존 Emote Multicast를 통해 모든 관련 클라이언트에서 웃음소리를 재생한다.
    NPCCharacter->RequestEmote(DMJellyAIControllerPrivate::LaughEmoteId);

    // 별도 Tick 없이 기존 Reaction Timer로 종료 시점을 예약한다.
    GetWorldTimerManager().SetTimer(
        ReactionTimerHandle, this, &ThisClass::FinishDeathMockingReaction, DurationSeconds, false
    );

    return true;
}

void ADMJellyAIController::FinishDeathMockingReaction()
{
    if (!HasAuthority() || BehaviorProfile == nullptr || CurrentBehaviorState != EDMNPCBehaviorState::DeathMocking)
    {
        return;
    }

    // 이동·상태는 기존 행동 선택 경로에 다시 위임한다.
    ScheduleNextBehavior(DMJellyAIControllerPrivate::NavigationRetryDelay);
}

#pragma endregion

#pragma region Behavior Profile

bool ADMJellyAIController::ResolveBehaviorProfile()
{
    const ADMNPCCharacter* NPCCharacter = Cast<ADMNPCCharacter>(GetPawn());

    if (!IsValid(NPCCharacter))
    {
        return false;
    }

    const FDataTableRowHandle& ProfileHandle = NPCCharacter->GetBehaviorProfileHandle();

    if (!ensureMsgf(
            IsValid(ProfileHandle.DataTable) && !ProfileHandle.RowName.IsNone(),
            TEXT("Behavior Profile is not configured. NPC=%s"),
            *GetNameSafe(NPCCharacter)
        ))
    {
        return false;
    }

    const FDMNPCBehaviorProfileRow* ResolvedProfile =
        ProfileHandle.GetRow<FDMNPCBehaviorProfileRow>(TEXT("DM NPC Behavior Profile"));

    if (!ensureMsgf(
            ResolvedProfile != nullptr,
            TEXT("Behavior Profile row could not be resolved. NPC=%s, Row=%s"),
            *GetNameSafe(NPCCharacter),
            *ProfileHandle.RowName.ToString()
        ))
    {
        return false;
    }

    const bool bHasInvalidWeight = ResolvedProfile->WalkWeight < 0.0f || ResolvedProfile->IdleWeight < 0.0f
        || ResolvedProfile->SprintWeight < 0.0f || ResolvedProfile->JumpWeight < 0.0f
        || ResolvedProfile->RelicApproachWeight < 0.0f;

    const float CurrentStageTotalWeight = ResolvedProfile->WalkWeight + ResolvedProfile->IdleWeight
        + ResolvedProfile->SprintWeight + ResolvedProfile->JumpWeight + ResolvedProfile->RelicApproachWeight;

    const bool bInvalidProfile = bHasInvalidWeight || CurrentStageTotalWeight <= KINDA_SMALL_NUMBER
        || ResolvedProfile->IdleDuration <= 0.0f || ResolvedProfile->RoamRadius <= 0.0f
        || ResolvedProfile->MoveAcceptanceRadius < 0.0f

        // Panic Profile Validation
        || ResolvedProfile->PanicDelayMin < 0.0f || ResolvedProfile->PanicDelayMax < ResolvedProfile->PanicDelayMin
        || !FMath::IsWithinInclusive(ResolvedProfile->PanicScreamChance, 0.0f, 1.0f)
        || !FMath::IsWithinInclusive(ResolvedProfile->PanicJumpChance, 0.0f, 1.0f);

    if (!ensureMsgf(
            !bInvalidProfile,
            TEXT("Behavior Profile contains invalid values. Row=%s"),
            *ProfileHandle.RowName.ToString()
        ))
    {
        return false;
    }

    BehaviorProfile = ResolvedProfile;

    return true;
}

#pragma endregion

#pragma region Behavior Loop

void ADMJellyAIController::ScheduleNextBehavior(const float DelaySeconds)
{
    if (!HasAuthority() || BehaviorProfile == nullptr || !IsValid(GetPawn()))
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(BehaviorTimerHandle);

    TransitionToBehaviorState(EDMNPCBehaviorState::Waiting);

    const float SafeDelay = FMath::Max(DelaySeconds, 0.01f);

    GetWorldTimerManager().SetTimer(BehaviorTimerHandle, this, &ThisClass::SelectNextBehavior, SafeDelay, false);
}

void ADMJellyAIController::SelectNextBehavior()
{
    if (!HasAuthority() || BehaviorProfile == nullptr || !IsValid(GetPawn()))
    {
        return;
    }

    CurrentRelicApproachTarget.Reset();

    TransitionToBehaviorState(EDMNPCBehaviorState::Idle);

    /*
     * 6-5는 보류 상태이므로
     * Emote / FakeAttack Weight는 아직 제외한다.
     */
    const float TotalWeight = BehaviorProfile->WalkWeight + BehaviorProfile->IdleWeight + BehaviorProfile->SprintWeight
        + BehaviorProfile->JumpWeight + BehaviorProfile->RelicApproachWeight;

    if (TotalWeight <= KINDA_SMALL_NUMBER)
    {
        ScheduleNextBehavior(DMJellyAIControllerPrivate::NavigationRetryDelay);

        return;
    }

    const float Roll = FMath::FRandRange(0.0f, TotalWeight);

    float AccumulatedWeight = BehaviorProfile->WalkWeight;

    DMJellyAIControllerPrivate::EBasicBehavior SelectedBehavior = DMJellyAIControllerPrivate::EBasicBehavior::Walk;

    if (Roll < AccumulatedWeight)
    {
        SelectedBehavior = DMJellyAIControllerPrivate::EBasicBehavior::Walk;
    }
    else
    {
        AccumulatedWeight += BehaviorProfile->IdleWeight;

        if (Roll < AccumulatedWeight)
        {
            SelectedBehavior = DMJellyAIControllerPrivate::EBasicBehavior::Idle;
        }
        else
        {
            AccumulatedWeight += BehaviorProfile->SprintWeight;

            if (Roll < AccumulatedWeight)
            {
                SelectedBehavior = DMJellyAIControllerPrivate::EBasicBehavior::Sprint;
            }
            else
            {
                AccumulatedWeight += BehaviorProfile->JumpWeight;

                if (Roll < AccumulatedWeight)
                {
                    SelectedBehavior = DMJellyAIControllerPrivate::EBasicBehavior::Jump;
                }
                else
                {
                    SelectedBehavior = DMJellyAIControllerPrivate::EBasicBehavior::RelicApproach;
                }
            }
        }
    }

    switch (SelectedBehavior)
    {
    case DMJellyAIControllerPrivate::EBasicBehavior::Walk:

        StartWalkBehavior();
        break;

    case DMJellyAIControllerPrivate::EBasicBehavior::Idle:

        StartIdleBehavior();
        break;

    case DMJellyAIControllerPrivate::EBasicBehavior::Sprint:

        StartSprintBehavior();
        break;

    case DMJellyAIControllerPrivate::EBasicBehavior::Jump:

        StartJumpBehavior();
        break;

    case DMJellyAIControllerPrivate::EBasicBehavior::RelicApproach:

        StartRelicApproachBehavior();
        break;
    }
}

void ADMJellyAIController::StartWalkBehavior()
{
    if (BehaviorProfile == nullptr)
    {
        return;
    }

    FVector Destination = FVector::ZeroVector;

    if (!TryFindRoamDestination(Destination))
    {
        ScheduleNextBehavior(DMJellyAIControllerPrivate::NavigationRetryDelay);

        return;
    }

    const bool bMoveStarted =
        TryStartNavigationMove(Destination, BehaviorProfile->MoveAcceptanceRadius, false, EDMNPCBehaviorState::Moving);

    /*
     * AlreadyAtGoal 등으로 실제 Moving 상태에
     * 들어가지 않았다면 다음 행동을 예약한다.
     */
    if (!bMoveStarted || CurrentBehaviorState != EDMNPCBehaviorState::Moving)
    {
        ScheduleNextBehavior(DMJellyAIControllerPrivate::NavigationRetryDelay);
    }
}

void ADMJellyAIController::StartIdleBehavior()
{
    if (BehaviorProfile == nullptr)
    {
        return;
    }

    ADMNPCCharacter* NPCCharacter = Cast<ADMNPCCharacter>(GetPawn());

    if (IsValid(NPCCharacter))
    {
        NPCCharacter->ClearSprintRequest();
    }

    /*
     * 같은 DataTable 값을 사용하더라도
     * NPC 40명이 완전히 동일한 주기로
     * 행동하지 않도록 작은 Jitter를 준다.
     */
    const float IdleDuration = BehaviorProfile->IdleDuration * FMath::FRandRange(0.85f, 1.15f);

    ScheduleNextBehavior(IdleDuration);
}

void ADMJellyAIController::StartSprintBehavior()
{
    if (BehaviorProfile == nullptr)
    {
        return;
    }

    FVector Destination = FVector::ZeroVector;

    if (!TryFindRoamDestination(Destination))
    {
        ScheduleNextBehavior(DMJellyAIControllerPrivate::NavigationRetryDelay);

        return;
    }

    const bool bMoveStarted =
        TryStartNavigationMove(Destination, BehaviorProfile->MoveAcceptanceRadius, true, EDMNPCBehaviorState::Moving);

    if (!bMoveStarted || CurrentBehaviorState != EDMNPCBehaviorState::Moving)
    {
        ScheduleNextBehavior(DMJellyAIControllerPrivate::NavigationRetryDelay);
    }
}

void ADMJellyAIController::StartJumpBehavior()
{
    ADMNPCCharacter* NPCCharacter = Cast<ADMNPCCharacter>(GetPawn());

    if (!IsValid(NPCCharacter))
    {
        return;
    }

    NPCCharacter->ClearSprintRequest();

    /*
     * 공중이거나 Jump 불가 상태라면
     * 이번 행동을 건너뛴다.
     */
    if (!NPCCharacter->CanJump())
    {
        ScheduleNextBehavior(DMJellyAIControllerPrivate::NavigationRetryDelay);

        return;
    }

    NPCCharacter->Jump();

    TransitionToBehaviorState(EDMNPCBehaviorState::Waiting);

    GetWorldTimerManager().ClearTimer(JumpReleaseTimerHandle);

    GetWorldTimerManager().SetTimer(
        JumpReleaseTimerHandle, this, &ThisClass::ReleaseJump, DMJellyAIControllerPrivate::JumpReleaseDelay, false
    );

    ScheduleNextBehavior(DMJellyAIControllerPrivate::JumpBehaviorDelay);
}

void ADMJellyAIController::StartRelicApproachBehavior()
{
    if (BehaviorProfile == nullptr)
    {
        return;
    }

    AActor* Target = nullptr;

    if (!TrySelectRelicApproachTarget(Target))
    {
        ScheduleNextBehavior(DMJellyAIControllerPrivate::NavigationRetryDelay);

        return;
    }

    CurrentRelicApproachTarget = Target;

    const bool bMoveAccepted = TryStartNavigationMove(
        Target->GetActorLocation(),
        BehaviorProfile->MoveAcceptanceRadius,
        false,
        EDMNPCBehaviorState::RelicApproachMoving
    );

    if (!bMoveAccepted)
    {
        CurrentRelicApproachTarget.Reset();

        ScheduleNextBehavior(DMJellyAIControllerPrivate::NavigationRetryDelay);

        return;
    }

    /*
     * 이미 Target 근처에 있었던 경우.
     *
     * MoveToLocation()은 AlreadyAtGoal을 반환하고
     * State는 Idle로 돌아온다.
     */
    if (CurrentBehaviorState == EDMNPCBehaviorState::Idle)
    {
        if (IsRelicApproachTargetValid(Target))
        {
            ScheduleNextBehavior(BehaviorProfile->RelicApproachWaitDuration);
        }
        else
        {
            CurrentRelicApproachTarget.Reset();

            ScheduleNextBehavior(DMJellyAIControllerPrivate::NavigationRetryDelay);
        }

        return;
    }

    if (CurrentBehaviorState != EDMNPCBehaviorState::RelicApproachMoving)
    {
        CurrentRelicApproachTarget.Reset();

        ScheduleNextBehavior(DMJellyAIControllerPrivate::NavigationRetryDelay);
    }
}

void ADMJellyAIController::ReleaseJump()
{
    ADMNPCCharacter* NPCCharacter = Cast<ADMNPCCharacter>(GetPawn());

    if (IsValid(NPCCharacter))
    {
        NPCCharacter->StopJumping();
    }
}

#pragma endregion

#pragma region Relic Approach

bool ADMJellyAIController::TrySelectRelicApproachTarget(AActor*& OutTarget) const
{
    OutTarget = nullptr;

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return false;
    }

    ADMMatchGameMode* MatchGameMode = World->GetAuthGameMode<ADMMatchGameMode>();

    if (!IsValid(MatchGameMode))
    {
        return false;
    }

    ADMRelicManager* RelicManager = MatchGameMode->GetRelicManager();

    if (!IsValid(RelicManager))
    {
        return false;
    }

    TArray<ADMRelicChest*> ActiveChests;

    TArray<ADMRelic*> WorldRelics;

    RelicManager->GetActiveRelicChests(ActiveChests);

    RelicManager->GetWorldVisibleRelics(WorldRelics);

    const int32 TotalTargetCount = ActiveChests.Num() + WorldRelics.Num();

    if (TotalTargetCount <= 0)
    {
        return false;
    }

    const int32 SelectedIndex = FMath::RandRange(0, TotalTargetCount - 1);

    if (SelectedIndex < ActiveChests.Num())
    {
        OutTarget = ActiveChests[SelectedIndex];
    }
    else
    {
        const int32 RelicIndex = SelectedIndex - ActiveChests.Num();

        OutTarget = WorldRelics[RelicIndex];
    }

    return IsRelicApproachTargetValid(OutTarget);
}

bool ADMJellyAIController::IsRelicApproachTargetValid(const AActor* Target) const
{
    if (!IsValid(Target))
    {
        return false;
    }

    const ADMRelicChest* Chest = Cast<ADMRelicChest>(Target);

    if (IsValid(Chest))
    {
        return Chest->IsBaseActive();
    }

    const ADMRelic* Relic = Cast<ADMRelic>(Target);

    if (!IsValid(Relic))
    {
        return false;
    }

    const EDMRelicWorldState WorldState = Relic->GetWorldState();

    return WorldState == EDMRelicWorldState::PublicSpawn || WorldState == EDMRelicWorldState::Dropped;
}

#pragma endregion

#pragma region Navigation

bool ADMJellyAIController::TryFindFleeDestination(
    const FVector& ThreatOrigin, const float FleeDistance, FVector& OutDestination
) const
{
    OutDestination = FVector::ZeroVector;

    if (ThreatOrigin.ContainsNaN() || FleeDistance <= 0.0f || !IsValid(GetPawn()))
    {
        return false;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return false;
    }

    UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

    if (!IsValid(NavigationSystem))
    {
        return false;
    }

    const FVector PawnLocation = GetPawn()->GetActorLocation();

    FVector AwayDirection = PawnLocation - ThreatOrigin;

    AwayDirection.Z = 0.0f;

    /*
     * 공격자와 NPC가 거의 같은 위치라면
     * 방향 벡터가 0이므로 임의 방향을 사용한다.
     */
    if (AwayDirection.IsNearlyZero())
    {
        const float RandomAngle = FMath::FRandRange(0.0f, 2.0f * PI);

        AwayDirection = FVector(FMath::Cos(RandomAngle), FMath::Sin(RandomAngle), 0.0f);
    }
    else
    {
        AwayDirection.Normalize();
    }

    const FVector DesiredLocation = PawnLocation + AwayDirection * FleeDistance;

    FNavLocation ProjectedLocation;

    /*
     * 우선 공격자의 정확한 반대 방향을 시도한다.
     */
    if (NavigationSystem->ProjectPointToNavigation(DesiredLocation, ProjectedLocation, FVector(300.0f, 300.0f, 500.0f)))
    {
        const float CurrentThreatDistanceSquared = FVector::DistSquared2D(PawnLocation, ThreatOrigin);

        const float ProjectedThreatDistanceSquared = FVector::DistSquared2D(ProjectedLocation.Location, ThreatOrigin);

        if (ProjectedThreatDistanceSquared > CurrentThreatDistanceSquared)
        {
            OutDestination = ProjectedLocation.Location;

            return true;
        }
    }

    /*
     * 직선 반대 방향이 NavMesh에 없으면
     * Reachable 후보 중 공격자에게서
     * 가장 멀어지는 점을 선택한다.
     */
    bool bFoundFallback = false;

    float BestThreatDistanceSquared = FVector::DistSquared2D(PawnLocation, ThreatOrigin);

    FVector BestLocation = FVector::ZeroVector;

    for (int32 SampleIndex = 0; SampleIndex < DMJellyAIControllerPrivate::FleeFallbackSampleCount; ++SampleIndex)
    {
        FNavLocation CandidateLocation;

        if (!NavigationSystem->GetRandomReachablePointInRadius(PawnLocation, FleeDistance, CandidateLocation))
        {
            continue;
        }

        const float ThreatDistanceSquared = FVector::DistSquared2D(CandidateLocation.Location, ThreatOrigin);

        if (ThreatDistanceSquared <= BestThreatDistanceSquared)
        {
            continue;
        }

        BestThreatDistanceSquared = ThreatDistanceSquared;

        BestLocation = CandidateLocation.Location;

        bFoundFallback = true;
    }

    if (!bFoundFallback)
    {
        return false;
    }

    OutDestination = BestLocation;

    return true;
}

bool ADMJellyAIController::TryFindRoamDestination(FVector& OutDestination) const
{
    OutDestination = FVector::ZeroVector;

    if (BehaviorProfile == nullptr || !IsValid(GetPawn()))
    {
        return false;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return false;
    }

    UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

    if (!IsValid(NavigationSystem))
    {
        return false;
    }

    const APawn* ControlledPawn = GetPawn();

    FNavLocation RandomLocation;

    const bool bFoundLocation = NavigationSystem->GetRandomReachablePointInRadius(
        ControlledPawn->GetActorLocation(), BehaviorProfile->RoamRadius, RandomLocation
    );

    if (!bFoundLocation)
    {
        return false;
    }

    OutDestination = RandomLocation.Location;

    return true;
}

bool ADMJellyAIController::TryStartNavigationMove(
    const FVector& Destination, const float AcceptanceRadius, const bool bSprint, const EDMNPCBehaviorState MovingState
)
{
    if (!HasAuthority())
    {
        return false;
    }

    ADMNPCCharacter* NPCCharacter = Cast<ADMNPCCharacter>(GetPawn());

    if (!IsValid(NPCCharacter))
    {
        return false;
    }

    if (Destination.ContainsNaN())
    {
        return false;
    }

    if (AcceptanceRadius < 0.0f)
    {
        return false;
    }

    if (CurrentBehaviorState != EDMNPCBehaviorState::Idle)
    {
        return false;
    }

    if (MovingState != EDMNPCBehaviorState::Moving && MovingState != EDMNPCBehaviorState::RelicApproachMoving
        && MovingState != EDMNPCBehaviorState::FleeMoving && MovingState != EDMNPCBehaviorState::PanicMoving)
    {
        return false;
    }

    UDMCharacterMovementComponent* MovementComponent =
        Cast<UDMCharacterMovementComponent>(NPCCharacter->GetCharacterMovement());

    if (MovementComponent == nullptr)
    {
        return false;
    }

    /*
     * 기존 CharacterMovement Sprint SSOT를 재사용한다.
     *
     * NPC 전용 MaxWalkSpeed나
     * 별도 bIsSprinting을 만들지 않는다.
     */
    MovementComponent->SetSprintRequested(bSprint);

    const EPathFollowingRequestResult::Type MoveResult =
        MoveToLocation(Destination, AcceptanceRadius, true, true, true, false, nullptr, false);

    switch (MoveResult)
    {
    case EPathFollowingRequestResult::RequestSuccessful:

        TransitionToBehaviorState(MovingState);

        return true;

    case EPathFollowingRequestResult::AlreadyAtGoal:

        NPCCharacter->ClearSprintRequest();

        TransitionToBehaviorState(EDMNPCBehaviorState::Idle);

        return true;

    case EPathFollowingRequestResult::Failed:
    default:

        NPCCharacter->ClearSprintRequest();

        TransitionToBehaviorState(EDMNPCBehaviorState::Idle);

        return false;
    }
}

void ADMJellyAIController::OnMoveCompleted(const FAIRequestID RequestID, const FPathFollowingResult& Result)
{
    Super::OnMoveCompleted(RequestID, Result);

    const EDMNPCBehaviorState CompletedMoveState = CurrentBehaviorState;

    if (CompletedMoveState != EDMNPCBehaviorState::Moving
        && CompletedMoveState != EDMNPCBehaviorState::RelicApproachMoving
        && CompletedMoveState != EDMNPCBehaviorState::FleeMoving
        && CompletedMoveState != EDMNPCBehaviorState::PanicMoving)
    {
        return;
    }

    ADMNPCCharacter* NPCCharacter = Cast<ADMNPCCharacter>(GetPawn());

    if (IsValid(NPCCharacter))
    {
        NPCCharacter->ClearSprintRequest();
    }

    TransitionToBehaviorState(EDMNPCBehaviorState::Idle);

    if (BehaviorProfile == nullptr)
    {
        return;
    }

    /*
     * =====================================
     * Global Panic 이동 완료
     * =====================================
     */
    if (CompletedMoveState == EDMNPCBehaviorState::PanicMoving)
    {
        if (!Result.IsSuccess())
        {
            UE_LOG(
                LogDMJellyAIController,
                Verbose,
                TEXT("NPC panic navigation finished without success. NPC=%s"),
                *GetNameSafe(GetPawn())
            );
        }

        const float PostPanicDelay = FMath::FRandRange(
            DMJellyAIControllerPrivate::PostMoveDelayMin, DMJellyAIControllerPrivate::PostMoveDelayMax
        );

        ScheduleNextBehavior(PostPanicDelay);

        return;
    }

    /*
     * =====================================
     * Wrong Target Flee 완료
     * =====================================
     */
    if (CompletedMoveState == EDMNPCBehaviorState::FleeMoving)
    {
        if (!Result.IsSuccess())
        {
            UE_LOG(
                LogDMJellyAIController,
                Verbose,
                TEXT("NPC flee navigation finished without success. NPC=%s"),
                *GetNameSafe(GetPawn())
            );
        }

        const float PostFleeDelay = FMath::FRandRange(
            DMJellyAIControllerPrivate::PostMoveDelayMin, DMJellyAIControllerPrivate::PostMoveDelayMax
        );

        ScheduleNextBehavior(PostFleeDelay);

        return;
    }

    /*
     * =====================================
     * Relic / Chest fake approach 완료
     * =====================================
     */
    if (CompletedMoveState == EDMNPCBehaviorState::RelicApproachMoving)
    {
        AActor* Target = CurrentRelicApproachTarget.Get();

        if (Result.IsSuccess() && IsRelicApproachTargetValid(Target))
        {
            /*
             * 실제 Interaction은 하지 않는다.
             *
             * 그냥 대상 앞에 일정 시간 머무른 뒤
             * 다시 일반 행동 선택으로 돌아간다.
             */
            ScheduleNextBehavior(BehaviorProfile->RelicApproachWaitDuration);
        }
        else
        {
            CurrentRelicApproachTarget.Reset();

            ScheduleNextBehavior(DMJellyAIControllerPrivate::NavigationRetryDelay);
        }

        return;
    }

    /*
     * =====================================
     * 일반 Walk / Sprint 완료
     * =====================================
     */
    if (!Result.IsSuccess())
    {
        UE_LOG(
            LogDMJellyAIController,
            Verbose,
            TEXT("NPC navigation finished without success. NPC=%s"),
            *GetNameSafe(GetPawn())
        );
    }

    const float PostMoveDelay =
        FMath::FRandRange(DMJellyAIControllerPrivate::PostMoveDelayMin, DMJellyAIControllerPrivate::PostMoveDelayMax);

    ScheduleNextBehavior(PostMoveDelay);
}

#pragma endregion

#pragma region State Machine

void ADMJellyAIController::TransitionToBehaviorState(const EDMNPCBehaviorState NewState)
{
    if (CurrentBehaviorState == NewState)
    {
        return;
    }

    CurrentBehaviorState = NewState;
}

#pragma endregion