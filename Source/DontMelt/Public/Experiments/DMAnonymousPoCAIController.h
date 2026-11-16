#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "TimerManager.h"
#include "DMAnonymousPoCAIController.generated.h"

/** 기존 행동 DataAsset 없이 Nav 이동 경로만 확인하는 서버 전용 실험 AI. */
UCLASS()
class DONTMELT_API ADMAnonymousPoCAIController : public AAIController
{
    GENERATED_BODY()

public:
    ADMAnonymousPoCAIController();

protected:
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

private:
    void ScheduleMove();
    void StartMove();
    FTimerHandle MoveTimer;
    bool bStopping = false;
};
