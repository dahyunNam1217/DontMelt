#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "TimerManager.h"
#include "DMJellyAIController.generated.h"

struct FDMNPCBehaviorProfileRow;
struct FPathFollowingResult;

/**
 * NPC AIController 내부 런타임 상태.
 *
 * Gameplay ActionState나
 * Character 이동 애니메이션 상태와는 별개다.
 */
enum class EDMNPCBehaviorState : uint8
{
    Idle,

    Waiting,

    Moving,

    RelicApproachMoving,

    /**
     * 기존 행동을 중단하고
     * Scream 후 Flee 시작을 기다리는 상태.
     */
    Reacting,

    /**
     * 오인 공격 위협으로부터
     * Sprint 도주 중.
     */
    FleeMoving,

    /**
     * Global Panic 명령을 받은 뒤
     * NPC별 Random Delay를 기다리는 상태.
     */
    PanicWaiting,

    /**
     * Panic 반응으로 Sprint 이동 중.
     */
    PanicMoving,

    /** 실제 플레이어 사망을 목격해 잠시 멈추고 비웃는 서버 전용 상태. */
    DeathMocking
};

/**
 * Jelly NPC 전용 AIController.
 *
 * 개별 NPC의 행동 선택과
 * Navigation 실행을 담당한다.
 */
UCLASS()
class DONTMELT_API ADMJellyAIController : public AAIController
{
    GENERATED_BODY()

public:
#pragma region Lifecycle Public

    ADMJellyAIController();

#pragma endregion

#pragma region External Reaction

    /**
     * PopulationDirector가 서버에서 호출하는
     * 오인 공격 반응 진입점.
     */
    void HandleWrongTargetReaction(const FVector& ThreatOrigin, float FleeDistance);

    /**
     * PopulationDirector가 Global Panic 발생 시
     * 서버에서 호출한다.
     */
    void HandlePanicReaction();

    /**
     * 사망 위치를 향해 멈춰 서는 비웃음 반응을 시작한다.
     * 위험·패닉·기존 비웃음 중이면 반응하지 않는다.
     */
    bool TryStartDeathMockingReaction(const FVector& DeathLocation, float DurationSeconds);

#pragma endregion

protected:
#pragma region Lifecycle Overrides

    virtual void OnPossess(APawn* InPawn) override;

    virtual void OnUnPossess() override;

#pragma endregion

#pragma region Navigation Callback

    virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

#pragma endregion

private:
#pragma region Behavior Profile

    /**
     * Possess한 NPC에서
     * Behavior DataTable Row를 확인한다.
     */
    bool ResolveBehaviorProfile();

#pragma endregion

#pragma region Behavior Loop

    /**
     * 일정 시간이 지난 뒤
     * 다음 행동 선택을 예약한다.
     */
    void ScheduleNextBehavior(float DelaySeconds);

    /**
     * 현재 구현된 기본 행동 중 하나를
     * DataTable Weight에 따라 선택한다.
     */
    void SelectNextBehavior();

    void StartWalkBehavior();

    void StartIdleBehavior();

    void StartSprintBehavior();

    void StartJumpBehavior();

    void StartRelicApproachBehavior();

    void ReleaseJump();

#pragma endregion

#pragma region Wrong Target Reaction

    /**
     * Scream Lead-in이 끝난 뒤
     * 실제 Flee Navigation을 시작한다.
     */
    void StartPendingFleeReaction();

    /**
     * 공격 위치의 반대 방향에서
     * NavMesh 위 Flee 목적지를 계산한다.
     */
    bool TryFindFleeDestination(const FVector& ThreatOrigin, float FleeDistance, FVector& OutDestination) const;

#pragma endregion

#pragma region Panic Reaction

    /**
     * NPC별 Random Delay가 끝난 뒤
     * 실제 Panic 행동을 시작한다.
     */
    void StartPendingPanicReaction();

#pragma endregion

#pragma region Death Mocking Reaction

    /** 비웃음 반응이 끝나면 기존 일반 행동 스케줄로 복귀한다. */
    void FinishDeathMockingReaction();

#pragma endregion

#pragma region Relic Approach

    /**
     * 현재 RelicManager가 관리 중인
     * 유효한 Chest / Relic 중 하나를 선택한다.
     */
    bool TrySelectRelicApproachTarget(AActor*& OutTarget) const;

    /**
     * 이동 도중 Target의 게임 상태가 바뀌었는지
     * 최종 검증한다.
     */
    bool IsRelicApproachTargetValid(const AActor* Target) const;

#pragma endregion

#pragma region Navigation

    /**
     * 현재 NPC 주변 NavMesh에서
     * 이동 가능한 임시 Roaming 목적지를 찾는다.
     */
    bool TryFindRoamDestination(FVector& OutDestination) const;

    /**
     * 하나의 Nav 이동을 시작한다.
     *
     * bSprint는 MovementComponent의
     * 기존 Sprint 요청 구조를 사용한다.
     */
    bool TryStartNavigationMove(
        const FVector& Destination, float AcceptanceRadius, bool bSprint, EDMNPCBehaviorState MovingState
    );

#pragma endregion

#pragma region State Machine

    void TransitionToBehaviorState(EDMNPCBehaviorState NewState);

#pragma endregion

#pragma region Runtime State

    /**
     * 서버 전용 AI 상태.
     * Replication하지 않는다.
     */
    EDMNPCBehaviorState CurrentBehaviorState = EDMNPCBehaviorState::Idle;

    /**
     * DataTable Row가 실제 원본이며
     * Controller는 런타임 동안 읽기 전용으로 참조한다.
     */
    const FDMNPCBehaviorProfileRow* BehaviorProfile = nullptr;

    /**
     * 현재 RelicApproach 행동에서만 사용하는
     * 서버 전용 임시 Target.
     *
     * 실제 Relic / Chest 상태의 소유자가 아니다.
     */
    TWeakObjectPtr<AActor> CurrentRelicApproachTarget;

    /**
     * Reacting 상태에서 다음 Flee 시작까지
     * 일시적으로 보관하는 Threat 위치.
     */
    FVector PendingFleeThreatOrigin = FVector::ZeroVector;

    /**
     * 현재 Reaction Event가 요청한
     * 도주 거리.
     */
    float PendingFleeDistance = 0.0f;

    /**
     * 다음 행동 선택용 Timer.
     */
    FTimerHandle BehaviorTimerHandle;

    /**
     * Jump 입력 해제용 단발 Timer.
     */
    FTimerHandle JumpReleaseTimerHandle;

    /**
     * 위협 반응·패닉 지연·비웃음 종료에 사용하는
     * 기존 단발 Reaction Timer.
     */
    FTimerHandle ReactionTimerHandle;

#pragma endregion
};