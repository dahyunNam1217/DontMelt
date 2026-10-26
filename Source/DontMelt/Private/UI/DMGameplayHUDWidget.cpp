
#include "UI/DMGameplayHUDWidget.h"

#include "UI/DMCombatHUDWidget.h"
#include "UI/DMSkillBarWidget.h"
#include "UI/DMRelicHUDWidget.h"

#include "Characters/DMPlayerCharacter.h"
#include "Framework/Match/DMMatchGameState.h"
#include "Framework/Match/DMMatchPlayerController.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"

namespace
{
    constexpr float MatchHUDRefreshInterval = 0.25f;
}

#pragma region Data Source

void UDMGameplayHUDWidget::SetMatchGameState(ADMMatchGameState* InGameState)
{
    if (BoundGameState.Get() == InGameState)
    {
        RefreshFromMatchState();
        return;
    }

    StopDisplayTimer();

    BoundGameState = InGameState;

    RefreshFromMatchState();
}

void UDMGameplayHUDWidget::SetCombatComponent(UDMCombatComponent* InCombatComponent)
{
    if (IsValid(CombatHUD))
    {
        CombatHUD->SetCombatComponent(InCombatComponent);
    }
}

void UDMGameplayHUDWidget::SetSkillPlayerState(ADMMatchPlayerState* InPlayerState)
{
    if (IsValid(SkillBar))
    {
        SkillBar->SetPlayerState(InPlayerState);
    }
}

void UDMGameplayHUDWidget::SetRelicHUDSources(ADMMatchPlayerController* InController, ADMPlayerCharacter* InCharacter)
{
    if (!IsValid(RelicHUD))
    {
        return;
    }

    // 현재 Pawn이 준비되지 않았으면 이전 연결을 정리한다.
    if (!IsValid(InController) || !IsValid(InCharacter))
    {
        RelicHUD->ShutdownRelicHUD();
        return;
    }

    // 기존 유물 HUD가 가진 초기 상태 갱신과
    // Delegate 연결·해제 로직을 그대로 재사용한다.
    RelicHUD->InitializeRelicHUD(InController, InCharacter);
}

void UDMGameplayHUDWidget::SetPresentationActive(const bool bActive)
{
    if (bPresentationActive == bActive)
    {
        return;
    }

    bPresentationActive = bActive;

    // 부모 GameplayHUD의 활성화 상태를 전투 HUD에도 전달한다.
    if (IsValid(CombatHUD))
    {
        CombatHUD->SetPresentationActive(bActive);
    }

    // 일반 HUD와 스킬 바의 표시 갱신 수명을 일치시킨다.
    if (IsValid(SkillBar))
    {
        SkillBar->SetPresentationActive(bActive);
    }

    if (!bPresentationActive)
    {
        StopDisplayTimer();
        return;
    }

    RefreshFromMatchState();
}

#pragma endregion

#pragma region Match Presentation

void UDMGameplayHUDWidget::RefreshFromMatchState()
{
    if (!bPresentationActive || !IsValid(TXT_MatchPhase) || !IsValid(TXT_RemainingTime))
    {
        return;
    }

    const ADMMatchGameState* GameState = BoundGameState.Get();

    if (!IsValid(GameState))
    {
        TXT_MatchPhase->SetText(NSLOCTEXT("DMGameplayHUD", "WaitingForMatchData", "매치 정보 대기 중"));

        TXT_RemainingTime->SetText(FText::FromString(TEXT("--:--")));

        StopDisplayTimer();
        return;
    }

    const FDMMatchPhaseState PhaseState = GameState->GetPhaseState();

    FText PhaseText;

    bool bHasPhaseTimer = false;

    switch (PhaseState.CurrentPhase)
    {
    case EDMMatchPhase::Waiting:

        PhaseText = NSLOCTEXT("DMGameplayHUD", "PhaseWaiting", "대기 중");
        break;

    case EDMMatchPhase::Countdown:

        PhaseText = NSLOCTEXT("DMGameplayHUD", "PhaseCountdown", "게임 시작까지");
        bHasPhaseTimer = true;
        break;

    case EDMMatchPhase::Playing:

        PhaseText = NSLOCTEXT("DMGameplayHUD", "PhasePlaying", "남은 시간");
        bHasPhaseTimer = true;
        break;

    case EDMMatchPhase::OvertimeIntro:

        PhaseText = NSLOCTEXT("DMGameplayHUD", "PhaseOvertimeIntro", "연장전 준비");

        bHasPhaseTimer = true;
        break;

    case EDMMatchPhase::Overtime:

        PhaseText = NSLOCTEXT("DMGameplayHUD", "PhaseOvertime", "연장전");
        bHasPhaseTimer = true;
        break;

    case EDMMatchPhase::Finished:

        PhaseText = NSLOCTEXT("DMGameplayHUD", "PhaseFinished", "경기 종료");
        break;

    case EDMMatchPhase::Aborted:

        PhaseText = NSLOCTEXT("DMGameplayHUD", "PhaseAborted", "경기 중단");
        break;

    default:

        PhaseText = FText::GetEmpty();
        break;
    }

    TXT_MatchPhase->SetText(PhaseText);

    if (!bHasPhaseTimer || PhaseState.PhaseEndServerTime <= 0.0)
    {
        TXT_RemainingTime->SetText(FText::FromString(TEXT("--:--")));

        StopDisplayTimer();
        return;
    }

    const double RemainingSeconds = GameState->GetRemainingPhaseTime();

    const int32 DisplaySeconds = FMath::Max(0, FMath::CeilToInt(static_cast<float>(RemainingSeconds)));

    const int32 Minutes = DisplaySeconds / 60;
    const int32 Seconds = DisplaySeconds % 60;

    const FString TimeString = FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);

    TXT_RemainingTime->SetText(FText::FromString(TimeString));

    if (RemainingSeconds <= 0.0)
    {
        StopDisplayTimer();
        return;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    FTimerManager& TimerManager = World->GetTimerManager();

    if (!TimerManager.IsTimerActive(DisplayTimerHandle))
    {
        TimerManager.SetTimer(
            DisplayTimerHandle, this, &ThisClass::RefreshFromMatchState, MatchHUDRefreshInterval, true
        );
    }
}

#pragma endregion

#pragma region Timer Cleanup

void UDMGameplayHUDWidget::StopDisplayTimer()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DisplayTimerHandle);
    }

    DisplayTimerHandle.Invalidate();
}

#pragma endregion

#pragma region Lifecycle

void UDMGameplayHUDWidget::NativeDestruct()
{
    bPresentationActive = false;

    StopDisplayTimer();

    // 자식 전투 HUD의 이전 Pawn 연결도 함께 해제한다.
    if (IsValid(CombatHUD))
    {
        CombatHUD->SetPresentationActive(false);
        CombatHUD->SetCombatComponent(nullptr);
    }

    // 부모 HUD 종료 시 스킬 바의 연결과 타이머도 해제한다.
    if (IsValid(SkillBar))
    {
        SkillBar->SetPresentationActive(false);
        SkillBar->SetPlayerState(nullptr);
    }

    // GameplayHUD가 제거되면 기존 유물 HUD 연결도 정리한다.
    SetRelicHUDSources(nullptr, nullptr);

    BoundGameState.Reset();

    Super::NativeDestruct();
}

#pragma endregion
