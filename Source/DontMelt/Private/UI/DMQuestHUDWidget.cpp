
#include "UI/DMQuestHUDWidget.h"

#include "Framework/Match/DMMatchGameState.h"
#include "Framework/Match/DMMatchPlayerState.h"
#include "Quests/DMQuestProgressComponent.h"
#include "Framework/Match/DMMatchPlayerController.h"

#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Animation/WidgetAnimation.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogDMQuestHUD, Log, All);

namespace
{
    // 진행 중인 퀘스트의 시간 표시를 0.2초마다 갱신한다.
    constexpr float QuestHUDRefreshInterval = 0.2f;
}

#pragma region Lifecycle

void UDMQuestHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 아직 퀘스트 데이터가 없으므로 처음에는 숨긴다.
    SetVisibility(ESlateVisibility::Collapsed);
}

void UDMQuestHUDWidget::NativeDestruct()
{
    SetPresentationActive(false);
    SetDataSources(nullptr, nullptr);

    StopRefreshTimer();

    // Widget 종료 시 표시용 행 검색 캐시를 초기화한다.
    CachedDefinitionQuestTag = FGameplayTag();
    CachedDefinitionRowName = NAME_None;

    Super::NativeDestruct();
}

void UDMQuestHUDWidget::OnAnimationFinished_Implementation(const UWidgetAnimation* Animation)
{
    Super::OnAnimationFinished_Implementation(Animation);

    if (Animation != Anim_QuestExit || !bQuestExitInProgress)
    {
        return;
    }

    const int32 FinishedRunId = ActiveQuestExitRunId;

    bQuestExitInProgress = false;
    ActiveQuestExitRunId = 0;

    FinishQuestExit(FinishedRunId);
}

#pragma endregion

#pragma region Data Source

void UDMQuestHUDWidget::SetDataSources(ADMMatchGameState* InGameState, ADMMatchPlayerState* InPlayerState)
{
    UDMQuestProgressComponent* NewProgressComponent =
        IsValid(InPlayerState) ? InPlayerState->GetQuestProgressComponent() : nullptr;

    const bool bSourcesChanged =
        BoundGameState.Get() != InGameState || BoundQuestProgressComponent.Get() != NewProgressComponent;

    if (!bSourcesChanged)
    {
        RefreshPresentation();
        return;
    }

    StopRefreshTimer();
    CancelQuestExitAnimation();

    PresentedQuestRunId = 0;
    QuestExitHandledRunId = 0;

    // 이전 GameState의 퀘스트 발생 알림을 해제한다.
    if (ADMMatchGameState* OldGameState = BoundGameState.Get())
    {
        OldGameState->OnQuestPublicStateChanged().RemoveAll(this);
    }

    // 이전 개인 QuestProgress 알림을 해제한다.
    if (UDMQuestProgressComponent* OldProgress = BoundQuestProgressComponent.Get())
    {
        OldProgress->OnQuestProgressChanged().RemoveAll(this);
    }

    BoundGameState = InGameState;
    BoundQuestProgressComponent = NewProgressComponent;

    if (IsValid(InGameState))
    {
        InGameState->OnQuestPublicStateChanged().AddUObject(this, &ThisClass::HandleQuestPublicStateChanged);
    }

    if (IsValid(NewProgressComponent))
    {
        NewProgressComponent->OnQuestProgressChanged().AddUObject(this, &ThisClass::HandleQuestProgressChanged);
    }

    // 다음 Delegate를 기다리지 않고 현재 상태를 반영한다.
    RefreshPresentation();
}

