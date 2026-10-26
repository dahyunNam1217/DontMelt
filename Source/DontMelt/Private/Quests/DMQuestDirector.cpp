#include "Quests/DMQuestDirector.h"

#include "Core/DMGameplayUtils.h"
#include "Core/DMMatchConstants.h"

#include "Data/DMMatchRuleDataAsset.h"
#include "Framework/Match/DMMatchGameMode.h"
#include "Framework/Match/DMMatchGameState.h"
#include "Framework/Match/DMMatchPlayerState.h"
#include "Characters/DMPlayerCharacter.h"
#include "Core/DMGameplayTags.h"
#include "Quests/DMQuestProgressComponent.h"

#include "Engine/DataTable.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogDMQuestDirector, Log, All);

#pragma region Lifecycle

bool UDMQuestDirector::Initialize(
    ADMMatchGameMode* InOwnerGameMode,
    const UDMMatchRuleDataAsset* InMatchRuleData,
    UDataTable* InQuestDefinitionTable,
    FDMQuestPublicStatePublisher InPublicStatePublisher
)
{
    if (!ensureMsgf(
            IsValid(InOwnerGameMode) && InOwnerGameMode->HasAuthority(),
            TEXT("Initialize failed: valid authoritative GameMode is required.")
        ))
    {
        return false;
    }

    if (!ensureMsgf(InPublicStatePublisher.IsBound(), TEXT("Initialize failed: public state publisher is not bound.")))
    {
        return false;
    }

    if (!ensureMsgf(
            IsValid(InMatchRuleData) && IsValid(InQuestDefinitionTable),
            TEXT("Initialize failed: MatchRuleData or QuestDefinitionTable is unavailable.")
        ))
    {
        return false;
    }

    if (!ensureMsgf(
            FMath::IsWithinInclusive(InMatchRuleData->QuestMinCount, 2, 3)
                && FMath::IsWithinInclusive(InMatchRuleData->QuestMaxCount, 2, 3)
                && InMatchRuleData->QuestMinCount <= InMatchRuleData->QuestMaxCount,
            TEXT("Initialize failed: invalid Quest count range. Min=%d, Max=%d"),
            InMatchRuleData->QuestMinCount,
            InMatchRuleData->QuestMaxCount
        ))
    {
        return false;
    }

    if (!ensureMsgf(
            InMatchRuleData->QuestAttemptTimes.Num() >= InMatchRuleData->QuestMaxCount,
            TEXT(
                "Initialize failed: QuestAttemptTimes must contain at least QuestMaxCount entries. Times=%d, MaxCount=%d"
            ),
            InMatchRuleData->QuestAttemptTimes.Num(),
            InMatchRuleData->QuestMaxCount
        ))
    {
        return false;
    }

    if (InMatchRuleData->MatchDuration <= 0.0f)
    {
        return false;
    }

    float PreviousAttemptTime = -1.0f;

    for (int32 Index = 0; Index < InMatchRuleData->QuestMaxCount; ++Index)
    {
        const float AttemptTime = InMatchRuleData->QuestAttemptTimes[Index];

        if (!ensureMsgf(
                AttemptTime > 0.0f && AttemptTime < InMatchRuleData->MatchDuration && AttemptTime > PreviousAttemptTime,
                TEXT("Initialize failed: invalid QuestAttemptTime. Index=%d, Time=%.3f"),
                Index,
                AttemptTime
            ))
        {
            return false;
        }

        PreviousAttemptTime = AttemptTime;
    }

    OwnerGameMode = InOwnerGameMode;

    PublicStatePublisher = MoveTemp(InPublicStatePublisher);

    QuestDefinitionTable = InQuestDefinitionTable;

    QuestMinCount = InMatchRuleData->QuestMinCount;

    QuestMaxCount = InMatchRuleData->QuestMaxCount;

    MatchDuration = InMatchRuleData->MatchDuration;

    QuestAttemptTimes = InMatchRuleData->QuestAttemptTimes;

    if (!CacheQuestDefinitions())
    {
        return false;
    }

    if (!BindPlayerDelegates())
    {
        UnbindPlayerDelegates();

        return false;
    }

    ActiveQuestState = FDMQuestRuntimeState();

    ActiveDefinitionIndex = INDEX_NONE;

    ActiveParticipants.Reset();

    ResolvedQuestAttemptCount = 0;
    StartedQuestCount = 0;
    NextRunId = 1;

    UE_LOG(
        LogDMQuestDirector,
        Log,
        TEXT("QuestDirector initialized. Definitions=%d, QuestCountRange=%d~%d"),
        CachedQuestDefinitions.Num(),
        QuestMinCount,
        QuestMaxCount
    );

    return true;
}

