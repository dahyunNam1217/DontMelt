#include "Quests/DMQuestProgressComponent.h"

#include "Core/DMMatchConstants.h"

#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogDMQuestProgress, Log, All);

#pragma region Lifecycle

UDMQuestProgressComponent::UDMQuestProgressComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    SetIsReplicatedByDefault(true);
}

void UDMQuestProgressComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION(UDMQuestProgressComponent, QuestProgress, COND_OwnerOnly);
}

#pragma endregion

#pragma region Server Quest Run

bool UDMQuestProgressComponent::BeginQuestRun(const int32 NewRunId)
{
    if (!HasServerAuthority())
    {
        return false;
    }

    if (!ensureMsgf(
            NewRunId > 0,
            TEXT("[%s] BeginQuestRun rejected. RunId must be positive. Requested=%d"),
            *GetNameSafe(GetOwner()),
            NewRunId
        ))
    {
        return false;
    }

    /*
     * 동일 Run을 두 번 초기화해서
     * 진행도를 날리는 실수를 방지한다.
     */
    if (QuestProgress.RunId == NewRunId && QuestProgress.State != EDMQuestParticipantState::Inactive)
    {
        return false;
    }

    ClearDurationObservation();

    QuestProgress.RunId = NewRunId;

    QuestProgress.ProgressValue = 0.0f;

    QuestProgress.ObservationStartServerTime = 0.0;

    QuestProgress.CompletionServerTime = 0.0;

    QuestProgress.State = EDMQuestParticipantState::Active;

    QuestProgress.Rank = 0;

    HandleQuestProgressChanged();

    return true;
}

bool UDMQuestProgressComponent::AddCountProgress(const int32 ExpectedRunId, const float Amount, const float TargetValue)
{
    if (!HasServerAuthority() || !IsExpectedRun(ExpectedRunId) || !IsProgressMutableState())
    {
        return false;
    }

    if (Amount <= 0.0f || TargetValue <= 0.0f)
    {
        return false;
    }

    if (QuestProgress.ProgressValue >= TargetValue)
    {
        return false;
    }

    const float NewProgressValue = FMath::Min(TargetValue, QuestProgress.ProgressValue + Amount);

    if (FMath::IsNearlyEqual(NewProgressValue, QuestProgress.ProgressValue))
    {
        return false;
    }

    const bool bReachedTarget = NewProgressValue >= TargetValue;

    QuestProgress.ProgressValue = NewProgressValue;

    if (bReachedTarget && QuestProgress.CompletionServerTime <= 0.0)
    {
        QuestProgress.CompletionServerTime = GetAuthoritativeServerTime();
    }

    HandleQuestProgressChanged();

    return true;
}

bool UDMQuestProgressComponent::BeginDurationObservation(
    const int32 ExpectedRunId, const EDMQuestProgressMode ProgressMode, const float TargetValue
)
{
    if (!HasServerAuthority() || !IsExpectedRun(ExpectedRunId) || !IsProgressMutableState())
    {
        return false;
    }

    if (ProgressMode == EDMQuestProgressMode::Count || TargetValue <= 0.0f)
    {
        return false;
    }

    /*
     * 중복 Start Event 방어.
     */
    if (QuestProgress.ObservationStartServerTime > 0.0)
    {
        return false;
    }

    if (QuestProgress.ProgressValue >= TargetValue)
    {
        return false;
    }

    const double ServerNow = GetAuthoritativeServerTime();

    if (ServerNow <= 0.0)
    {
        return false;
    }

    float RemainingDuration = TargetValue;

    if (ProgressMode == EDMQuestProgressMode::AccumulatedDuration)
    {
        RemainingDuration = FMath::Max(0.0f, TargetValue - QuestProgress.ProgressValue);
    }
    else
    {
        /*
         * ContinuousDuration은 새 Start마다
         * 처음부터 연속 유지해야 한다.
         */
        QuestProgress.ProgressValue = 0.0f;
    }

    if (RemainingDuration <= KINDA_SMALL_NUMBER)
    {
        QuestProgress.ProgressValue = TargetValue;

        QuestProgress.CompletionServerTime = ServerNow;

        HandleQuestProgressChanged();

        return true;
    }

    QuestProgress.ObservationStartServerTime = ServerNow;

    ActiveDurationTargetServerTime = ServerNow + static_cast<double>(RemainingDuration);

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        QuestProgress.ObservationStartServerTime = 0.0;

        ActiveDurationTargetServerTime = 0.0;

        return false;
    }

    FTimerDelegate TargetTimerDelegate;

    TargetTimerDelegate.BindUObject(
        this, &UDMQuestProgressComponent::HandleDurationTargetReached, ExpectedRunId, TargetValue
    );

    World->GetTimerManager().SetTimer(DurationTargetTimerHandle, TargetTimerDelegate, RemainingDuration, false);

    if (!World->GetTimerManager().IsTimerActive(DurationTargetTimerHandle))
    {
        QuestProgress.ObservationStartServerTime = 0.0;

        ActiveDurationTargetServerTime = 0.0;

        return false;
    }

    /*
     * Owner HUD가 locally elapsed를 계산할 수 있도록
     * 관찰 시작 시각을 한 번 복제한다.
     */
    HandleQuestProgressChanged();

    return true;
}

