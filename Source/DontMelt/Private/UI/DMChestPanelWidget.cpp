
#include "UI/DMChestPanelWidget.h"

#include "Characters/Components/DMInteractionComponent.h"

#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Border.h"

#include "Engine/World.h"

namespace
{
    // 등록·훔치기 진행 중에만 표시를 갱신한다.
    constexpr float ChestProgressRefreshInterval = 0.05f;
}

#pragma region Data Source

void UDMChestPanelWidget::SetInteractionComponent(UDMInteractionComponent* InComponent)
{
    if (BoundInteractionComponent.Get() == InComponent)
    {
        RefreshInteractionProgress();
        return;
    }

    StopProgressTimer();

    // 이전 Pawn의 상호작용 Delegate를 해제한다.
    if (UDMInteractionComponent* OldComponent = BoundInteractionComponent.Get())
    {
        OldComponent->OnInteractionSessionChanged().RemoveAll(this);
    }

    BoundInteractionComponent = InComponent;

    if (IsValid(InComponent))
    {
        InComponent->OnInteractionSessionChanged().AddUObject(this, &ThisClass::HandleInteractionSessionChanged);
    }

    RefreshInteractionProgress();
}

void UDMChestPanelWidget::SetChestSnapshot(
    const bool bIsOwnChest, const int32 RegisteredCount, const int32 RequiredCount, const bool bIsCarryingRelic
)
{
    if (IsValid(TXT_ChestTitle))
    {
        TXT_ChestTitle->SetText(
            bIsOwnChest ? NSLOCTEXT("DMChestPanel", "OwnChest", "내 유물 상자")
                        : NSLOCTEXT("DMChestPanel", "OtherChest", "다른 유물 상자")
        );
    }

    if (IsValid(TXT_RelicCount))
    {
        const FString CountText =
            FString::Printf(TEXT("%d / %d"), FMath::Max(0, RegisteredCount), FMath::Max(0, RequiredCount));

        TXT_RelicCount->SetText(FText::FromString(CountText));
    }

    if (!IsValid(TXT_ChestAction))
    {
        return;
    }

    FText ActionText;

    if (bIsOwnChest)
    {
        ActionText = bIsCarryingRelic ? NSLOCTEXT("DMChestPanel", "DepositHint", "Space 길게 · 유물 등록")
                                      : NSLOCTEXT("DMChestPanel", "NoCarriedRelic", "운반 중인 유물이 없습니다");
    }
    else
    {
        ActionText = RegisteredCount > 0 ? NSLOCTEXT("DMChestPanel", "StealHint", "Space 길게 · 유물 훔치기")
                                         : NSLOCTEXT("DMChestPanel", "NoRegisteredRelic", "훔칠 유물이 없습니다");
    }

    TXT_ChestAction->SetText(ActionText);
}

void UDMChestPanelWidget::SetChestOpen(const bool bOpen)
{
    bModalOpen = bOpen;

    SetVisibility(bModalOpen ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);

    if (!bModalOpen)
    {
        StopProgressTimer();

        if (IsValid(BRD_ProgressPanel))
        {
            BRD_ProgressPanel->SetVisibility(ESlateVisibility::Collapsed);
        }

        return;
    }

    RefreshInteractionProgress();
}

#pragma endregion

#pragma region Interaction Progress

void UDMChestPanelWidget::HandleInteractionSessionChanged(const FDMInteractionSession& /*Session*/
)
{
    RefreshInteractionProgress();
}

void UDMChestPanelWidget::RefreshInteractionProgress()
{
    if (!bModalOpen)
    {
        StopProgressTimer();
        return;
    }

    UDMInteractionComponent* Interaction = BoundInteractionComponent.Get();

    bool bShowProgress = false;
    EDMInteractionType InteractionType = EDMInteractionType::InspectChest;

    if (IsValid(Interaction))
    {
        const FDMInteractionSession& Session = Interaction->GetOwnerSession();

        InteractionType = Session.InteractionType;

        bShowProgress = Session.IsActive()
            && (InteractionType == EDMInteractionType::DepositRelic || InteractionType == EDMInteractionType::StealRelic
            );
    }

    if (IsValid(BRD_ProgressPanel))
    {
        BRD_ProgressPanel->SetVisibility(
            bShowProgress ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed
        );
    }

    if (!bShowProgress)
    {
        StopProgressTimer();
        return;
    }

    if (IsValid(TXT_ProgressState))
    {
        TXT_ProgressState->SetText(
            InteractionType == EDMInteractionType::DepositRelic
                ? NSLOCTEXT("DMChestPanel", "Depositing", "유물 등록 중")
                : NSLOCTEXT("DMChestPanel", "Stealing", "유물 훔치는 중")
        );
    }

    if (IsValid(PB_ChestProgress))
    {
        PB_ChestProgress->SetPercent(FMath::Clamp(Interaction->GetInteractionProgress01(), 0.0f, 1.0f));
    }

    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    FTimerManager& TimerManager = World->GetTimerManager();

    if (!TimerManager.IsTimerActive(ProgressTimerHandle))
    {
        TimerManager.SetTimer(
            ProgressTimerHandle, this, &ThisClass::RefreshInteractionProgress, ChestProgressRefreshInterval, true
        );
    }
}

void UDMChestPanelWidget::StopProgressTimer()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ProgressTimerHandle);
    }

    ProgressTimerHandle.Invalidate();
}

#pragma endregion

#pragma region Lifecycle

void UDMChestPanelWidget::NativeDestruct()
{
    SetChestOpen(false);
    SetInteractionComponent(nullptr);

    Super::NativeDestruct();
}

#pragma endregion