bool UDMQuestDirector::StartPlaying()
{
    ADMMatchGameMode* MatchGameMode = OwnerGameMode.Get();

    ADMMatchGameState* MatchGameState = GetMatchGameState();

    if (!IsValid(MatchGameMode) || !MatchGameMode->HasAuthority() || !IsValid(MatchGameState))
    {
        return false;
    }

    if (MatchGameState->GetCurrentPhase() != EDMMatchPhase::Playing)
    {
        UE_LOG(LogDMQuestDirector, Warning, TEXT("StartPlaying rejected: Match phase is not Playing."));

        return false;
    }

    if (ResolvedQuestAttemptCount > 0 || QuestAttemptTimerHandles.Num() > 0)
    {
        UE_LOG(LogDMQuestDirector, Warning, TEXT("StartPlaying rejected: Quest schedule already started."));

        return false;
    }

    ResolvedQuestAttemptCount = FMath::RandRange(QuestMinCount, QuestMaxCount);

    UWorld* World = MatchGameMode->GetWorld();

    if (!IsValid(World))
    {
        return false;
    }

    QuestAttemptTimerHandles.Reserve(ResolvedQuestAttemptCount);

    for (int32 AttemptIndex = 0; AttemptIndex < ResolvedQuestAttemptCount; ++AttemptIndex)
    {
        const float AttemptDelay = QuestAttemptTimes[AttemptIndex];

        FTimerHandle TimerHandle;

        FTimerDelegate TimerDelegate;

        TimerDelegate.BindUObject(this, &UDMQuestDirector::HandleQuestAttempt, AttemptIndex);

        World->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, AttemptDelay, false);

        if (!World->GetTimerManager().IsTimerActive(TimerHandle))
        {
            UE_LOG(
                LogDMQuestDirector, Error, TEXT("Quest attempt timer registration failed. Attempt=%d"), AttemptIndex
            );

            ClearQuestAttemptTimers();

            ResolvedQuestAttemptCount = 0;

            return false;
        }

        QuestAttemptTimerHandles.Add(TimerHandle);
    }

    UE_LOG(LogDMQuestDirector, Log, TEXT("Quest schedule started. ResolvedAttempts=%d"), ResolvedQuestAttemptCount);

    return true;
}

void UDMQuestDirector::Shutdown()
{
    StopAllFailureWarnings();

    ADMMatchGameMode* MatchGameMode = OwnerGameMode.Get();

    if (IsValid(MatchGameMode) && MatchGameMode->HasAuthority())
    {
        ClearQuestAttemptTimers();

        if (UWorld* World = MatchGameMode->GetWorld())
        {
            World->GetTimerManager().ClearTimer(QuestDeadlineTimerHandle);
        }

        if (ADMMatchGameState* MatchGameState = GetMatchGameState())
        {
            for (ADMMatchPlayerState* MatchPlayerState : DMGameplay::GetMatchPlayerStates(MatchGameState))
            {
                if (UDMQuestProgressComponent* QuestProgressComponent = MatchPlayerState->GetQuestProgressComponent())
                {
                    QuestProgressComponent->ResetQuestProgress();
                }
            }
        }

        PublishPublicState(FDMQuestRuntimeState());
    }

    UnbindPlayerDelegates();

    ActiveQuestState = FDMQuestRuntimeState();

    ActiveDefinitionIndex = INDEX_NONE;

    ActiveParticipants.Reset();

    ResolvedQuestAttemptCount = 0;
    StartedQuestCount = 0;

    UE_LOG(LogDMQuestDirector, Log, TEXT("QuestDirector shutdown completed."));
}

#pragma endregion

#pragma region Initialization

bool UDMQuestDirector::CacheQuestDefinitions()
{
    if (!IsValid(QuestDefinitionTable))
    {
        return false;
    }

    if (!ensureMsgf(
            QuestDefinitionTable->GetRowStruct() == FDMQuestDefinitionRow::StaticStruct(),
            TEXT("Quest DataTable row struct must be FDMQuestDefinitionRow.")
        ))
    {
        return false;
    }

    TArray<FDMQuestDefinitionRow*> SourceRows;

    QuestDefinitionTable->GetAllRows<FDMQuestDefinitionRow>(TEXT("DMQuestDirector"), SourceRows);

    if (!ensureMsgf(!SourceRows.IsEmpty(), TEXT("Quest DataTable has no rows.")))
    {
        return false;
    }

    TSet<FGameplayTag> FoundQuestTags;

    CachedQuestDefinitions.Reset();

    CachedQuestDefinitions.Reserve(SourceRows.Num());

    for (const FDMQuestDefinitionRow* Definition : SourceRows)
    {
        if (Definition == nullptr || !ValidateQuestDefinition(*Definition))
        {
            CachedQuestDefinitions.Reset();

            return false;
        }

        if (!ensureMsgf(
                !FoundQuestTags.Contains(Definition->QuestTag),
                TEXT("Duplicate QuestTag in Quest DataTable: %s"),
                *Definition->QuestTag.ToString()
            ))
        {
            CachedQuestDefinitions.Reset();

            return false;
        }

        FoundQuestTags.Add(Definition->QuestTag);

        CachedQuestDefinitions.Add(*Definition);
    }

    return !CachedQuestDefinitions.IsEmpty();
}

