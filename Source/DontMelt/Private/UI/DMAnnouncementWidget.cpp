
#include "UI/DMAnnouncementWidget.h"

#include "Framework/Match/DMMatchPlayerController.h"
#include "Framework/Match/DMMatchGameState.h"
#include "Framework/Match/DMMatchPlayerState.h"
#include "Quests/DMQuestProgressComponent.h"

#include "Components/TextBlock.h"
#include "Engine/World.h"

#pragma region Data Source

void UDMAnnouncementWidget::SetDataSources(
    ADMMatchPlayerController* InController, ADMMatchGameState* InGameState, ADMMatchPlayerState* InPlayerState
)
{
    UDMQuestProgressComponent* NewProgress =
        IsValid(InPlayerState) ? InPlayerState->GetQuestProgressComponent() : nullptr;

    if (BoundController.Get() == InController && BoundGameState.Get() == InGameState
        && BoundQuestProgress.Get() == NewProgress)
    {
        return;
    }

    StopMessageTimer();
    StopEventWatch();
    HideMessage();

    // 이전 데이터 소스의 모든 Delegate를 해제한다.

    if (ADMMatchPlayerController* OldController = BoundController.Get())
    {
        OldController->OnRelicStolenNotification().RemoveAll(this);
    }

    if (ADMMatchGameState* OldGameState = BoundGameState.Get())
    {
        OldGameState->OnQuestPublicStateChanged().RemoveAll(this);

        OldGameState->OnRandomEventPublicStateChanged().RemoveAll(this);
    }

    if (UDMQuestProgressComponent* OldProgress = BoundQuestProgress.Get())
    {
        OldProgress->OnQuestProgressChanged().RemoveAll(this);
    }

    BoundController = InController;
    BoundGameState = InGameState;
    BoundQuestProgress = NewProgress;

    if (IsValid(InController))
    {
        InController->OnRelicStolenNotification().AddUObject(this, &ThisClass::HandleRelicStolen);
    }

    if (IsValid(InGameState))
    {
        InGameState->OnQuestPublicStateChanged().AddUObject(this, &ThisClass::HandleQuestPublicStateChanged);

        InGameState->OnRandomEventPublicStateChanged().AddUObject(
            this, &ThisClass::HandleRandomEventPublicStateChanged
        );
    }

    if (IsValid(NewProgress))
    {
        NewProgress->OnQuestProgressChanged().AddUObject(this, &ThisClass::HandleQuestProgressChanged);
    }

    // 최초 연결은 과거 이벤트로 취급해 중복 안내하지 않는다.
    const FDMQuestRuntimeState QuestState =
        IsValid(InGameState) ? InGameState->GetQuestPublicState() : FDMQuestRuntimeState();

    LastQuestRunId = QuestState.RunId;
    bHadQuest = QuestState.IsActive();

    const FDMQuestPlayerProgress PlayerProgress =
        IsValid(NewProgress) ? NewProgress->GetQuestProgress() : FDMQuestPlayerProgress();

    LastProgressRunId = PlayerProgress.RunId;
    LastParticipantState = PlayerProgress.State;

    const FDMRandomEventRuntimeState EventState =
        IsValid(InGameState) ? InGameState->GetRandomEventPublicState() : FDMRandomEventRuntimeState();

    bHadEvent = EventState.IsScheduledOrActive();

    LastEventStartServerTime = EventState.StartServerTime;

    LastEventEndServerTime = EventState.EndServerTime;

    // 이미 예고 중인 이벤트라면 시작 시각만 감시한다.
    StartEventWatchIfNeeded();
}

void UDMAnnouncementWidget::SetPresentationActive(const bool bActive)
{
    if (bPresentationActive == bActive)
    {
        return;
    }

    bPresentationActive = bActive;

    if (!bPresentationActive)
    {
        StopMessageTimer();
        StopEventWatch();
        HideMessage();
        return;
    }

    StartEventWatchIfNeeded();
}

#pragma endregion

#pragma region Event Handlers

void UDMAnnouncementWidget::HandleQuestPublicStateChanged(ADMMatchGameState* Source, FDMQuestRuntimeState NewState)
{
    if (Source != BoundGameState.Get())
    {
        return;
    }

    const bool bNewQuest = NewState.IsActive();

    if (bNewQuest && NewState.RunId != LastQuestRunId)
    {
        ShowMessage(NSLOCTEXT("DMAnnouncement", "QuestStarted", "퀘스트 시작!"));
    }
    else if (!bNewQuest && bHadQuest)
    {
        ShowMessage(NSLOCTEXT("DMAnnouncement", "QuestEnded", "퀘스트 종료"));
    }

    LastQuestRunId = NewState.RunId;
    bHadQuest = bNewQuest;
}

void UDMAnnouncementWidget::HandleQuestProgressChanged(
    UDMQuestProgressComponent* Source, FDMQuestPlayerProgress NewProgress
)
{
    if (Source != BoundQuestProgress.Get())
    {
        return;
    }

    const bool bRunChanged = NewProgress.RunId != LastProgressRunId;

    const bool bStateChanged = NewProgress.State != LastParticipantState;

    LastProgressRunId = NewProgress.RunId;
    LastParticipantState = NewProgress.State;

    // 새 퀘스트에 처음 참가하는 Active 상태는
    // 공개 퀘스트 시작 알림에서 안내한다.
    if (!bStateChanged || (bRunChanged && NewProgress.State == EDMQuestParticipantState::Active))
    {
        return;
    }

    switch (NewProgress.State)
    {
    case EDMQuestParticipantState::CompletedInTime:
        ShowMessage(NSLOCTEXT("DMAnnouncement", "QuestSuccess", "퀘스트 성공!"));
        break;

    case EDMQuestParticipantState::FailedPendingCompletion:
        ShowMessage(NSLOCTEXT("DMAnnouncement", "QuestFailedSiren", "시간 초과 · 완료할 때까지 사이렌이 울립니다"));
        break;

    case EDMQuestParticipantState::CompletedAfterFailure:
        ShowMessage(NSLOCTEXT("DMAnnouncement", "QuestAfterFailure", "퀘스트 수행 완료"));
        break;

    default:
        break;
    }
}

