
#include "UI/DMStatusHUDWidget.h"

#include "Characters/Components/DMCharacterStateComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Framework/Match/DMMatchPlayerController.h"
#include "Core/DMGameplayTags.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

#pragma region Data Source

void UDMStatusHUDWidget::SetDataSources(
    UDMCharacterStateComponent* InStateComponent, ADMMatchPlayerController* InController
)
{
    const bool bSourcesChanged = BoundStateComponent.Get() != InStateComponent || BoundController.Get() != InController;

    if (bSourcesChanged)
    {
        // 이전 Pawn의 상태이상 변경 알림을 해제한다.
        if (UDMCharacterStateComponent* OldState = BoundStateComponent.Get())
        {
            OldState->OnStatusChanged().RemoveAll(this);
        }

        // 이전 Controller의 시야·발소리 알림을 모두 해제한다.
        if (ADMMatchPlayerController* OldController = BoundController.Get())
        {
            OldController->OnVisionDistortPresentationChanged().RemoveAll(this);

            OldController->OnFootstepAmplifyWarning().RemoveAll(this);
        }

        // Pawn 또는 Controller가 바뀌면 과거 경고를 계승하지 않는다.
        StopFootstepWarningTimer();
        StopInvisibleRefreshTimer();

        FootstepWarningEndServerTime = 0.0;

        BoundStateComponent = InStateComponent;
        BoundController = InController;

        if (IsValid(InStateComponent))
        {
            InStateComponent->OnStatusChanged().AddUObject(this, &ThisClass::HandleStatusChanged);
        }

        if (IsValid(InController))
        {
            InController->OnVisionDistortPresentationChanged().AddUObject(
                this, &ThisClass::HandleVisionPresentationChanged
            );

            InController->OnFootstepAmplifyWarning().AddUObject(this, &ThisClass::HandleFootstepAmplifyWarning);
        }
    }

    // 복제 알림보다 Widget 연결이 늦어도 현재 상태를 표시한다.
    RefreshPresentation();
}

void UDMStatusHUDWidget::SetPresentationActive(const bool bActive)
{
    if (bPresentationActive == bActive)
    {
        return;
    }

    bPresentationActive = bActive;

    if (!bPresentationActive)
    {
        // 사망·관전·경기 종료 시 발소리 피해 안내도 정리한다.
        StopFootstepWarningTimer();
        StopInvisibleRefreshTimer();

        FootstepWarningEndServerTime = 0.0;
    }

    RefreshPresentation();
}

#pragma endregion

#pragma region Presentation

void UDMStatusHUDWidget::HandleStatusChanged()
{
    RefreshPresentation();
}

void UDMStatusHUDWidget::HandleVisionPresentationChanged(bool /*bActive*/
)
{
    RefreshPresentation();
}

void UDMStatusHUDWidget::HandleFootstepAmplifyWarning(const double EffectEndServerTime)
{
    if (!bPresentationActive)
    {
        return;
    }

    UWorld* World = GetWorld();

    const AGameStateBase* GameState = IsValid(World) ? World->GetGameState<AGameStateBase>() : nullptr;

    if (!IsValid(GameState))
    {
        return;
    }

    const double ServerNow = GameState->GetServerWorldTimeSeconds();

    // 이미 끝난 효과에 대한 지연 RPC는 표시하지 않는다.
    if (!FMath::IsFinite(EffectEndServerTime) || EffectEndServerTime <= ServerNow)
    {
        return;
    }

    // 여러 경고가 겹치면 가장 늦은 종료 시각을 사용한다.
    FootstepWarningEndServerTime = FMath::Max(FootstepWarningEndServerTime, EffectEndServerTime);

    const double RemainingSeconds = FootstepWarningEndServerTime - ServerNow;

    StopFootstepWarningTimer();

    // 예정 종료 시점에만 갱신하며 매 프레임 확인하지 않는다.
    World->GetTimerManager().SetTimer(
        FootstepWarningTimerHandle,
        this,
        &ThisClass::HandleFootstepWarningExpired,
        static_cast<float>(RemainingSeconds),
        false
    );

    RefreshPresentation();
}

void UDMStatusHUDWidget::HandleFootstepWarningExpired()
{
    // 서버 효과 종료 시각에 맞춰 로컬 표시를 제거한다.
    FootstepWarningEndServerTime = 0.0;

    StopFootstepWarningTimer();
    RefreshPresentation();
}

void UDMStatusHUDWidget::StopFootstepWarningTimer()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(FootstepWarningTimerHandle);
    }

    FootstepWarningTimerHandle.Invalidate();
}

void UDMStatusHUDWidget::StopInvisibleRefreshTimer()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(InvisibleRefreshTimerHandle);
    }

    InvisibleRefreshTimerHandle.Invalidate();
}