bool UDMQuestDirector::ValidateQuestDefinition(const FDMQuestDefinitionRow& Definition) const
{
    if (!ensureMsgf(
            Definition.QuestTag.IsValid() && Definition.StartEventTag.IsValid() && Definition.TargetValue > 0.0f
                && Definition.TimeLimit > 0.0f,
            TEXT("Invalid Quest definition. Quest=%s"),
            *Definition.QuestTag.ToString()
        ))
    {
        return false;
    }

    if (!ensureMsgf(
            !(Definition.ProgressMode != EDMQuestProgressMode::Count && !Definition.EndEventTag.IsValid()),
            TEXT("Duration Quest requires a valid EndEventTag. Quest=%s"),
            *Definition.QuestTag.ToString()
        ))
    {
        return false;
    }

    if (!ensureMsgf(
            !(Definition.ProgressMode != EDMQuestProgressMode::Count
              && Definition.StartEventTag == Definition.EndEventTag),
            TEXT("Duration Quest requires different Start/End EventTags. Quest=%s"),
            *Definition.QuestTag.ToString()
        ))
    {
        return false;
    }

    const bool bUsesPoseEvents = Definition.StartEventTag == DMGameplayTags::QuestEvent_Pose_Start
        || Definition.EndEventTag == DMGameplayTags::QuestEvent_Pose_End;

    if (!ensureMsgf(
            !(bUsesPoseEvents && Definition.RequiredPose == EDMPoseType::Default),
            TEXT("Pose Quest requires a non-default RequiredPose. Quest=%s"),
            *Definition.QuestTag.ToString()
        ))
    {
        return false;
    }

    // 사이렌 사운드와 보상 수치는 더 이상 퀘스트 정의에 포함하지 않는다.
    return true;
}

bool UDMQuestDirector::BindPlayerDelegates()
{
    ADMMatchGameState* MatchGameState = GetMatchGameState();

    if (!IsValid(MatchGameState))
    {
        return false;
    }

    if (!DMMatch::IsValidPlayerCount(MatchGameState->PlayerArray.Num()))
    {
        return false;
    }

    for (APlayerState* PlayerState : MatchGameState->PlayerArray)
    {
        ADMMatchPlayerState* MatchPlayerState = Cast<ADMMatchPlayerState>(PlayerState);

        if (!IsValid(MatchPlayerState))
        {
            return false;
        }

        UDMQuestProgressComponent* QuestProgressComponent = MatchPlayerState->GetQuestProgressComponent();

        if (!IsValid(QuestProgressComponent))
        {
            return false;
        }

        MatchPlayerState->OnLifeStateChanged().RemoveAll(this);

        MatchPlayerState->OnLifeStateChanged().AddUObject(this, &UDMQuestDirector::HandlePlayerLifeStateChanged);

        QuestProgressComponent->OnQuestProgressChanged().RemoveAll(this);

        QuestProgressComponent->OnQuestProgressChanged().AddUObject(
            this, &UDMQuestDirector::HandleParticipantQuestProgressChanged
        );

        if (MatchPlayerState->GetLifeState() == EDMPlayerLifeState::Alive && !BindQuestActionSource(MatchPlayerState))
        {
            return false;
        }
    }

    return true;
}

bool UDMQuestDirector::BindQuestActionSource(ADMMatchPlayerState* MatchPlayerState)
{
    if (!IsValid(MatchPlayerState))
    {
        return false;
    }

    ADMPlayerCharacter* PlayerCharacter = MatchPlayerState->GetPawn<ADMPlayerCharacter>();

    if (!IsValid(PlayerCharacter) || !PlayerCharacter->HasAuthority())
    {
        return false;
    }

    /*
     * 중복 Bind 방지.
     */
    PlayerCharacter->OnQuestActionEvent().RemoveAll(this);

    /*
     * FDMQuestActionEvent는 Delegate 원래 인자,
     * MatchPlayerState는 Bound Payload.
     */
    PlayerCharacter->OnQuestActionEvent().AddUObject(this, &UDMQuestDirector::HandleQuestActionEvent, MatchPlayerState);

    return true;
}

void UDMQuestDirector::UnbindQuestActionSource(ADMMatchPlayerState* MatchPlayerState)
{
    if (!IsValid(MatchPlayerState))
    {
        return;
    }

    if (ADMPlayerCharacter* PlayerCharacter = MatchPlayerState->GetPawn<ADMPlayerCharacter>())
    {
        PlayerCharacter->OnQuestActionEvent().RemoveAll(this);
    }
}

void UDMQuestDirector::UnbindPlayerDelegates()
{
    ADMMatchGameState* MatchGameState = GetMatchGameState();

    if (!IsValid(MatchGameState))
    {
        return;
    }

    for (ADMMatchPlayerState* MatchPlayerState : DMGameplay::GetMatchPlayerStates(MatchGameState))
    {
        UnbindQuestActionSource(MatchPlayerState);

        MatchPlayerState->OnLifeStateChanged().RemoveAll(this);

        if (UDMQuestProgressComponent* QuestProgressComponent = MatchPlayerState->GetQuestProgressComponent())
        {
            QuestProgressComponent->OnQuestProgressChanged().RemoveAll(this);
        }
    }
}

#pragma endregion

#pragma region Scheduling

void UDMQuestDirector::HandleQuestAttempt(const int32 AttemptIndex)
{
    TryStartQuest(AttemptIndex);
}

