
#include "UI/DMRandomEventHUDWidget.h"

#include "Framework/Match/DMMatchGameState.h"
#include "Data/DMRandomEventSetDataAsset.h"

#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Engine/World.h"

namespace
{
    // 이벤트 공개 중에만 진행 바를 갱신한다.
    constexpr float RandomEventHUDRefreshInterval = 0.1f;
}

#pragma region Lifecycle

void UDMRandomEventHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 공개 이벤트가 없을 때는 HUD를 숨긴다.
    SetVisibility(ESlateVisibility::Collapsed);
}

void UDMRandomEventHUDWidget::NativeDestruct()
{
    // 데이터 구독과 로컬 표시 타이머를 정리한다.
    SetPresentationActive(false);
    SetGameState(nullptr);

    StopRefreshTimer();

    Super::NativeDestruct();
}

#pragma endregion

#pragma region Data Source

void UDMRandomEventHUDWidget::SetGameState(ADMMatchGameState* InGameState)
{
    if (BoundGameState.Get() == InGameState)
    {
        RefreshPresentation();
        return;
    }

    StopRefreshTimer();

    // 이전 GameState의 이벤트 변경 알림을 해제한다.
    if (ADMMatchGameState* OldGameState = BoundGameState.Get())
    {
        OldGameState->OnRandomEventPublicStateChanged().RemoveAll(this);
    }

    BoundGameState = InGameState;

    if (IsValid(InGameState))
    {
        InGameState->OnRandomEventPublicStateChanged().AddUObject(
            this, &ThisClass::HandleRandomEventPublicStateChanged
        );
    }

    // 현재 공개 상태를 즉시 반영한다.
    RefreshPresentation();
}

void UDMRandomEventHUDWidget::SetPresentationActive(const bool bActive)
{
    if (bPresentationActive == bActive)
    {
        return;
    }

    bPresentationActive = bActive;

    if (!bPresentationActive)
    {
        StopRefreshTimer();
        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    RefreshPresentation();
}

#pragma endregion

#pragma region Event Presentation

const FDMRandomEventDefinition* UDMRandomEventHUDWidget::FindEventDefinition(const FGameplayTag& EventTag) const
{
    if (!IsValid(RandomEventSetData) || !EventTag.IsValid())
    {
        return nullptr;
    }

    // 정적 Data Asset에서 이벤트 표시 정보를 찾는다.
    for (const FDMRandomEventDefinition& Definition : RandomEventSetData->GetEventDefinitions())
    {
        if (Definition.EventTag == EventTag)
        {
            return &Definition;
        }
    }

    return nullptr;
}

void UDMRandomEventHUDWidget::HandleRandomEventPublicStateChanged(
    ADMMatchGameState* Source, FDMRandomEventRuntimeState /*NewState*/
)
{
    if (Source != BoundGameState.Get())
    {
        return;
    }

    RefreshPresentation();
}

void UDMRandomEventHUDWidget::RefreshPresentation()
{
    // 이름·설명·진행 바만 필수 Widget으로 사용한다.
    if (!bPresentationActive || !IsValid(TXT_EventName) || !IsValid(TXT_EventDescription) || !IsValid(PB_EventProgress))
    {
        StopRefreshTimer();
        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    const ADMMatchGameState* GameState = BoundGameState.Get();

    if (!IsValid(GameState))
    {
        StopRefreshTimer();
        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    const FDMRandomEventRuntimeState EventState = GameState->GetRandomEventPublicState();

    // 공개 중인 이벤트가 없으면 HUD를 숨긴다.
    if (!EventState.IsScheduledOrActive())
    {
        StopRefreshTimer();
        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    const double ServerNow = GameState->GetServerWorldTimeSeconds();

    // 서버 기준 이벤트 종료 시각에 도달하면 표시를 끝낸다.
    if (ServerNow >= EventState.EndServerTime)
    {
        StopRefreshTimer();
        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    // 예고 구간과 실제 진행 구간을 구분한다.
    const bool bWarning = ServerNow < EventState.StartServerTime;

    const FDMRandomEventDefinition* Definition = FindEventDefinition(EventState.EventTag);

    FText DescriptionText;

    if (Definition != nullptr)
    {
        // 이벤트 이름은 Data Asset의 DisplayName을 사용한다.
        TXT_EventName->SetText(
            Definition->DisplayName.IsEmpty() ? NSLOCTEXT("DMRandomEventHUD", "DefaultEventName", "랜덤 이벤트")
                                              : Definition->DisplayName
        );

        DescriptionText = Definition->Description.IsEmpty()
            ? NSLOCTEXT("DMRandomEventHUD", "EmptyDescription", "이벤트가 진행됩니다.")
            : Definition->Description;
    }
    else
    {
        TXT_EventName->SetText(NSLOCTEXT("DMRandomEventHUD", "MissingEventDefinition", "랜덤 이벤트"));

        DescriptionText = NSLOCTEXT("DMRandomEventHUD", "MissingEventDescription", "이벤트 정보 확인 중");
    }

    // 별도 상태 TextBlock 대신 설명 문구로 예고를 표현한다.
    if (bWarning)
    {
        DescriptionText =
            FText::Format(NSLOCTEXT("DMRandomEventHUD", "WarningDescription", "곧 시작: {0}"), DescriptionText);
    }

    TXT_EventDescription->SetText(DescriptionText);

    // 기존 GameState의 시간 Query를 재사용한다.
    const double RemainingSeconds =
        bWarning ? GameState->GetRemainingRandomEventWarningTime() : GameState->GetRemainingRandomEventTime();

    // 진행 바의 기준 구간을 계산한다.
    const double WindowDuration = bWarning ? EventState.StartServerTime - EventState.WarningStartServerTime
                                           : EventState.EndServerTime - EventState.StartServerTime;

    // 예고와 진행 구간의 경과 비율을 계산한다.
    const double Progress = WindowDuration > KINDA_SMALL_NUMBER ? 1.0 - RemainingSeconds / WindowDuration : 1.0;

    PB_EventProgress->SetPercent(static_cast<float>(FMath::Clamp(Progress, 0.0, 1.0)));

    SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    FTimerManager& TimerManager = World->GetTimerManager();

    // 이벤트 진행 중에만 로컬 진행 바를 주기적으로 갱신한다.
    if (!TimerManager.IsTimerActive(RefreshTimerHandle))
    {
        TimerManager.SetTimer(
            RefreshTimerHandle, this, &ThisClass::RefreshPresentation, RandomEventHUDRefreshInterval, true
        );
    }
}

#pragma endregion

#pragma region Presentation Timer

void UDMRandomEventHUDWidget::StopRefreshTimer()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(RefreshTimerHandle);
    }

    RefreshTimerHandle.Invalidate();
}

#pragma endregion