bool UDMQuestProgressComponent::EndDurationObservation(
    const int32 ExpectedRunId, const EDMQuestProgressMode ProgressMode, const float TargetValue
)
{
    if (!HasServerAuthority() || !IsExpectedRun(ExpectedRunId) || !IsProgressMutableState())
    {
        return false;
    }

    if (ProgressMode == EDMQuestProgressMode::Count || TargetValue <= 0.0f)
    {
        return false;
    }

    const double ObservationStartTime = QuestProgress.ObservationStartServerTime;

    if (ObservationStartTime <= 0.0)
    {
        return false;
    }

    const double ServerNow = GetAuthoritativeServerTime();

    if (ServerNow <= 0.0)
    {
        return false;
    }

    const double TargetServerTime = ActiveDurationTargetServerTime;

    const double ElapsedSeconds = FMath::Max(0.0, ServerNow - ObservationStartTime);

    const bool bReachedTarget = TargetServerTime > 0.0 && ServerNow + KINDA_SMALL_NUMBER >= TargetServerTime;

    /*
     * Timer와 관찰 상태부터 정리한 뒤
     * Progress 결과를 Commit한다.
     */
    ClearDurationObservation();

    if (ProgressMode == EDMQuestProgressMode::AccumulatedDuration)
    {
        if (bReachedTarget)
        {
            QuestProgress.ProgressValue = TargetValue;
        }
        else
        {
            QuestProgress.ProgressValue =
                FMath::Min(TargetValue, QuestProgress.ProgressValue + static_cast<float>(ElapsedSeconds));
        }
    }
    else
    {
        /*
         * ContinuousDuration:
         *
         * 목표시간 전에 행동이 끊기면
         * 누적하지 않고 다시 0부터 시작한다.
         */
        QuestProgress.ProgressValue = bReachedTarget ? TargetValue : 0.0f;
    }

    if (QuestProgress.ProgressValue >= TargetValue && QuestProgress.CompletionServerTime <= 0.0)
    {
        /*
         * Timer callback이 약간 늦게 실행되더라도
         * Rank 판정에는 실제 논리적 완료시각을 사용한다.
         */
        QuestProgress.CompletionServerTime = TargetServerTime > 0.0 ? TargetServerTime : ServerNow;
    }

    HandleQuestProgressChanged();

    return true;
}

void UDMQuestProgressComponent::ResetQuestProgress()
{
    if (!HasServerAuthority())
    {
        return;
    }

    const bool bAlreadyReset = QuestProgress.RunId == 0 && FMath::IsNearlyZero(QuestProgress.ProgressValue)
        && QuestProgress.ObservationStartServerTime <= 0.0 && QuestProgress.CompletionServerTime <= 0.0
        && QuestProgress.State == EDMQuestParticipantState::Inactive && QuestProgress.Rank == 0;

    if (bAlreadyReset)
    {
        return;
    }

    ClearDurationObservation();

    QuestProgress = FDMQuestPlayerProgress();

    HandleQuestProgressChanged();
}

bool UDMQuestProgressComponent::MarkFailedPendingCompletion(const int32 ExpectedRunId)
{
    if (!HasServerAuthority() || !IsExpectedRun(ExpectedRunId))
    {
        return false;
    }

    if (QuestProgress.State != EDMQuestParticipantState::Active)
    {
        return false;
    }

    QuestProgress.State = EDMQuestParticipantState::FailedPendingCompletion;

    QuestProgress.Rank = 0;

    HandleQuestProgressChanged();

    return true;
}

bool UDMQuestProgressComponent::MarkCompletedInTime(const int32 ExpectedRunId)
{
    if (!HasServerAuthority() || !IsExpectedRun(ExpectedRunId))
    {
        return false;
    }

    /*
     * 보통 Active에서 완료하지만,
     *
     * Server hitch 때문에 Duration 목표 Timer가
     * Deadline Timer보다 늦게 dispatch됐더라도
     * CompletionServerTime이 Deadline 이전이면
     * Director가 InTime으로 복구할 수 있어야 한다.
     */
    const bool bCanCompleteInTime = QuestProgress.State == EDMQuestParticipantState::Active
        || QuestProgress.State == EDMQuestParticipantState::FailedPendingCompletion;

    if (!bCanCompleteInTime)
    {
        return false;
    }

    ClearDurationObservation();

    QuestProgress.State = EDMQuestParticipantState::CompletedInTime;

    /*
     * Rank는 UDMQuestDirector::FinalizeCompletionRanks()에서 별도 확정한다.
     */
    QuestProgress.Rank = 0;

    HandleQuestProgressChanged();

    return true;
}