void UDMAnnouncementWidget::HandleRandomEventPublicStateChanged(
    ADMMatchGameState* Source, FDMRandomEventRuntimeState NewState
)
{
    if (Source != BoundGameState.Get())
    {
        return;
    }

    const bool bNewEvent = NewState.IsScheduledOrActive();

    StopEventWatch();

    if (!bNewEvent)
    {
        if (bHadEvent)
        {
            const double ServerNow = Source->GetServerWorldTimeSeconds();

            ShowMessage(
                ServerNow < LastEventEndServerTime ? NSLOCTEXT("DMAnnouncement", "EventCancelled", "랜덤 이벤트 취소")
                                                   : NSLOCTEXT("DMAnnouncement", "EventEnded", "랜덤 이벤트 종료")
            );
        }

        bHadEvent = false;
        LastEventStartServerTime = 0.0;
        LastEventEndServerTime = 0.0;
        return;
    }

    const bool bDifferentEvent =
        !bHadEvent || !FMath::IsNearlyEqual(LastEventStartServerTime, NewState.StartServerTime);

    bHadEvent = true;

    LastEventStartServerTime = NewState.StartServerTime;

    LastEventEndServerTime = NewState.EndServerTime;

    if (bDifferentEvent)
    {
        const double ServerNow = Source->GetServerWorldTimeSeconds();

        ShowMessage(
            ServerNow < NewState.StartServerTime ? NSLOCTEXT("DMAnnouncement", "EventWarning", "랜덤 이벤트 예고")
                                                 : NSLOCTEXT("DMAnnouncement", "EventStarted", "랜덤 이벤트 시작")
        );
    }

    StartEventWatchIfNeeded();
}

void UDMAnnouncementWidget::HandleRelicStolen()
{
    ShowMessage(NSLOCTEXT("DMAnnouncement", "RelicStolen", "누군가 유물을 훔쳤습니다!"));
}

#pragma endregion

#pragma region Presentation

void UDMAnnouncementWidget::ShowMessage(const FText& Message)
{
    if (!bPresentationActive || !IsValid(TXT_Announcement) || Message.IsEmpty())
    {
        return;
    }

    StopMessageTimer();

    TXT_Announcement->SetText(Message);

    SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    // 최신 메시지 한 건만 표시하고 지정 시간 후 숨긴다.
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            MessageTimerHandle, this, &ThisClass::HideMessage, FMath::Max(0.1f, MessageDisplayDuration), false
        );
    }
}

void UDMAnnouncementWidget::HideMessage()
{
    SetVisibility(ESlateVisibility::Collapsed);
}

void UDMAnnouncementWidget::StopMessageTimer()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(MessageTimerHandle);
    }

    MessageTimerHandle.Invalidate();
}

void UDMAnnouncementWidget::StartEventWatchIfNeeded()
{
    if (!bPresentationActive)
    {
        return;
    }

    ADMMatchGameState* GameState = BoundGameState.Get();

    if (!IsValid(GameState))
    {
        return;
    }

    const FDMRandomEventRuntimeState EventState = GameState->GetRandomEventPublicState();

    if (!EventState.IsScheduledOrActive() || GameState->GetServerWorldTimeSeconds() >= EventState.StartServerTime)
    {
        return;
    }

    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    // 이벤트 예고 구간에만 실제 시작 시각을 감시한다.
    World->GetTimerManager().SetTimer(EventStartTimerHandle, this, &ThisClass::HandleEventStartTimeReached, 0.1f, true);
}

void UDMAnnouncementWidget::HandleEventStartTimeReached()
{
    ADMMatchGameState* GameState = BoundGameState.Get();

    if (!bPresentationActive || !IsValid(GameState))
    {
        StopEventWatch();
        return;
    }

    const FDMRandomEventRuntimeState EventState = GameState->GetRandomEventPublicState();

    if (!EventState.IsScheduledOrActive())
    {
        StopEventWatch();
        return;
    }

    const double ServerNow = GameState->GetServerWorldTimeSeconds();

    if (ServerNow < EventState.StartServerTime)
    {
        return;
    }

    StopEventWatch();

    if (ServerNow < EventState.EndServerTime)
    {
        ShowMessage(NSLOCTEXT("DMAnnouncement", "EventBecameActive", "랜덤 이벤트 시작"));
    }
}

void UDMAnnouncementWidget::StopEventWatch()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(EventStartTimerHandle);
    }

    EventStartTimerHandle.Invalidate();
}

#pragma endregion

#pragma region Lifecycle

void UDMAnnouncementWidget::NativeDestruct()
{
    SetPresentationActive(false);

    SetDataSources(nullptr, nullptr, nullptr);

    StopMessageTimer();
    StopEventWatch();

    Super::NativeDestruct();
}

#pragma endregion