void UDMQuestHUDWidget::SetPresentationActive(const bool bActive)
{
    if (bPresentationActive == bActive)
    {
        return;
    }

    bPresentationActive = bActive;

    if (!bPresentationActive)
    {
        StopRefreshTimer();
        CancelQuestExitAnimation();
        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    RefreshPresentation();
}

#pragma endregion

#pragma region Data Binding

const FDMQuestDefinitionRow* UDMQuestHUDWidget::FindQuestDefinition(const FGameplayTag& QuestTag)
{
    if (!IsValid(QuestDefinitionTable) || !QuestTag.IsValid())
    {
        return nullptr;
    }

    // 퀘스트 종류가 변경될 때에만 DataTable을 검색한다.
    if (CachedDefinitionQuestTag != QuestTag)
    {
        CachedDefinitionQuestTag = QuestTag;
        CachedDefinitionRowName = NAME_None;

        for (const FName& RowName : QuestDefinitionTable->GetRowNames())
        {
            const FDMQuestDefinitionRow* Row =
                QuestDefinitionTable->FindRow<FDMQuestDefinitionRow>(RowName, TEXT("DMQuestHUD"), false);

            if (Row != nullptr && Row->QuestTag == QuestTag)
            {
                CachedDefinitionRowName = RowName;
                break;
            }
        }

        // 동일한 퀘스트에 대해 경고 로그를 반복하지 않는다.
        if (CachedDefinitionRowName.IsNone())
        {
            UE_LOG(
                LogDMQuestHUD,
                Warning,
                TEXT("Quest definition missing. QuestTag=%s, Table=%s"),
                *QuestTag.ToString(),
                *GetNameSafe(QuestDefinitionTable)
            );
        }
    }

    if (CachedDefinitionRowName.IsNone())
    {
        return nullptr;
    }

    // 찾은 Row를 실제로 반환하여 HUD가 이름·설명을 사용하게 한다.
    return QuestDefinitionTable->FindRow<FDMQuestDefinitionRow>(CachedDefinitionRowName, TEXT("DMQuestHUD"), false);
}

void UDMQuestHUDWidget::HandleQuestPublicStateChanged(
    ADMMatchGameState* Source, FDMQuestRuntimeState /*NewState*/
)
{
    if (Source != BoundGameState.Get())
    {
        return;
    }

    // 공개 상태가 바뀌면 현재 Run 정보를 다시 읽는다.
    RefreshPresentation();
}

void UDMQuestHUDWidget::HandleQuestProgressChanged(
    UDMQuestProgressComponent* Source, FDMQuestPlayerProgress /*NewProgress*/
)
{
    if (Source != BoundQuestProgressComponent.Get())
    {
        return;
    }

    // 개인 진행도·성공 여부·순위를 다시 조회한다.
    RefreshPresentationInternal(true);
}

void UDMQuestHUDWidget::RefreshPresentation()
{
    RefreshPresentationInternal(false);
}

void UDMQuestHUDWidget::RefreshPresentationInternal(const bool bAllowCompletionAnimation)
{
    if (!bPresentationActive || !IsValid(TXT_QuestName) || !IsValid(TXT_QuestDescription) || !IsValid(TXT_QuestTime)
        || !IsValid(TXT_QuestProgress) || !IsValid(TXT_QuestStatus) || !IsValid(PB_QuestProgress))
    {
        StopRefreshTimer();
        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    ADMMatchGameState* GameState = BoundGameState.Get();

    if (!IsValid(GameState))
    {
        StopRefreshTimer();
        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    const FDMQuestRuntimeState PublicQuest = GameState->GetQuestPublicState();

    // 서버에서 현재 퀘스트를 종료했으면 즉시 숨긴다.
    if (!PublicQuest.IsActive())
    {
        StopRefreshTimer();
        CancelQuestExitAnimation();
        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    if (PresentedQuestRunId != PublicQuest.RunId)
    {
        ResetQuestRunPresentation(PublicQuest.RunId);

        if (ADMMatchPlayerController* Controller = Cast<ADMMatchPlayerController>(GetOwningPlayer()))
        {
            Controller->NotifyQuestStartedAudio(PublicQuest.RunId);
        }
    }

    SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    // 현재 QuestTag에 해당하는 정적 정의를 조회한다.
    const FDMQuestDefinitionRow* Definition = FindQuestDefinition(PublicQuest.QuestTag);

    if (Definition != nullptr)
    {
        TXT_QuestName->SetText(
            Definition->DisplayName.IsEmpty() ? NSLOCTEXT("DMQuestHUD", "DefaultQuestName", "현재 퀘스트")
                                              : Definition->DisplayName
        );

        TXT_QuestDescription->SetText(Definition->Description);
    }
    else
    {
        TXT_QuestName->SetText(NSLOCTEXT("DMQuestHUD", "MissingDefinition", "퀘스트 정보 확인 필요"));

        TXT_QuestDescription->SetText(FText::GetEmpty());
    }

    // 제한시간은 GameState의 서버 기준 Query를 사용한다.
    const double RemainingTime = GameState->GetRemainingQuestTime();

    const int32 RemainingWholeSeconds = FMath::Max(0, FMath::CeilToInt(static_cast<float>(RemainingTime)));

    if (RemainingTime > 0.0 && RemainingTime <= 5.0)
    {
        if (ADMMatchPlayerController* Controller = Cast<ADMMatchPlayerController>(GetOwningPlayer()))
        {
            Controller->NotifyQuestDeadlineWarningAudio(PublicQuest.RunId);
        }
    }

    TXT_QuestTime->SetText(
        FText::FromString(FString::Printf(TEXT("%02d:%02d"), RemainingWholeSeconds / 60, RemainingWholeSeconds % 60))
    );

    UDMQuestProgressComponent* ProgressComponent = BoundQuestProgressComponent.Get();

    const FDMQuestPlayerProgress PlayerProgress =
        IsValid(ProgressComponent) ? ProgressComponent->GetQuestProgress() : FDMQuestPlayerProgress();

    // 공개 Run과 본인 진행도의 RunId가 일치해야 한다.
    const bool bHasCurrentProgress = IsValid(ProgressComponent) && PlayerProgress.RunId == PublicQuest.RunId
        && PlayerProgress.State != EDMQuestParticipantState::Inactive;

    const bool bQuestCompleted = bHasCurrentProgress
        && (PlayerProgress.State == EDMQuestParticipantState::CompletedInTime
            || PlayerProgress.State == EDMQuestParticipantState::CompletedAfterFailure);

    if (bQuestCompleted)
    {
        if (PlayerProgress.State == EDMQuestParticipantState::CompletedInTime)
        {
            if (ADMMatchPlayerController* Controller = Cast<ADMMatchPlayerController>(GetOwningPlayer()))
            {
                Controller->NotifyQuestCompletedAudio(PublicQuest.RunId);
            }
        }

        BeginQuestExit(PublicQuest.RunId, bAllowCompletionAnimation);
        return;
    }

    if (!bHasCurrentProgress)
    {
        TXT_QuestProgress->SetText(NSLOCTEXT("DMQuestHUD", "ProgressWaiting", "진행 정보 대기 중"));

        TXT_QuestStatus->SetText(NSLOCTEXT("DMQuestHUD", "StatusWaiting", "동기화 중"));

        PB_QuestProgress->SetPercent(0.0f);
    }
    else
    {
        // 상태값은 서버가 확정한 ParticipantState를 사용한다.
        FText StatusText;

        switch (PlayerProgress.State)
        {
        case EDMQuestParticipantState::Active:

            StatusText = RemainingTime > 0.0 ? NSLOCTEXT("DMQuestHUD", "Active", "진행 중")
                                             : NSLOCTEXT("DMQuestHUD", "AwaitingDeadline", "제한시간 판정 대기");
            break;

        case EDMQuestParticipantState::CompletedInTime:

            StatusText = PlayerProgress.Rank > 0
                ? FText::Format(
                    NSLOCTEXT("DMQuestHUD", "CompletedRankFormat", "완료 · {0}위"),
                    FText::AsNumber(PlayerProgress.Rank)
                )
                : NSLOCTEXT("DMQuestHUD", "RankPending", "성공 · 순위 집계 중");
            break;

        case EDMQuestParticipantState::FailedPendingCompletion:

            StatusText = NSLOCTEXT("DMQuestHUD", "FailedPendingSiren", "시간 초과 · 완료할 때까지 사이렌 발생");
            break;

        case EDMQuestParticipantState::CompletedAfterFailure:

            StatusText = NSLOCTEXT("DMQuestHUD", "CompletedAfterFailure", "시간 초과 후 완료");
            break;

        case EDMQuestParticipantState::Excluded:

            StatusText = NSLOCTEXT("DMQuestHUD", "Excluded", "퀘스트 대상 제외");
            break;

        default:

            StatusText = NSLOCTEXT("DMQuestHUD", "UnknownStatus", "진행 정보 대기 중");
            break;
        }

        TXT_QuestStatus->SetText(StatusText);

        float DisplayProgress = FMath::Max(0.0f, PlayerProgress.ProgressValue);

        if (Definition != nullptr)
        {
            const float TargetValue = FMath::Max(0.0f, Definition->TargetValue);

            const bool bMutableState = PlayerProgress.State == EDMQuestParticipantState::Active
                || PlayerProgress.State == EDMQuestParticipantState::FailedPendingCompletion;

            const bool bDurationQuest = Definition->ProgressMode != EDMQuestProgressMode::Count;

            // Duration 진행 중에는 관찰 시작 시각을 이용한다.
            if (bDurationQuest && bMutableState && PlayerProgress.ObservationStartServerTime > 0.0)
            {
                const double Elapsed =
                    FMath::Max(0.0, GameState->GetServerWorldTimeSeconds() - PlayerProgress.ObservationStartServerTime);

                DisplayProgress += static_cast<float>(Elapsed);
            }

            DisplayProgress = FMath::Clamp(DisplayProgress, 0.0f, TargetValue);

            const float ProgressRatio = TargetValue > 0.0f ? DisplayProgress / TargetValue : 0.0f;

            PB_QuestProgress->SetPercent(FMath::Clamp(ProgressRatio, 0.0f, 1.0f));

            // Count와 Duration은 표시 단위가 다르다.
            if (Definition->ProgressMode == EDMQuestProgressMode::Count)
            {
                TXT_QuestProgress->SetText(FText::Format(
                    NSLOCTEXT("DMQuestHUD", "CountProgressFormat", "{0} / {1}"),
                    FText::AsNumber(FMath::RoundToInt(DisplayProgress)),
                    FText::AsNumber(FMath::RoundToInt(TargetValue))
                ));
            }
            else
            {
                const FNumberFormattingOptions OneDecimal = FNumberFormattingOptions()
                    .SetMaximumFractionalDigits(1).SetMinimumFractionalDigits(1);
                TXT_QuestProgress->SetText(FText::Format(
                    NSLOCTEXT("DMQuestHUD", "DurationProgressFormat", "{0} / {1}초"),
                    FText::AsNumber(DisplayProgress, &OneDecimal),
                    FText::AsNumber(TargetValue, &OneDecimal)
                ));
            }
        }
        else
        {
            PB_QuestProgress->SetPercent(0.0f);

            TXT_QuestProgress->SetText(NSLOCTEXT("DMQuestHUD", "TargetUnavailable", "목표 정보 없음"));
        }
    }

    // 제한시간 또는 Duration 관찰 중에는 로컬 표시를 갱신한다.
    const bool bProgressMutable = bHasCurrentProgress
        && (PlayerProgress.State == EDMQuestParticipantState::Active
            || PlayerProgress.State == EDMQuestParticipantState::FailedPendingCompletion);

    const bool bObserving = bProgressMutable && PlayerProgress.ObservationStartServerTime > 0.0;

    const bool bNeedsTimer = RemainingTime > 0.0 || bObserving;

    if (!bNeedsTimer)
    {
        StopRefreshTimer();
        return;
    }

    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    FTimerManager& TimerManager = World->GetTimerManager();

    if (!TimerManager.IsTimerActive(RefreshTimerHandle))
    {
        TimerManager.SetTimer(RefreshTimerHandle, this, &ThisClass::RefreshPresentation, QuestHUDRefreshInterval, true);
    }
}

void UDMQuestHUDWidget::ResetQuestRunPresentation(const int32 NewRunId)
{
    CancelQuestExitAnimation();

    PresentedQuestRunId = NewRunId;
    QuestExitHandledRunId = 0;

    // 이전 종료 애니메이션의 마지막 프레임을 새 퀘스트에 남기지 않는다.
    if (IsValid(Anim_QuestExit))
    {
        SetAnimationCurrentTime(Anim_QuestExit, 0.0f);
    }

    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UDMQuestHUDWidget::CancelQuestExitAnimation()
{
    bQuestExitInProgress = false;
    ActiveQuestExitRunId = 0;

    if (IsValid(Anim_QuestExit) && IsAnimationPlaying(Anim_QuestExit))
    {
        StopAnimation(Anim_QuestExit);
    }
}

void UDMQuestHUDWidget::BeginQuestExit(const int32 RunId, const bool bAllowAnimation)
{
    StopRefreshTimer();

    if (QuestExitHandledRunId == RunId)
    {
        if (!bQuestExitInProgress)
        {
            SetVisibility(ESlateVisibility::Collapsed);
        }

        return;
    }

    QuestExitHandledRunId = RunId;

    if (!bAllowAnimation || !IsValid(Anim_QuestExit) || GetVisibility() == ESlateVisibility::Collapsed)
    {
        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    bQuestExitInProgress = true;
    ActiveQuestExitRunId = RunId;

    if (!PlayAnimation(Anim_QuestExit))
    {
        bQuestExitInProgress = false;
        ActiveQuestExitRunId = 0;
        SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UDMQuestHUDWidget::FinishQuestExit(const int32 ExpectedRunId)
{
    if (ExpectedRunId == 0 || ExpectedRunId != PresentedQuestRunId || ExpectedRunId != QuestExitHandledRunId)
    {
        return;
    }

    SetVisibility(ESlateVisibility::Collapsed);
}

#pragma endregion

#pragma region Presentation Timer

void UDMQuestHUDWidget::StopRefreshTimer()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(RefreshTimerHandle);
    }

    RefreshTimerHandle.Invalidate();
}

#pragma endregion
