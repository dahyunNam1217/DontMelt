
#include "UI/DMRandomEventHUDWidget.h"

#include "Framework/Match/DMMatchGameState.h"
#include "Data/DMRandomEventSetDataAsset.h"

#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Engine/World.h"

namespace
{
    // 공개 이벤트의 예고·진행·종료 경계를 서버 시각으로 확인한다.
    constexpr float RandomEventHUDRefreshInterval = 0.1f;
}

#pragma region Lifecycle

void UDMRandomEventHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 기존 WBP의 이름·진행바는 최종 설명 단일화 전까지 숨긴다.
    if (IsValid(TXT_EventName))
    {
        TXT_EventName->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (IsValid(PB_EventProgress))
    {
        PB_EventProgress->SetVisibility(ESlateVisibility::Collapsed);
    }

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
    // 기존 위젯이 남아 있어도 설명 이외의 요소는 표시하지 않는다.
    if (IsValid(TXT_EventName))
    {
        TXT_EventName->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (IsValid(PB_EventProgress))
    {
        PB_EventProgress->SetVisibility(ESlateVisibility::Collapsed);
    }

    // 설명만 필수 Widget으로 사용한다.
    if (!bPresentationActive || !IsValid(TXT_EventDescription))
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
        DescriptionText = Definition->Description.IsEmpty()
            ? NSLOCTEXT("DMRandomEventHUD", "EmptyDescription", "이벤트가 진행됩니다.")
            : Definition->Description;
    }
    else
    {
        DescriptionText = NSLOCTEXT("DMRandomEventHUD", "MissingEventDescription", "이벤트 정보 확인 중");
    }

    // 별도 상태 TextBlock 대신 설명 문구로 예고를 표현한다.
    if (bWarning)
    {
        DescriptionText =
            FText::Format(NSLOCTEXT("DMRandomEventHUD", "WarningDescription", "곧 시작: {0}"), DescriptionText);
    }

    TXT_EventDescription->SetText(DescriptionText);

    SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    FTimerManager& TimerManager = World->GetTimerManager();

    // 예고 접두사 제거와 종료 시 숨김을 위해 기존 갱신 주기를 유지한다.
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