bool UDMQuestDirector::TryStartQuest(const int32 AttemptIndex)
{
    ADMMatchGameMode* MatchGameMode = OwnerGameMode.Get();

    ADMMatchGameState* MatchGameState = GetMatchGameState();

    if (!IsValid(MatchGameMode) || !MatchGameMode->HasAuthority() || !IsValid(MatchGameState))
    {
        return false;
    }

    if (AttemptIndex < 0 || AttemptIndex >= ResolvedQuestAttemptCount)
    {
        return false;
    }

    /*
     * Overtime / Countdown / Finished 등에서는
     * Quest를 시작하지 않는다.
     */
    if (MatchGameState->GetCurrentPhase() != EDMMatchPhase::Playing)
    {
        UE_LOG(LogDMQuestDirector, Log, TEXT("Quest attempt skipped. Reason=NotPlaying, Attempt=%d"), AttemptIndex);

        return false;
    }

    /*
     * 현재 Quest가 아직 끝나지 않았다면
     * 새로운 Quest를 중첩하지 않는다.
     */
    if (ActiveQuestState.IsActive() || MatchGameState->HasCurrentQuest())
    {
        UE_LOG(
            LogDMQuestDirector, Log, TEXT("Quest attempt skipped. Reason=QuestAlreadyActive, Attempt=%d"), AttemptIndex
        );

        return false;
    }

    /*
     * Random Event의 Warning 또는 Active Window가
     * 존재하는 동안에는 Quest를 시작하지 않는다.
     */
    if (MatchGameState->HasCurrentRandomEvent())
    {
        UE_LOG(
            LogDMQuestDirector, Log, TEXT("Quest attempt skipped. Reason=RandomEventActive, Attempt=%d"), AttemptIndex
        );

        return false;
    }

    TArray<ADMMatchPlayerState*> Participants;

    if (!CollectAllAliveParticipants(Participants))
    {
        UE_LOG(
            LogDMQuestDirector, Log, TEXT("Quest attempt skipped. Reason=NotAllPlayersAlive, Attempt=%d"), AttemptIndex
        );

        return false;
    }

    if (CachedQuestDefinitions.IsEmpty())
    {
        return false;
    }

    const int32 DefinitionIndex = FMath::RandRange(0, CachedQuestDefinitions.Num() - 1);

    const FDMQuestDefinitionRow& Definition = CachedQuestDefinitions[DefinitionIndex];

    const int32 NewRunId = NextRunId++;

    const double ServerNow = MatchGameState->GetServerWorldTimeSeconds();

    FDMQuestRuntimeState NewQuestState;

    NewQuestState.RunId = NewRunId;

    NewQuestState.QuestTag = Definition.QuestTag;

    NewQuestState.StartServerTime = ServerNow;

    NewQuestState.EndServerTime = ServerNow + Definition.TimeLimit;

    ActiveParticipants.Reset();

    ActiveParticipants.Reserve(Participants.Num());

    for (ADMMatchPlayerState* MatchPlayerState : Participants)
    {
        ActiveParticipants.Add(MatchPlayerState);
    }

    ActiveDefinitionIndex = DefinitionIndex;

    ActiveQuestState = NewQuestState;

    for (ADMMatchPlayerState* MatchPlayerState : Participants)
    {
        UDMQuestProgressComponent* QuestProgressComponent = MatchPlayerState->GetQuestProgressComponent();

        if (!IsValid(QuestProgressComponent) || !QuestProgressComponent->BeginQuestRun(NewRunId))
        {
            UE_LOG(
                LogDMQuestDirector,
                Error,
                TEXT("Quest start failed: participant initialization failed. RunId=%d"),
                NewRunId
            );

            RollbackActiveQuestStart();

            return false;
        }
    }

    UWorld* World = MatchGameMode->GetWorld();

    if (!IsValid(World))
    {
        RollbackActiveQuestStart();

        return false;
    }

    World->GetTimerManager().SetTimer(
        QuestDeadlineTimerHandle, this, &UDMQuestDirector::HandleQuestDeadlineExpired, Definition.TimeLimit, false
    );

    if (!World->GetTimerManager().IsTimerActive(QuestDeadlineTimerHandle))
    {
        UE_LOG(LogDMQuestDirector, Error, TEXT("Quest start failed: deadline timer was not activated."));

        RollbackActiveQuestStart();

        return false;
    }

    if (!PublishPublicState(ActiveQuestState))
    {
        UE_LOG(LogDMQuestDirector, Error, TEXT("Quest start failed: GameState projection failed."));

        RollbackActiveQuestStart();

        return false;
    }

    ++StartedQuestCount;

    UE_LOG(
        LogDMQuestDirector,
        Log,
        TEXT("Quest started. RunId=%d, Quest=%s, Participants=%d, TimeLimit=%.3f, Attempt=%d"),
        NewRunId,
        *Definition.QuestTag.ToString(),
        Participants.Num(),
        Definition.TimeLimit,
        AttemptIndex
    );

    return true;
}

bool UDMQuestDirector::CollectAllAliveParticipants(TArray<ADMMatchPlayerState*>& OutParticipants) const
{
    OutParticipants.Reset();

    ADMMatchGameState* MatchGameState = GetMatchGameState();

    if (!IsValid(MatchGameState))
    {
        return false;
    }

    if (!DMMatch::IsValidPlayerCount(MatchGameState->PlayerArray.Num()))
    {
        return false;
    }

    for (APlayerState* PlayerState : MatchGameState->PlayerArray)
    {
        ADMMatchPlayerState* MatchPlayerState = Cast<ADMMatchPlayerState>(PlayerState);

        if (!IsValid(MatchPlayerState) || MatchPlayerState->GetLifeState() != EDMPlayerLifeState::Alive
            || !IsValid(MatchPlayerState->GetQuestProgressComponent()))
        {
            OutParticipants.Reset();

            return false;
        }

        OutParticipants.Add(MatchPlayerState);
    }

    return !OutParticipants.IsEmpty();
}

