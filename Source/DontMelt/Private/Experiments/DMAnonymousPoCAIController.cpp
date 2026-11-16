#include "Experiments/DMAnonymousPoCAIController.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

ADMAnonymousPoCAIController::ADMAnonymousPoCAIController()
{
    bReplicates = false;
    bWantsPlayerState = false;
}

void ADMAnonymousPoCAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    bStopping = false;
    ScheduleMove();
}

void ADMAnonymousPoCAIController::ScheduleMove()
{
    if (HasAuthority() && !bStopping && IsValid(GetPawn()))
    {
        GetWorldTimerManager().SetTimer(MoveTimer, this, &ThisClass::StartMove, 1.0f, false);
    }
}

void ADMAnonymousPoCAIController::StartMove()
{
    if (!HasAuthority() || bStopping || !IsValid(GetPawn()))
    {
        return;
    }
    UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    FNavLocation Destination;
    if (!IsValid(Navigation)
        || !Navigation->GetRandomReachablePointInRadius(GetPawn()->GetActorLocation(), 500.0f, Destination))
    {
        ScheduleMove();
        return;
    }
    if (MoveToLocation(Destination.Location, 35.0f) != EPathFollowingRequestResult::RequestSuccessful)
    {
        ScheduleMove();
    }
}

void ADMAnonymousPoCAIController::OnMoveCompleted(const FAIRequestID RequestID, const FPathFollowingResult& Result)
{
    Super::OnMoveCompleted(RequestID, Result);
    ScheduleMove();
}

void ADMAnonymousPoCAIController::OnUnPossess()
{
    // 이동 중단 콜백이 다음 이동을 예약하기 전에 종료 상태를 확정한다.
    bStopping = true;
    GetWorldTimerManager().ClearTimer(MoveTimer);
    StopMovement();
    Super::OnUnPossess();
}

void ADMAnonymousPoCAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bStopping = true;
    GetWorldTimerManager().ClearTimer(MoveTimer);
    StopMovement();
    Super::EndPlay(EndPlayReason);
}
