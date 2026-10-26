
#include "UI/DMSpectatorHUDWidget.h"

#include "Framework/Match/DMMatchPlayerController.h"

#include "Components/TextBlock.h"
#include "Engine/World.h"

namespace
{
    // 관전 중 남은 초 표시만 0.25초 간격으로 갱신한다.
    constexpr float SpectatorHUDRefreshInterval = 0.25f;
}

#pragma region Lifecycle

void UDMSpectatorHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 데이터 소스가 연결되기 전에는 HUD를 숨긴다.
    SetVisibility(ESlateVisibility::Collapsed);
}

void UDMSpectatorHUDWidget::NativeDestruct()
{
    SetPresentationActive(false);
    SetController(nullptr);

    StopRefreshTimer();

    Super::NativeDestruct();
}

#pragma endregion

#pragma region Data Source

void UDMSpectatorHUDWidget::SetController(ADMMatchPlayerController* InController)
{
    if (BoundController.Get() == InController)
    {
        // 동일 Controller여도 현재 관전 단계는 다시 확인한다.
        RefreshPresentation();
        return;
    }

    // 이전 Controller가 있다면 변경 알림을 해제한다.
    if (ADMMatchPlayerController* OldController = BoundController.Get())
    {
        OldController->OnLocalSpectatorPhaseChanged().RemoveAll(this);
    }

    StopRefreshTimer();

    BoundController = InController;

    if (IsValid(InController))
    {
        InController->OnLocalSpectatorPhaseChanged().AddUObject(this, &ThisClass::HandleLocalSpectatorPhaseChanged);
    }

    RefreshPresentation();
}

void UDMSpectatorHUDWidget::SetPresentationActive(const bool bActive)
{
    if (bPresentationActive == bActive)
    {
        if (bActive)
        {
            RefreshPresentation();
        }

        return;
    }

    bPresentationActive = bActive;

    if (!bPresentationActive)
    {
        HideSpectatorHUD();
        return;
    }

    RefreshPresentation();
}

#pragma endregion

#pragma region Presentation

void UDMSpectatorHUDWidget::HandleLocalSpectatorPhaseChanged(EDMSpectatorPhase /*NewPhase*/
)
{
    // 전달받은 값을 별도로 저장하지 않고 Controller를 재조회한다.
    RefreshPresentation();
}

void UDMSpectatorHUDWidget::RefreshPresentation()
{
    const ADMMatchPlayerController* Controller = BoundController.Get();

    if (!bPresentationActive || !IsValid(Controller) || !Controller->IsLocalController()
        || !Controller->IsSpectatorInformationRestricted() || !IsValid(TXT_SpectatorPhase)
        || !IsValid(TXT_SpectatorHint) || !IsValid(TXT_RespawnTime))
    {
        HideSpectatorHUD();
        return;
    }

    const EDMSpectatorPhase Phase = Controller->GetLocalSpectatorPhase();

    // 마지막 Fade의 숫자 표시는 기존 CountdownWidget이 소유한다.
    if (Phase == EDMSpectatorPhase::Fade)
    {
        HideSpectatorHUD();
        return;
    }

    const double Remaining = Controller->GetRemainingSpectatorTimeSeconds();

    if (Remaining <= 0.0)
    {
        HideSpectatorHUD();
        return;
    }

    FText PhaseText;
    FText HintText;

    switch (Phase)
    {
    case EDMSpectatorPhase::Corpse:

        PhaseText = NSLOCTEXT("DMSpectatorHUD", "CorpsePhase", "사망 지점 관전");

        HintText = NSLOCTEXT("DMSpectatorHUD", "CorpseHint", "쓰러진 위치를 보여줍니다.");
        break;

    case EDMSpectatorPhase::KillerView:

        PhaseText = NSLOCTEXT("DMSpectatorHUD", "KillerPhase", "공격자 관찰 구간");

        HintText = NSLOCTEXT("DMSpectatorHUD", "KillerHint", "공격자를 볼 수 없으면 사망 지점으로 전환됩니다.");
        break;

    case EDMSpectatorPhase::FreeCamera:

        PhaseText = NSLOCTEXT("DMSpectatorHUD", "FreeCameraPhase", "자유 카메라");

        HintText = NSLOCTEXT("DMSpectatorHUD", "FreeCameraHint", "WASD 이동 · 마우스로 시점 변경");
        break;

    default:

        HideSpectatorHUD();
        return;
    }

    TXT_SpectatorPhase->SetText(PhaseText);
    TXT_SpectatorHint->SetText(HintText);

    SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    RefreshRemainingTime();

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    FTimerManager& Timers = World->GetTimerManager();

    if (!Timers.IsTimerActive(RefreshTimerHandle))
    {
        Timers.SetTimer(RefreshTimerHandle, this, &ThisClass::RefreshRemainingTime, SpectatorHUDRefreshInterval, true);
    }
}

void UDMSpectatorHUDWidget::RefreshRemainingTime()
{
    const ADMMatchPlayerController* Controller = BoundController.Get();

    if (!bPresentationActive || !IsValid(Controller) || !Controller->IsSpectatorInformationRestricted()
        || Controller->GetLocalSpectatorPhase() == EDMSpectatorPhase::Fade)
    {
        HideSpectatorHUD();
        return;
    }

    const double Remaining = Controller->GetRemainingSpectatorTimeSeconds();

    if (Remaining <= 0.0)
    {
        HideSpectatorHUD();
        return;
    }

    const int32 DisplaySeconds = FMath::Max(0, FMath::CeilToInt(static_cast<float>(Remaining)));

    if (IsValid(TXT_RespawnTime))
    {
        TXT_RespawnTime->SetText(FText::Format(
            NSLOCTEXT("DMSpectatorHUD", "RespawnRemaining", "부활까지 {0}초"), FText::AsNumber(DisplaySeconds)
        ));
    }
}

void UDMSpectatorHUDWidget::HideSpectatorHUD()
{
    StopRefreshTimer();

    SetVisibility(ESlateVisibility::Collapsed);
}

void UDMSpectatorHUDWidget::StopRefreshTimer()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(RefreshTimerHandle);
    }

    RefreshTimerHandle.Invalidate();
}

#pragma endregion