void UDMQuestDirector::ClearQuestAttemptTimers()
{
    ADMMatchGameMode* MatchGameMode = OwnerGameMode.Get();

    if (!IsValid(MatchGameMode))
    {
        QuestAttemptTimerHandles.Reset();

        return;
    }

    UWorld* World = MatchGameMode->GetWorld();

    if (!IsValid(World))
    {
        QuestAttemptTimerHandles.Reset();

        return;
    }

    for (FTimerHandle& TimerHandle : QuestAttemptTimerHandles)
    {
        World->GetTimerManager().ClearTimer(TimerHandle);
    }

    QuestAttemptTimerHandles.Reset();
}

#pragma endregion

#pragma region Active Run

void UDMQuestDirector::HandleQuestActionEvent(
    const FDMQuestActionEvent& ActionEvent, ADMMatchPlayerState* MatchPlayerState
)
{
    if (!ActiveQuestState.IsActive() || !IsValid(MatchPlayerState)
        || MatchPlayerState->GetLifeState() != EDMPlayerLifeState::Alive || !IsActiveParticipant(MatchPlayerState))
    {
        return;
    }

    const FDMQuestDefinitionRow* Definition = GetActiveQuestDefinition();

    if (Definition == nullptr)
    {
        return;
    }

    UDMQuestProgressComponent* QuestProgressComponent = MatchPlayerState->GetQuestProgressComponent();

    if (!IsValid(QuestProgressComponent))
    {
        return;
    }

    const EDMQuestParticipantState ParticipantState = QuestProgressComponent->GetParticipantState();

    if (ParticipantState != EDMQuestParticipantState::Active
        && ParticipantState != EDMQuestParticipantState::FailedPendingCompletion)
    {
        return;
    }

    /*
     * ==================================================
     * Count
     * ==================================================
     */
    if (Definition->ProgressMode == EDMQuestProgressMode::Count)
    {
        if (ActionEvent.EventTag != Definition->StartEventTag)
        {
            return;
        }

        QuestProgressComponent->AddCountProgress(ActiveQuestState.RunId, 1.0f, Definition->TargetValue);

        return;
    }

    /*
     * ==================================================
     * Duration Quest
     * ==================================================
     */

    const bool bIsStartEvent = ActionEvent.EventTag == Definition->StartEventTag;

    const bool bIsEndEvent = ActionEvent.EventTag == Definition->EndEventTag;

    if (!bIsStartEvent && !bIsEndEvent)
    {
        return;
    }

    /*
     * Pose Quest만 RequiredPose Context를 검사한다.
     *
     * Flat -> Crouched 전환 시
     * Pose.End(Flat)
     * Pose.Start(Crouched)
     * 순으로 들어오므로 원하는 자세만 통과한다.
     */
    const bool bIsPoseQuestEvent = Definition->StartEventTag == DMGameplayTags::QuestEvent_Pose_Start
        || Definition->EndEventTag == DMGameplayTags::QuestEvent_Pose_End;

    if (bIsPoseQuestEvent && ActionEvent.PoseType != Definition->RequiredPose)
    {
        return;
    }

    if (bIsStartEvent)
    {
        QuestProgressComponent->BeginDurationObservation(
            ActiveQuestState.RunId, Definition->ProgressMode, Definition->TargetValue
        );

        return;
    }

    QuestProgressComponent->EndDurationObservation(
        ActiveQuestState.RunId, Definition->ProgressMode, Definition->TargetValue
    );
}

void UDMQuestDirector::HandleQuestDeadlineExpired()
{
    if (!ActiveQuestState.IsActive())
    {
        return;
    }

    const int32 CurrentRunId = ActiveQuestState.RunId;

    const FDMQuestDefinitionRow* Definition = GetActiveQuestDefinition();

    if (Definition == nullptr)
    {
        return;
    }

    for (const TWeakObjectPtr<ADMMatchPlayerState>& ParticipantWeak : ActiveParticipants)
    {
        ADMMatchPlayerState* MatchPlayerState = ParticipantWeak.Get();

        if (!IsValid(MatchPlayerState))
        {
            continue;
        }

        UDMQuestProgressComponent* QuestProgressComponent = MatchPlayerState->GetQuestProgressComponent();

        if (!IsValid(QuestProgressComponent))
        {
            continue;
        }

        if (QuestProgressComponent->GetQuestRunId() != CurrentRunId)
        {
            continue;
        }

        if (QuestProgressComponent->GetParticipantState() == EDMQuestParticipantState::Active)
        {
            const bool bMarkedFailed = QuestProgressComponent->MarkFailedPendingCompletion(CurrentRunId);

            /*
             * Progress delegate의 동기 재진입으로
             * 상태가 즉시 CompletedInTime 등으로
             * 바뀌었을 가능성까지 다시 확인한다.
             */
            if (bMarkedFailed
                && QuestProgressComponent->GetParticipantState() == EDMQuestParticipantState::FailedPendingCompletion)
            {
                // 실패 상태가 확정된 참가자만 사이렌을 켠다.
                StartFailureWarningForParticipant(MatchPlayerState);
            }
        }
    }

    /*
     * Deadline 도달 시점부터는
     * 새로운 CompletedInTime Player가 생길 수 없다.
     */
    FinalizeCompletionRanks();

    /*
     * Deadline은 Rank window 종료 시점이지
     * Global Quest를 즉시 삭제하는 시점이 아니다.
     *
     * 실패한 Player는 남은 행동을 완료해야 하므로
     * FailedPendingCompletion 상태로 Quest가 유지된다.
     */
    TryFinishActiveQuestRun();
}

