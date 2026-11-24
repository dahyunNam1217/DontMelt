#include "UI/DMStatusHUDWidget.h"

#include "Characters/Components/DMCharacterStateComponent.h"
#include "Core/DMGameplayTags.h"
#include "Framework/Match/DMMatchPlayerController.h"

#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogDMStatusHUD, Log, All);

namespace
{
    constexpr float SkillStatusRefreshInterval = 0.1f;

    struct FDMSkillStatusCandidate
    {
        FText Message;
        double StartServerTime = 0.0;
        double EndServerTime = 0.0;
        int32 TieBreakPriority = INDEX_NONE;

        bool IsActive(const double ServerNow) const
        {
            return TieBreakPriority != INDEX_NONE && EndServerTime > StartServerTime && EndServerTime > ServerNow;
        }
    };

    void ConsiderSkillStatusCandidate(
        FDMSkillStatusCandidate& Selected,
        const FText& Message,
        const double StartServerTime,
        const double EndServerTime,
        const int32 TieBreakPriority,
        const double ServerNow
    )
    {
        FDMSkillStatusCandidate Candidate;
        Candidate.Message = Message;
        Candidate.StartServerTime = StartServerTime;
        Candidate.EndServerTime = EndServerTime;
        Candidate.TieBreakPriority = TieBreakPriority;

        if (!Candidate.IsActive(ServerNow))
        {
            return;
        }

        const bool bStartsLater = Candidate.StartServerTime > Selected.StartServerTime;
        const bool bSameStart = FMath::IsNearlyEqual(Candidate.StartServerTime, Selected.StartServerTime);

        if (!Selected.IsActive(ServerNow) || bStartsLater
            || (bSameStart && Candidate.TieBreakPriority > Selected.TieBreakPriority))
        {
            Selected = MoveTemp(Candidate);
        }
    }
}

#pragma region Data Source

void UDMStatusHUDWidget::SetDataSources(
    UDMCharacterStateComponent* InStateComponent, ADMMatchPlayerController* InController
)
{
    const bool bSourcesChanged = BoundStateComponent.Get() != InStateComponent || BoundController.Get() != InController;

    if (bSourcesChanged)
    {
        if (UDMCharacterStateComponent* OldState = BoundStateComponent.Get())
        {
            OldState->OnStatusChanged().RemoveAll(this);
        }

        if (ADMMatchPlayerController* OldController = BoundController.Get())
        {
            OldController->OnVisionDistortPresentationChanged().RemoveAll(this);
            OldController->OnFootstepAmplifyWarning().RemoveAll(this);
        }

        StopSkillStatusRefreshTimer();
        FootstepWarningStartServerTime = 0.0;
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
        StopSkillStatusRefreshTimer();
        FootstepWarningStartServerTime = 0.0;
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

void UDMStatusHUDWidget::HandleVisionPresentationChanged(bool /*bActive*/)
{
    RefreshPresentation();
}

void UDMStatusHUDWidget::HandleFootstepAmplifyWarning(
    const double EffectStartServerTime, const double EffectEndServerTime
)
{
    if (!bPresentationActive)
    {
        return;
    }

    const UWorld* World = GetWorld();
    const AGameStateBase* GameState = IsValid(World) ? World->GetGameState<AGameStateBase>() : nullptr;

    if (!IsValid(GameState) || !FMath::IsFinite(EffectStartServerTime) || !FMath::IsFinite(EffectEndServerTime)
        || EffectEndServerTime <= EffectStartServerTime
        || EffectEndServerTime <= GameState->GetServerWorldTimeSeconds())
    {
        return;
    }

    FootstepWarningStartServerTime = EffectStartServerTime;
    FootstepWarningEndServerTime = EffectEndServerTime;
    RefreshPresentation();
}

void UDMStatusHUDWidget::StopSkillStatusRefreshTimer()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(SkillStatusRefreshTimerHandle);
    }

    SkillStatusRefreshTimerHandle.Invalidate();
}