bool UDMQuestProgressComponent::AssignCompletionRank(const int32 ExpectedRunId, const int32 CompletionRank)
{
    if (!HasServerAuthority() || !IsExpectedRun(ExpectedRunId))
    {
        return false;
    }

    if (QuestProgress.State != EDMQuestParticipantState::CompletedInTime)
    {
        return false;
    }

    if (!FMath::IsWithinInclusive(CompletionRank, 1, DMMatch::MaxPlayers))
    {
        return false;
    }

    /*
     * Rank는 한 Run에서 한 번만 Commit한다.
     */
    if (QuestProgress.Rank != 0)
    {
        return false;
    }

    QuestProgress.Rank = CompletionRank;

    HandleQuestProgressChanged();

    return true;
}

bool UDMQuestProgressComponent::MarkCompletedAfterFailure(const int32 ExpectedRunId)
{
    if (!HasServerAuthority() || !IsExpectedRun(ExpectedRunId))
    {
        return false;
    }

    if (QuestProgress.State != EDMQuestParticipantState::FailedPendingCompletion)
    {
        return false;
    }

    ClearDurationObservation();

    QuestProgress.State = EDMQuestParticipantState::CompletedAfterFailure;

    /*
     * 실패 이후 완료에는
     * 정상 Completion Rank를 부여하지 않는다.
     */
    QuestProgress.Rank = 0;

    HandleQuestProgressChanged();

    return true;
}

bool UDMQuestProgressComponent::MarkExcluded(const int32 ExpectedRunId)
{
    if (!HasServerAuthority() || !IsExpectedRun(ExpectedRunId))
    {
        return false;
    }

    /*
     * 완료 결과까지 얻은 플레이어가 이후 죽었다고 해서
     * 완료 결과를 Excluded로 덮어쓰지 않는다.
     *
     * 아직 진행 중인 플레이어만 제외할 수 있다.
     */
    const bool bCanExclude = QuestProgress.State == EDMQuestParticipantState::Active
        || QuestProgress.State == EDMQuestParticipantState::FailedPendingCompletion;

    if (!bCanExclude)
    {
        return false;
    }

    ClearDurationObservation();

    QuestProgress.State = EDMQuestParticipantState::Excluded;

    QuestProgress.Rank = 0;

    HandleQuestProgressChanged();

    return true;
}

#pragma endregion

#pragma region Validation

bool UDMQuestProgressComponent::HasServerAuthority() const
{
    const AActor* OwnerActor = GetOwner();

    return OwnerActor != nullptr && OwnerActor->HasAuthority();
}

bool UDMQuestProgressComponent::IsExpectedRun(const int32 ExpectedRunId) const
{
    return ExpectedRunId > 0 && QuestProgress.RunId == ExpectedRunId;
}

bool UDMQuestProgressComponent::IsProgressMutableState() const
{
    return QuestProgress.State == EDMQuestParticipantState::Active
        || QuestProgress.State == EDMQuestParticipantState::FailedPendingCompletion;
}

double UDMQuestProgressComponent::GetAuthoritativeServerTime() const
{
    const UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return 0.0;
    }

    const AGameStateBase* GameState = World->GetGameState();

    if (IsValid(GameState))
    {
        return GameState->GetServerWorldTimeSeconds();
    }

    return static_cast<double>(World->GetTimeSeconds());
}

#pragma endregion

#pragma region Duration Observation

void UDMQuestProgressComponent::ClearDurationObservation()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DurationTargetTimerHandle);
    }

    QuestProgress.ObservationStartServerTime = 0.0;

    ActiveDurationTargetServerTime = 0.0;
}

void UDMQuestProgressComponent::HandleDurationTargetReached(const int32 ExpectedRunId, const float TargetValue)
{
    if (!HasServerAuthority() || !IsExpectedRun(ExpectedRunId) || !IsProgressMutableState() || TargetValue <= 0.0f
        || QuestProgress.ObservationStartServerTime <= 0.0)
    {
        return;
    }

    const double CompletionTime =
        ActiveDurationTargetServerTime > 0.0 ? ActiveDurationTargetServerTime : GetAuthoritativeServerTime();

    QuestProgress.ProgressValue = TargetValue;

    QuestProgress.CompletionServerTime = CompletionTime;

    ClearDurationObservation();

    HandleQuestProgressChanged();
}

#pragma endregion

#pragma region Replication

void UDMQuestProgressComponent::OnRep_QuestProgress()
{
    HandleQuestProgressChanged();
}

#pragma endregion

#pragma region Internal Notification

void UDMQuestProgressComponent::HandleQuestProgressChanged()
{
    QuestProgressChangedDelegate.Broadcast(this, QuestProgress);

    /*
     * Server에서 상태 변경 직후
     * Owner에게 Projection이 빠르게 전달되도록 한다.
     *
     * Client OnRep에서 호출될 때는
     * Authority가 아니므로 ForceNetUpdate하지 않는다.
     */
    AActor* OwnerActor = GetOwner();

    if (OwnerActor != nullptr && OwnerActor->HasAuthority())
    {
        OwnerActor->ForceNetUpdate();
    }
}

#pragma endregion