void UDMQuestDirector::HandlePlayerLifeStateChanged(
    ADMMatchPlayerState* MatchPlayerState, const EDMPlayerLifeState NewLifeState
)
{
    if (!IsValid(MatchPlayerState))
    {
        return;
    }

    /*
     * Respawn 완료 시 새 Pawn이 이미 Possess된 뒤
     * LifeState가 Alive로 바뀐다.
     *
     * 따라서 여기서 새 Character Action Event에
     * 자동으로 다시 연결할 수 있다.
     */
    if (NewLifeState == EDMPlayerLifeState::Alive)
    {
        BindQuestActionSource(MatchPlayerState);

        return;
    }

    StopFailureWarningForParticipant(MatchPlayerState);

    /*
     * Dead / Spectating / Respawning 동안에는
     * 이전 Pawn의 Quest Action을 받지 않는다.
     */
    UnbindQuestActionSource(MatchPlayerState);

    if (!ActiveQuestState.IsActive())
    {
        return;
    }

    bool bIsActiveParticipant = false;

    for (const TWeakObjectPtr<ADMMatchPlayerState>& ParticipantWeak : ActiveParticipants)
    {
        if (ParticipantWeak.Get() == MatchPlayerState)
        {
            bIsActiveParticipant = true;

            break;
        }
    }

    if (!bIsActiveParticipant)
    {
        return;
    }

    if (UDMQuestProgressComponent* QuestProgressComponent = MatchPlayerState->GetQuestProgressComponent())
    {
        QuestProgressComponent->MarkExcluded(ActiveQuestState.RunId);
    }
}

void UDMQuestDirector::HandleParticipantQuestProgressChanged(
    UDMQuestProgressComponent* QuestProgressComponent, const FDMQuestPlayerProgress NewQuestProgress
)
{
    if (!ActiveQuestState.IsActive() || !IsValid(QuestProgressComponent)
        || NewQuestProgress.RunId != ActiveQuestState.RunId)
    {
        return;
    }

    if (bIsFinalizingCompletionRanks)
    {
        return;
    }

    const FDMQuestDefinitionRow* Definition = GetActiveQuestDefinition();

    if (Definition == nullptr)
    {
        return;
    }

    ADMMatchPlayerState* MatchPlayerState = Cast<ADMMatchPlayerState>(QuestProgressComponent->GetOwner());

    const bool bProgressReachedTarget = NewQuestProgress.ProgressValue + KINDA_SMALL_NUMBER >= Definition->TargetValue;

    const bool bCanCommitCompletion = NewQuestProgress.State == EDMQuestParticipantState::Active
        || NewQuestProgress.State == EDMQuestParticipantState::FailedPendingCompletion;

    if (bProgressReachedTarget && bCanCommitCompletion)
    {
        const bool bCompletedWithinDeadline = NewQuestProgress.CompletionServerTime > 0.0
            && NewQuestProgress.CompletionServerTime
                <= ActiveQuestState.EndServerTime + static_cast<double>(KINDA_SMALL_NUMBER);

        if (bCompletedWithinDeadline)
        {
            if (QuestProgressComponent->MarkCompletedInTime(ActiveQuestState.RunId))
            {
                StopFailureWarningForParticipant(MatchPlayerState);
            }

            return;
        }

        /*
         * Deadline Timer보다 action/timer callback이
         * 늦게 dispatch된 경우까지 방어한다.
         */
        if (NewQuestProgress.State == EDMQuestParticipantState::Active)
        {
            QuestProgressComponent->MarkFailedPendingCompletion(ActiveQuestState.RunId);

            /*
             * MarkFailedPendingCompletion이
             * ProgressChanged를 동기 Broadcast하므로
             * 재진입한 Handler에서
             * CompletedAfterFailure로 처리된다.
             */
            return;
        }

        if (QuestProgressComponent->MarkCompletedAfterFailure(ActiveQuestState.RunId))
        {
            StopFailureWarningForParticipant(MatchPlayerState);
        }

        return;
    }

    TryFinishActiveQuestRun();
}

