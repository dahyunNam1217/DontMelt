#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Quests/DMQuestTypes.h"
#include "TimerManager.h"
#include "DMQuestProgressComponent.generated.h"

class UDMQuestProgressComponent;

DECLARE_MULTICAST_DELEGATE_TwoParams(FDMOnQuestProgressChanged, UDMQuestProgressComponent*, FDMQuestPlayerProgress);

/**
 * 플레이어 개인 Quest 진행 상태의 SSOT.
 *
 * MatchPlayerState가 소유하므로
 * Character가 사망/Respawn되어도 유지된다.
 *
 * Quest 종류 선택, 발생 시점, 완료 순위 판정은
 * QuestDirector의 책임이며 이 Component에서 수행하지 않는다.
 */
UCLASS(ClassGroup = (DontMelt))
class DONTMELT_API UDMQuestProgressComponent : public UActorComponent
{
    GENERATED_BODY()

public:
#pragma region Lifecycle

    UDMQuestProgressComponent();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion

#pragma region Query

    UFUNCTION(BlueprintPure, Category = "DM|Quest|Progress")
    FDMQuestPlayerProgress GetQuestProgress() const
    {
        return QuestProgress;
    }

    UFUNCTION(BlueprintPure, Category = "DM|Quest|Progress")
    int32 GetQuestRunId() const
    {
        return QuestProgress.RunId;
    }

    UFUNCTION(BlueprintPure, Category = "DM|Quest|Progress")
    float GetProgressValue() const
    {
        return QuestProgress.ProgressValue;
    }

    UFUNCTION(BlueprintPure, Category = "DM|Quest|Progress")
    EDMQuestParticipantState GetParticipantState() const
    {
        return QuestProgress.State;
    }

    UFUNCTION(BlueprintPure, Category = "DM|Quest|Progress")
    int32 GetCompletionRank() const
    {
        return QuestProgress.Rank;
    }

    UFUNCTION(BlueprintPure, Category = "DM|Quest|Progress")
    bool IsParticipating() const
    {
        return QuestProgress.State == EDMQuestParticipantState::Active
            || QuestProgress.State == EDMQuestParticipantState::FailedPendingCompletion;
    }

#pragma endregion

#pragma region Server Quest Run

    /**
     * 새로운 글로벌 Quest Run 참여를 시작한다.
     *
     * Server only.
     */
    bool BeginQuestRun(int32 NewRunId);

    /**
     * Count Quest의 진행도를 증가시킨다.
     *
     * TargetValue를 초과하지 않는다.
     * Target 도달 순간 CompletionServerTime을 기록한다.
     *
     * Server only.
     */
    bool AddCountProgress(int32 ExpectedRunId, float Amount, float TargetValue);

    /**
     * AccumulatedDuration 또는 ContinuousDuration
     * 행동 관찰을 시작한다.
     *
     * Server only.
     */
    bool BeginDurationObservation(int32 ExpectedRunId, EDMQuestProgressMode ProgressMode, float TargetValue);

    /**
     * 현재 Duration 행동 관찰을 끝내고
     * 실제 경과 시간을 Progress에 반영한다.
     *
     * Server only.
     */
    bool EndDurationObservation(int32 ExpectedRunId, EDMQuestProgressMode ProgressMode, float TargetValue);

    /**
     * 개인 Quest 상태를 완전히 초기화한다.
     *
     * Match 종료/초기화용.
     * Server only.
     */
    void ResetQuestProgress();

    /**
     * 제한시간 내 완료하지 못했지만
     * 남은 행동을 계속 수행해야 하는 상태로 변경한다.
     *
     * Server only.
     */
    bool MarkFailedPendingCompletion(int32 ExpectedRunId);

    /**
     * 제한시간 안에 TargetValue를 달성한 상태로 전환한다.
     *
     * Rank는 QuestDirector가
     * CompletionServerTime 기준으로 별도 확정한다.
     *
     * Server only.
     */
    bool MarkCompletedInTime(int32 ExpectedRunId);

    /**
     * 제한시간 내 완료된 Player의 확정 순위를 저장한다.
     *
     * 순위 자체의 결정은 QuestDirector가 담당한다.
     * Server only.
     */
    bool AssignCompletionRank(int32 ExpectedRunId, int32 CompletionRank);

    /**
     * 제한시간 실패 이후
     * 남은 행동까지 완료한 상태로 변경한다.
     *
     * Server only.
     */
    bool MarkCompletedAfterFailure(int32 ExpectedRunId);

    /**
     * 사망 등으로 현재 Quest 대상에서 제외한다.
     *
     * 이미 완료된 결과는 Excluded로 덮어쓰지 않는다.
     *
     * Server only.
     */
    bool MarkExcluded(int32 ExpectedRunId);

#pragma endregion

#pragma region Delegate

    FDMOnQuestProgressChanged& OnQuestProgressChanged()
    {
        return QuestProgressChangedDelegate;
    }

#pragma endregion

private:
#pragma region Validation

    bool HasServerAuthority() const;

    bool IsExpectedRun(int32 ExpectedRunId) const;

    bool IsProgressMutableState() const;

    double GetAuthoritativeServerTime() const;

#pragma endregion

#pragma region Duration Observation

    void ClearDurationObservation();

    void HandleDurationTargetReached(int32 ExpectedRunId, float TargetValue);

#pragma endregion

#pragma region Replication

    UFUNCTION()
    void OnRep_QuestProgress();

#pragma endregion

#pragma region Internal Notification

    void HandleQuestProgressChanged();

#pragma endregion

#pragma region Runtime State

    /**
     * 개인 Quest Progress의 유일한 Runtime 원본.
     *
     * 다른 플레이어에게 공개할 필요가 없으므로
     * OwnerOnly로 복제한다.
     */
    UPROPERTY(
        VisibleInstanceOnly,
        ReplicatedUsing = OnRep_QuestProgress,
        BlueprintReadOnly,
        Category = "DM|Quest|Progress",
        meta = (AllowPrivateAccess = "true")
    )
    FDMQuestPlayerProgress QuestProgress;

    /**
     * 현재 Duration 관찰이 Target에 도달하는
     * 논리적 서버 시각.
     *
     * 서버 내부 계산용이며 복제하지 않는다.
     */
    double ActiveDurationTargetServerTime = 0.0;

    /**
     * Duration Target 도달용 단발성 Server Timer.
     *
     * Tick을 사용하지 않는다.
     */
    FTimerHandle DurationTargetTimerHandle;

#pragma endregion

#pragma region Delegates

    FDMOnQuestProgressChanged QuestProgressChangedDelegate;

#pragma endregion
};