void UDMStatusHUDWidget::RefreshPresentation()
{
    const auto SetTextVisibility = [](UTextBlock* TextWidget, const bool bActive)
    {
        if (IsValid(TextWidget))
        {
            TextWidget->SetVisibility(bActive ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
        }
    };

    const auto CollapseLegacySkillWidgets = [this]()
    {
        const TArray<UWidget*> LegacyWidgets = {
            TXT_VisionDistorted,
            TXT_FootstepObserved,
            TXT_SlowMovement,
            TXT_SlowMovementRemaining,
            PB_SlowMovementRemaining,
            TXT_SizeAmplified,
            TXT_SizeAmplifiedRemaining,
            PB_SizeAmplifiedRemaining
        };

        for (UWidget* Widget : LegacyWidgets)
        {
            if (IsValid(Widget))
            {
                Widget->SetVisibility(ESlateVisibility::Collapsed);
            }
        }
    };

    CollapseLegacySkillWidgets();

    if (!bPresentationActive)
    {
        StopSkillStatusRefreshTimer();
        SetTextVisibility(TXT_Invisible, false);
        if (IsValid(TXT_InvisibleRemaining))
        {
            TXT_InvisibleRemaining->SetVisibility(ESlateVisibility::Collapsed);
        }
        if (IsValid(PB_InvisibleRemaining))
        {
            PB_InvisibleRemaining->SetVisibility(ESlateVisibility::Collapsed);
        }
        if (IsValid(SkillStatusPanel))
        {
            SkillStatusPanel->SetVisibility(ESlateVisibility::Collapsed);
        }
        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    const UDMCharacterStateComponent* State = BoundStateComponent.Get();
    const ADMMatchPlayerController* Controller = BoundController.Get();
    UWorld* World = GetWorld();
    const AGameStateBase* GameState = IsValid(World) ? World->GetGameState<AGameStateBase>() : nullptr;
    const FNumberFormattingOptions OneDecimal = FNumberFormattingOptions()
        .SetMaximumFractionalDigits(1).SetMinimumFractionalDigits(1);

    const bool bStunned = IsValid(State) && State->HasStatus(DMGameplayTags::Status_Stunned);
    const bool bInvisible = IsValid(State) && State->HasStatus(DMGameplayTags::Status_Invisible);
    const bool bSpawnProtected = IsValid(State) && State->HasStatus(DMGameplayTags::Status_SpawnProtected);

    SetTextVisibility(TXT_Stunned, bStunned);
    SetTextVisibility(TXT_Invisible, bInvisible);
    SetTextVisibility(TXT_SpawnProtected, bSpawnProtected);

    FDMSkillStatusCandidate Selected;
    const double ServerNow = IsValid(GameState) ? GameState->GetServerWorldTimeSeconds() : 0.0;

    if (IsValid(State) && IsValid(GameState))
    {
        const FDMSizeAmplificationTiming SizeTiming = State->GetSizeAmplificationTiming();
        if (State->HasStatus(DMGameplayTags::Status_SizeAmplified))
        {
            ConsiderSkillStatusCandidate(
                Selected,
		NSLOCTEXT("DMStatusHUD", "SizeAmplificationActive", "누군가 당신의 크기를 부풀렸습니다 !"),
                SizeTiming.StartServerTime,
                SizeTiming.EndServerTime,
                2,
                ServerNow
            );
        }

        const FDMSlowMovementTiming SlowTiming = State->GetSlowMovementTiming();
        if (State->HasStatus(DMGameplayTags::Status_SlowMovement))
        {
            ConsiderSkillStatusCandidate(
                Selected,
		NSLOCTEXT("DMStatusHUD", "SlowMovementActive", "누군가 당신의 발바닥을 녹였습니다 ! 이동속도가 감소합니다"),
                SlowTiming.StartServerTime,
                SlowTiming.EndServerTime,
                3,
                ServerNow
            );
        }
    }

    if (IsValid(Controller) && Controller->IsVisionDistortPresentationActive() && IsValid(GameState))
    {
        ConsiderSkillStatusCandidate(
            Selected,
		NSLOCTEXT("DMStatusHUD", "VisionDistortActive", "누군가 당신의 시야를 방해합니다 !"),
            Controller->GetVisionDistortStartServerTime(),
            Controller->GetVisionDistortEndServerTime(),
            1,
            ServerNow
        );
    }

    if (IsValid(GameState))
    {
        ConsiderSkillStatusCandidate(
            Selected,
		NSLOCTEXT("DMStatusHUD", "FootstepAmplifyActive", "누군가 당신의 발소리를 엿듣습니다 ! 발소리가 증폭됩니다"),
            FootstepWarningStartServerTime,
            FootstepWarningEndServerTime,
            0,
            ServerNow
        );
    }

    const FDMInvisibleTiming InvisibleTiming = IsValid(State) ? State->GetInvisibleTiming() : FDMInvisibleTiming();
    const bool bHasInvisibleTiming = bInvisible && IsValid(GameState)
        && InvisibleTiming.EndServerTime > InvisibleTiming.StartServerTime
        && InvisibleTiming.EndServerTime > ServerNow;
    const double InvisibleDuration = bHasInvisibleTiming
        ? InvisibleTiming.EndServerTime - InvisibleTiming.StartServerTime
        : 0.0;
    const double InvisibleRemaining = bHasInvisibleTiming
        ? FMath::Max(0.0, InvisibleTiming.EndServerTime - ServerNow)
        : 0.0;

    if (IsValid(TXT_InvisibleRemaining))
    {
        TXT_InvisibleRemaining->SetVisibility(
            bHasInvisibleTiming ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed
        );
        if (bHasInvisibleTiming)
        {
            TXT_InvisibleRemaining->SetText(FText::Format(
                NSLOCTEXT("DMStatusHUD", "SecondsFormat", "{0}초"),
                FText::AsNumber(InvisibleRemaining, &OneDecimal)
            ));
        }
    }

    if (IsValid(PB_InvisibleRemaining))
    {
        PB_InvisibleRemaining->SetVisibility(
            bHasInvisibleTiming ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed
        );
        PB_InvisibleRemaining->SetPercent(
            InvisibleDuration > 0.0
                ? FMath::Clamp(static_cast<float>(InvisibleRemaining / InvisibleDuration), 0.0f, 1.0f)
                : 0.0f
        );
    }

    const bool bHasSelectedSkill = Selected.IsActive(ServerNow);
    const bool bHasCommonSkillWidgets = IsValid(SkillStatusPanel) && IsValid(TXT_Skill)
        && IsValid(TXT_SkillRemaning) && IsValid(PB_Skill);

    if (bHasSelectedSkill && !bHasCommonSkillWidgets && !bMissingCommonSkillWidgetsReported)
    {
        bMissingCommonSkillWidgetsReported = true;
        UE_LOG(
            LogDMStatusHUD,
            Warning,
            TEXT("Common skill status UI is incomplete. Required optional bindings: SkillStatusPanel, TXT_Skill, TXT_SkillRemaning, PB_Skill.")
        );
    }

    if (IsValid(SkillStatusPanel))
    {
        SkillStatusPanel->SetVisibility(
            bHasSelectedSkill && bHasCommonSkillWidgets ? ESlateVisibility::Visible : ESlateVisibility::Collapsed
        );
    }

    if (bHasSelectedSkill && bHasCommonSkillWidgets)
    {
        const double Duration = Selected.EndServerTime - Selected.StartServerTime;
        const double Remaining = FMath::Max(0.0, Selected.EndServerTime - ServerNow);

        TXT_Skill->SetText(Selected.Message);
        TXT_SkillRemaning->SetText(FText::Format(
            NSLOCTEXT("DMStatusHUD", "SecondsFormat", "{0}초"),
            FText::AsNumber(Remaining, &OneDecimal)
        ));
        PB_Skill->SetPercent(
            Duration > 0.0 ? FMath::Clamp(static_cast<float>(Remaining / Duration), 0.0f, 1.0f) : 0.0f
        );
    }

    const bool bNeedsStatusRefresh = bHasSelectedSkill || bHasInvisibleTiming;
    if (bNeedsStatusRefresh && IsValid(World))
    {
        if (!World->GetTimerManager().IsTimerActive(SkillStatusRefreshTimerHandle))
        {
            World->GetTimerManager().SetTimer(
                SkillStatusRefreshTimerHandle,
                this,
                &ThisClass::RefreshPresentation,
                SkillStatusRefreshInterval,
                true
            );
        }
    }
    else
    {
        StopSkillStatusRefreshTimer();
    }

    const bool bAnyStatusActive = bStunned || bInvisible || bSpawnProtected || bHasSelectedSkill;
    SetVisibility(bAnyStatusActive ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

#pragma endregion

#pragma region Lifecycle

void UDMStatusHUDWidget::NativeDestruct()
{
    SetPresentationActive(false);
    SetDataSources(nullptr, nullptr);

    StopSkillStatusRefreshTimer();
    FootstepWarningStartServerTime = 0.0;
    FootstepWarningEndServerTime = 0.0;

    Super::NativeDestruct();
}

#pragma endregion