void UDMQuestDirector::FinalizeCompletionRanks()
{
    if (!ActiveQuestState.IsActive() || bIsFinalizingCompletionRanks)
    {
        return;
    }

    struct FCompletedParticipant
    {
        ADMMatchPlayerState* PlayerState = nullptr;

        UDMQuestProgressComponent* ProgressComponent = nullptr;

        double CompletionServerTime = 0.0;

        int32 BaseSlot = INDEX_NONE;
    };

    TArray<FCompletedParticipant> CompletedParticipants;

    for (const TWeakObjectPtr<ADMMatchPlayerState>& ParticipantWeak : ActiveParticipants)
    {
        ADMMatchPlayerState* MatchPlayerState = ParticipantWeak.Get();

        if (!IsValid(MatchPlayerState))
        {
            continue;
        }

        UDMQuestProgressComponent* QuestProgressComponent = MatchPlayerState->GetQuestProgressComponent();

        if (!IsValid(QuestProgressComponent))
        {
            continue;
        }

        const FDMQuestPlayerProgress Progress = QuestProgressComponent->GetQuestProgress();

        if (Progress.RunId != ActiveQuestState.RunId || Progress.State != EDMQuestParticipantState::CompletedInTime)
        {
            continue;
        }

        if (!ensureMsgf(
                Progress.CompletionServerTime > 0.0,
                TEXT("CompletedInTime participant has invalid CompletionServerTime. Player=%s"),
                *GetNameSafe(MatchPlayerState)
            ))
        {
            return;
        }

        FCompletedParticipant Entry;

        Entry.PlayerState = MatchPlayerState;

        Entry.ProgressComponent = QuestProgressComponent;

        Entry.CompletionServerTime = Progress.CompletionServerTime;

        Entry.BaseSlot = MatchPlayerState->GetBaseSlot();

        CompletedParticipants.Add(Entry);
    }

    if (CompletedParticipants.IsEmpty())
    {
        return;
    }

    /*
     * 이미 전체 Rank가 확정된 경우 다시 계산하지 않는다.
     */
    bool bAllAlreadyRanked = true;

    for (const FCompletedParticipant& Entry : CompletedParticipants)
    {
        if (Entry.ProgressComponent->GetCompletionRank() == 0)
        {
            bAllAlreadyRanked = false;

            break;
        }
    }

    if (bAllAlreadyRanked)
    {
        return;
    }

    CompletedParticipants.Sort(
        [](const FCompletedParticipant& A, const FCompletedParticipant& B)
        {
            if (!FMath::IsNearlyEqual(A.CompletionServerTime, B.CompletionServerTime, 0.000001))
            {
                return A.CompletionServerTime < B.CompletionServerTime;
            }

            /*
             * 같은 logical completion time이면
             * 이미 서버에서 확정된 BaseSlot으로
             * deterministic tie-break한다.
             */
            return A.BaseSlot < B.BaseSlot;
        }
    );

    TGuardValue<bool> RankFinalizeGuard(bIsFinalizingCompletionRanks, true);

    for (int32 Index = 0; Index < CompletedParticipants.Num(); ++Index)
    {
        FCompletedParticipant& Entry = CompletedParticipants[Index];

        const int32 CompletionRank = Index + 1;

        if (!Entry.ProgressComponent->AssignCompletionRank(ActiveQuestState.RunId, CompletionRank))
        {
            continue;
        }

        // 순위는 결과 표시용 정보이며 보상을 지급하지 않는다.
        UE_LOG(
            LogDMQuestDirector,
            Log,
            TEXT("Quest rank finalized. Player=%s, Rank=%d, CompletionTime=%.6f"),
            *GetNameSafe(Entry.PlayerState),
            CompletionRank,
            Entry.CompletionServerTime
        );
    }
}

void UDMQuestDirector::StartFailureWarningForParticipant(ADMMatchPlayerState* MatchPlayerState)
{
    if (!IsValid(MatchPlayerState))
    {
        return;
    }

    ADMPlayerCharacter* PlayerCharacter = MatchPlayerState->GetPawn<ADMPlayerCharacter>();

    if (!IsValid(PlayerCharacter))
    {
        UE_LOG(
            LogDMQuestDirector,
            Verbose,
            TEXT("Quest siren start skipped: Pawn is unavailable. Player=%s"),
            *GetNameSafe(MatchPlayerState)
        );

        return;
    }

    // 서버는 공개 상태만 변경한다.
    // 실제 3D 사운드는 각 머신이 복제된 상태를 보고 재생한다.
    PlayerCharacter->SetQuestSirenActive(true);
}

void UDMQuestDirector::StopFailureWarningForParticipant(ADMMatchPlayerState* MatchPlayerState)
{
    if (!IsValid(MatchPlayerState))
    {
        return;
    }

    ADMPlayerCharacter* PlayerCharacter = MatchPlayerState->GetPawn<ADMPlayerCharacter>();

    if (!IsValid(PlayerCharacter))
    {
        return;
    }

    // 공개 상태를 끄면 Host는 즉시, Client는 OnRep에서 소리를 정지한다.
    PlayerCharacter->SetQuestSirenActive(false);
}

void UDMQuestDirector::StopAllFailureWarnings()
{
    for (const TWeakObjectPtr<ADMMatchPlayerState>& ParticipantWeak : ActiveParticipants)
    {
        StopFailureWarningForParticipant(ParticipantWeak.Get());
    }
}

void UDMQuestDirector::TryFinishActiveQuestRun()
{
    if (!ActiveQuestState.IsActive() || !AreAllActiveParticipantsResolved())
    {
        return;
    }

    FinishActiveQuestRun();
}