void UDMStatusHUDWidget::RefreshPresentation()
{
    if (!bPresentationActive)
    {
        StopInvisibleRefreshTimer();
        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    const UDMCharacterStateComponent* State = BoundStateComponent.Get();

    const ADMMatchPlayerController* Controller = BoundController.Get();

    const auto SetStatusVisibility = [](UTextBlock* TextWidget, const bool bActive)
    {
        if (IsValid(TextWidget))
        {
            TextWidget->SetVisibility(bActive ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
        }
    };

    // 현재 플레이어에게 적용된 실제 상태 태그만 읽는다.
    const bool bStunned = IsValid(State) && State->HasStatus(DMGameplayTags::Status_Stunned);

    const bool bInvisible = IsValid(State) && State->HasStatus(DMGameplayTags::Status_Invisible);

    const bool bSpawnProtected = IsValid(State) && State->HasStatus(DMGameplayTags::Status_SpawnProtected);

    // 시야 교란은 기존 로컬 PostProcess 연출 상태를 따른다.
    const bool bVisionDistorted = IsValid(Controller) && Controller->IsVisionDistortPresentationActive();

    // 발소리 증폭 피해 경고는 서버가 전달한 종료 시각을 따른다.
    bool bFootstepObserved = false;

    UWorld* World = GetWorld();

    const AGameStateBase* GameState = IsValid(World) ? World->GetGameState<AGameStateBase>() : nullptr;

    if (IsValid(GameState))
    {
        bFootstepObserved = FootstepWarningEndServerTime > GameState->GetServerWorldTimeSeconds();
    }

    SetStatusVisibility(TXT_Stunned, bStunned);
    SetStatusVisibility(TXT_Invisible, bInvisible);
    SetStatusVisibility(TXT_SpawnProtected, bSpawnProtected);
    SetStatusVisibility(TXT_VisionDistorted, bVisionDistorted);
    SetStatusVisibility(TXT_FootstepObserved, bFootstepObserved);

    // 투명화 효과의 남은 시간은 서버 기준 종료 시각에서 계산한다.
    const FDMInvisibleTiming InvisibleTiming = IsValid(State) ? State->GetInvisibleTiming() : FDMInvisibleTiming();

    const bool bHasInvisibleTiming =
        bInvisible && IsValid(GameState) && InvisibleTiming.EndServerTime > InvisibleTiming.StartServerTime;

    const double InvisibleDuration =
        bHasInvisibleTiming ? InvisibleTiming.EndServerTime - InvisibleTiming.StartServerTime : 0.0;

    const double InvisibleRemaining = bHasInvisibleTiming
        ? FMath::Max(0.0, InvisibleTiming.EndServerTime - GameState->GetServerWorldTimeSeconds())
        : 0.0;

    if (IsValid(TXT_InvisibleRemaining))
    {
        TXT_InvisibleRemaining->SetVisibility(
            bHasInvisibleTiming ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed
        );

        if (bHasInvisibleTiming)
        {
            TXT_InvisibleRemaining->SetText(FText::FromString(FString::Printf(TEXT("%.1f초"), InvisibleRemaining)));
        }
    }

    if (IsValid(PB_InvisibleRemaining))
    {
        PB_InvisibleRemaining->SetVisibility(
            bHasInvisibleTiming ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed
        );

        if (bHasInvisibleTiming)
        {
            PB_InvisibleRemaining->SetPercent(
                InvisibleDuration > 0.0
                    ? FMath::Clamp(static_cast<float>(InvisibleRemaining / InvisibleDuration), 0.0f, 1.0f)
                    : 0.0f
            );
        }
    }

    // 투명화 중에만 로컬 HUD 갱신 Timer를 실행한다.
    const bool bNeedsInvisibleRefresh = bHasInvisibleTiming && InvisibleRemaining > 0.0;

    if (bNeedsInvisibleRefresh && IsValid(World)
        && !World->GetTimerManager().IsTimerActive(InvisibleRefreshTimerHandle))
    {
        World->GetTimerManager().SetTimer(
            InvisibleRefreshTimerHandle, this, &ThisClass::RefreshPresentation, 0.1f, true
        );
    }
    else if (!bNeedsInvisibleRefresh)
    {
        StopInvisibleRefreshTimer();
    }

    const bool bAnyStatusActive = bStunned || bInvisible || bSpawnProtected || bVisionDistorted || bFootstepObserved;

    SetVisibility(bAnyStatusActive ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

#pragma endregion

#pragma region Lifecycle

void UDMStatusHUDWidget::NativeDestruct()
{
    SetPresentationActive(false);
    SetDataSources(nullptr, nullptr);

    StopFootstepWarningTimer();
    StopInvisibleRefreshTimer();
    FootstepWarningEndServerTime = 0.0;

    Super::NativeDestruct();
}

#pragma endregion
