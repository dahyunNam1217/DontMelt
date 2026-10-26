
#include "UI/DMOvertimeIntroWidget.h"

#include "Framework/Match/DMMatchGameState.h"

#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"

namespace
{
    // Intro가 진행되는 짧은 시간 동안만 카운트다운을 갱신한다.
    constexpr float OvertimeIntroRefreshInterval = 0.1f;
}

#pragma region Lifecycle

void UDMOvertimeIntroWidget::NativeConstruct()
{
    Super::NativeConstruct();

    SetVisibility(ESlateVisibility::Collapsed);
}

void UDMOvertimeIntroWidget::NativeDestruct()
{
    SetPresentationActive(false);
    SetGameState(nullptr);

    StopRefreshTimer();

    Super::NativeDestruct();
}

#pragma endregion

#pragma region Data Source

void UDMOvertimeIntroWidget::SetGameState(ADMMatchGameState* InGameState)
{
    if (BoundGameState.Get() == InGameState)
    {
        RefreshPresentation();
        return;
    }

    // 이전 GameState의 복제 변경 알림을 해제한다.
    if (ADMMatchGameState* OldGameState = BoundGameState.Get())
    {
        OldGameState->OnPhaseStateChanged().RemoveAll(this);

        OldGameState->OnOvertimeIntroStateChanged().RemoveAll(this);
    }

    HideIntro();

    BoundGameState = InGameState;

    if (IsValid(InGameState))
    {
        InGameState->OnPhaseStateChanged().AddUObject(this, &ThisClass::HandlePhaseStateChanged);

        InGameState->OnOvertimeIntroStateChanged().AddUObject(this, &ThisClass::HandleOvertimeIntroStateChanged);
    }

    // RepNotify를 이미 수신한 경우도 복구한다.
    RefreshPresentation();
}

void UDMOvertimeIntroWidget::SetPresentationActive(const bool bActive)
{
    if (bPresentationActive == bActive)
    {
        return;
    }

    bPresentationActive = bActive;

    if (!bPresentationActive)
    {
        HideIntro();
        return;
    }

    RefreshPresentation();
}

#pragma endregion

#pragma region Data Binding

void UDMOvertimeIntroWidget::HandlePhaseStateChanged(
    ADMMatchGameState* Source, FDMMatchPhaseState /*NewPhase*/
)
{
    if (Source != BoundGameState.Get())
    {
        return;
    }

    RefreshPresentation();
}

void UDMOvertimeIntroWidget::HandleOvertimeIntroStateChanged(
    ADMMatchGameState* Source, FDMOvertimeIntroState /*NewState*/
)
{
    if (Source != BoundGameState.Get())
    {
        return;
    }

    RefreshPresentation();
}

#pragma endregion

#pragma region Presentation

bool UDMOvertimeIntroWidget::RebuildBaseRows(const TArray<FDMOvertimeBaseCount>& Entries)
{
    if (!IsValid(VB_BaseRows) || !IsValid(WidgetTree) || Entries.IsEmpty())
    {
        return false;
    }

    VB_BaseRows->ClearChildren();

    for (const FDMOvertimeBaseCount& Entry : Entries)
    {
        UTextBlock* RowText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());

        if (!IsValid(RowText))
        {
            VB_BaseRows->ClearChildren();
            return false;
        }

        RowText->SetText(FText::Format(
            NSLOCTEXT("DMOvertimeIntro", "BaseCountRow", "진영 {0}  |  등록 유물 {1}개"),
            FText::AsNumber(Entry.BaseSlot + 1),
            FText::AsNumber(Entry.RegisteredRelicCount)
        ));

        RowText->SetJustification(ETextJustify::Center);

        RowText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

        VB_BaseRows->AddChildToVerticalBox(RowText);
    }

    return VB_BaseRows->GetChildrenCount() == Entries.Num();
}

void UDMOvertimeIntroWidget::RefreshPresentation()
{
    const ADMMatchGameState* GameState = BoundGameState.Get();

    if (!bPresentationActive || !IsValid(GameState) || !IsValid(VB_BaseRows) || !IsValid(TXT_IntroTime))
    {
        HideIntro();
        return;
    }

    const FDMMatchPhaseState PhaseState = GameState->GetPhaseState();

    if (PhaseState.CurrentPhase != EDMMatchPhase::OvertimeIntro)
    {
        HideIntro();
        return;
    }

    const FDMOvertimeIntroState IntroState = GameState->GetOvertimeIntroState();

    // 복제 도착 순서가 달라도 같은 Intro의 데이터만 표시한다.
    const bool bMatchingSnapshot = IntroState.HasPayload()
        && IntroState.BaseCounts.Num() == GameState->GetStartingPlayerCount()
        && FMath::IsNearlyEqual(IntroState.IntroEndServerTime, PhaseState.PhaseEndServerTime, 0.01);

    if (!bMatchingSnapshot)
    {
        HideIntro();
        return;
    }

    const double Remaining = PhaseState.PhaseEndServerTime - GameState->GetServerWorldTimeSeconds();

    if (Remaining <= 0.0)
    {
        HideIntro();
        return;
    }

    if (DisplayedIntroEndServerTime != IntroState.IntroEndServerTime)
    {
        if (!RebuildBaseRows(IntroState.BaseCounts))
        {
            HideIntro();
            return;
        }

        DisplayedIntroEndServerTime = IntroState.IntroEndServerTime;
    }

    SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    RefreshRemainingTime();

    if (UWorld* World = GetWorld())
    {
        FTimerManager& Timers = World->GetTimerManager();

        if (!Timers.IsTimerActive(RefreshTimerHandle))
        {
            Timers.SetTimer(
                RefreshTimerHandle, this, &ThisClass::RefreshRemainingTime, OvertimeIntroRefreshInterval, true
            );
        }
    }
}

void UDMOvertimeIntroWidget::RefreshRemainingTime()
{
    const ADMMatchGameState* GameState = BoundGameState.Get();

    if (!bPresentationActive || !IsValid(GameState) || GameState->GetCurrentPhase() != EDMMatchPhase::OvertimeIntro)
    {
        HideIntro();
        return;
    }

    const double Remaining = GameState->GetRemainingPhaseTime();

    if (Remaining <= 0.0)
    {
        HideIntro();
        return;
    }

    if (IsValid(TXT_IntroTime))
    {
        const int32 WholeSeconds = FMath::Max(0, FMath::CeilToInt(Remaining));

        TXT_IntroTime->SetText(
            FText::FromString(FString::Printf(TEXT("%02d:%02d"), WholeSeconds / 60, WholeSeconds % 60))
        );
    }
}

void UDMOvertimeIntroWidget::HideIntro()
{
    StopRefreshTimer();

    DisplayedIntroEndServerTime = 0.0;

    if (IsValid(VB_BaseRows))
    {
        VB_BaseRows->ClearChildren();
    }

    SetVisibility(ESlateVisibility::Collapsed);
}

void UDMOvertimeIntroWidget::StopRefreshTimer()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(RefreshTimerHandle);
    }

    RefreshTimerHandle.Invalidate();
}

#pragma endregion