bool UDMQuestDirector::AreAllActiveParticipantsResolved() const
{
    if (!ActiveQuestState.IsActive() || ActiveParticipants.IsEmpty())
    {
        return false;
    }

    for (const TWeakObjectPtr<ADMMatchPlayerState>& ParticipantWeak : ActiveParticipants)
    {
        const ADMMatchPlayerState* MatchPlayerState = ParticipantWeak.Get();

        if (!IsValid(MatchPlayerState))
        {
            return false;
        }

        const UDMQuestProgressComponent* QuestProgressComponent = MatchPlayerState->GetQuestProgressComponent();

        if (!IsValid(QuestProgressComponent))
        {
            return false;
        }

        const FDMQuestPlayerProgress Progress = QuestProgressComponent->GetQuestProgress();

        if (Progress.RunId != ActiveQuestState.RunId || !IsTerminalParticipantState(Progress.State))
        {
            return false;
        }
    }

    return true;
}

void UDMQuestDirector::FinishActiveQuestRun()
{
    if (!ActiveQuestState.IsActive())
    {
        return;
    }

    /*
     * 모든 참가자가 Deadline 전에 끝난 경우에는
     * Deadline callback이 오기 전에 Run이 종료되므로
     * 여기서 Rank를 확정해야 한다.
     */
    FinalizeCompletionRanks();

    StopAllFailureWarnings();

    ADMMatchGameMode* MatchGameMode = OwnerGameMode.Get();

    if (IsValid(MatchGameMode) && IsValid(MatchGameMode->GetWorld()))
    {
        MatchGameMode->GetWorld()->GetTimerManager().ClearTimer(QuestDeadlineTimerHandle);
    }

    const int32 FinishedRunId = ActiveQuestState.RunId;

    const FGameplayTag FinishedQuestTag = ActiveQuestState.QuestTag;

    if (IsValid(MatchGameMode))
    {
        PublishPublicState(FDMQuestRuntimeState());
    }

    ActiveQuestState = FDMQuestRuntimeState();

    ActiveDefinitionIndex = INDEX_NONE;

    ActiveParticipants.Reset();

    UE_LOG(
        LogDMQuestDirector,
        Log,
        TEXT("Quest run finished. RunId=%d, Quest=%s"),
        FinishedRunId,
        *FinishedQuestTag.ToString()
    );
}

void UDMQuestDirector::RollbackActiveQuestStart()
{
    StopAllFailureWarnings();

    ADMMatchGameMode* MatchGameMode = OwnerGameMode.Get();

    if (IsValid(MatchGameMode) && IsValid(MatchGameMode->GetWorld()))
    {
        MatchGameMode->GetWorld()->GetTimerManager().ClearTimer(QuestDeadlineTimerHandle);
    }

    const int32 RollbackRunId = ActiveQuestState.RunId;

    for (const TWeakObjectPtr<ADMMatchPlayerState>& ParticipantWeak : ActiveParticipants)
    {
        if (ADMMatchPlayerState* MatchPlayerState = ParticipantWeak.Get())
        {
            if (UDMQuestProgressComponent* QuestProgressComponent = MatchPlayerState->GetQuestProgressComponent())
            {
                if (QuestProgressComponent->GetQuestRunId() == RollbackRunId)
                {
                    QuestProgressComponent->ResetQuestProgress();
                }
            }
        }
    }

    if (IsValid(MatchGameMode))
    {
        PublishPublicState(FDMQuestRuntimeState());
    }

    ActiveQuestState = FDMQuestRuntimeState();

    ActiveDefinitionIndex = INDEX_NONE;

    ActiveParticipants.Reset();
}

#pragma endregion

#pragma region Helpers

ADMMatchGameState* UDMQuestDirector::GetMatchGameState() const
{
    const ADMMatchGameMode* MatchGameMode = OwnerGameMode.Get();

    if (!IsValid(MatchGameMode))
    {
        return nullptr;
    }

    return MatchGameMode->GetGameState<ADMMatchGameState>();
}

bool UDMQuestDirector::PublishPublicState(const FDMQuestRuntimeState& NewPublicState) const
{
    return PublicStatePublisher.IsBound() && PublicStatePublisher.Execute(NewPublicState);
}

const FDMQuestDefinitionRow* UDMQuestDirector::GetActiveQuestDefinition() const
{
    if (!ActiveQuestState.IsActive() || !CachedQuestDefinitions.IsValidIndex(ActiveDefinitionIndex))
    {
        return nullptr;
    }

    const FDMQuestDefinitionRow& Definition = CachedQuestDefinitions[ActiveDefinitionIndex];

    if (Definition.QuestTag != ActiveQuestState.QuestTag)
    {
        return nullptr;
    }

    return &Definition;
}

bool UDMQuestDirector::IsActiveParticipant(const ADMMatchPlayerState* MatchPlayerState) const
{
    if (!IsValid(MatchPlayerState))
    {
        return false;
    }

    for (const TWeakObjectPtr<ADMMatchPlayerState>& ParticipantWeak : ActiveParticipants)
    {
        if (ParticipantWeak.Get() == MatchPlayerState)
        {
            return true;
        }
    }

    return false;
}

bool UDMQuestDirector::IsTerminalParticipantState(const EDMQuestParticipantState State)
{
    switch (State)
    {
    case EDMQuestParticipantState::CompletedInTime:

    case EDMQuestParticipantState::CompletedAfterFailure:

    case EDMQuestParticipantState::Excluded:

        return true;

    case EDMQuestParticipantState::Inactive:

    case EDMQuestParticipantState::Active:

    case EDMQuestParticipantState::FailedPendingCompletion:

    default:

        return false;
    }
}

#pragma endregion