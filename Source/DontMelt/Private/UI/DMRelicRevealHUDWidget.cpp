#include "UI/DMRelicRevealHUDWidget.h"

#include "Framework/Match/DMMatchGameState.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"

namespace
{
    constexpr float RelicRevealRefreshInterval = 0.05f;
    constexpr float RelicMarkerSize = 18.0f;
}

#pragma region Lifecycle
void UDMRelicRevealHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::Collapsed);
}

void UDMRelicRevealHUDWidget::NativeDestruct()
{
    SetPresentationActive(false);
    SetGameState(nullptr);
    StopRefreshTimer();
    Super::NativeDestruct();
}
#pragma endregion

#pragma region Data Source
void UDMRelicRevealHUDWidget::SetGameState(ADMMatchGameState* InGameState)
{
    if (BoundGameState.Get() == InGameState)
    {
        RefreshPresentation();
        return;
    }

    if (ADMMatchGameState* Old = BoundGameState.Get())
    {
        Old->OnRelicRevealChanged().RemoveAll(this);
    }

    HideReveal();
    BoundGameState = InGameState;

    if (IsValid(InGameState))
    {
        InGameState->OnRelicRevealChanged().AddUObject(this, &ThisClass::HandleRelicRevealChanged);
    }
    RefreshPresentation();
}

void UDMRelicRevealHUDWidget::SetPresentationActive(bool bActive)
{
    if (bPresentationActive == bActive)
    {
        return;
    }

    bPresentationActive = bActive;
    if (bPresentationActive)
    {
        RefreshPresentation();
    }
    else
    {
        HideReveal();
    }
}
#pragma endregion

#pragma region Presentation
bool UDMRelicRevealHUDWidget::WorldToMapPosition(const FVector& WorldLocation, FVector2D& OutMapPosition) const
{
    const FVector2D Extent = MapWorldMax - MapWorldMin;
    if (Extent.X <= KINDA_SMALL_NUMBER || Extent.Y <= KINDA_SMALL_NUMBER || MapPixelSize.X <= 0.0
        || MapPixelSize.Y <= 0.0)
    {
        return false;
    }

    const double U = (WorldLocation.X - MapWorldMin.X) / Extent.X;
    const double V = 1.0 - (WorldLocation.Y - MapWorldMin.Y) / Extent.Y;
    if (U < 0.0 || U > 1.0 || V < 0.0 || V > 1.0)
    {
        return false;
    }

    OutMapPosition = FVector2D(U * MapPixelSize.X, V * MapPixelSize.Y);
    return true;
}

bool UDMRelicRevealHUDWidget::RebuildMarkers(const TArray<FDMRelicRevealEntry>& Entries)
{
    if (!IsValid(MarkerCanvas) || !IsValid(MarkerIconTexture) || !IsValid(WidgetTree))
    {
        return false;
    }

    MarkerCanvas->ClearChildren();

    TArray<FVector2D> OriginalPositions;
    OriginalPositions.Reserve(Entries.Num());

    for (const FDMRelicRevealEntry& Entry : Entries)
    {
        FVector2D Position;
        if (!WorldToMapPosition(Entry.WorldLocation, Position))
        {
            MarkerCanvas->ClearChildren();
            return false;
        }

        // 같은 상자에 여러 유물이 있으면 마커만 주변으로 살짝 벌린다.
        int32 OverlapIndex = 0;
        for (const FVector2D& Previous : OriginalPositions)
        {
            if (FVector2D::Distance(Position, Previous) < 1.0)
            {
                ++OverlapIndex;
            }
        }
        OriginalPositions.Add(Position);

        if (OverlapIndex > 0)
        {
            const double Angle = OverlapIndex * (PI / 2.0);
            Position += FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * 12.0;
        }

        UImage* Marker = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
        if (!IsValid(Marker))
        {
            MarkerCanvas->ClearChildren();
            return false;
        }

        Marker->SetBrushFromTexture(MarkerIconTexture, true);
        Marker->SetVisibility(ESlateVisibility::HitTestInvisible);

        UCanvasPanelSlot* MarkerSlot = MarkerCanvas->AddChildToCanvas(Marker);
        if (!IsValid(MarkerSlot))
        {
            MarkerCanvas->ClearChildren();
            return false;
        }

        MarkerSlot->SetAutoSize(false);
        MarkerSlot->SetSize(FVector2D(RelicMarkerSize, RelicMarkerSize));
        MarkerSlot->SetAlignment(FVector2D(0.5, 0.5));
        MarkerSlot->SetPosition(Position);
    }

    return MarkerCanvas->GetChildrenCount() == Entries.Num();
}

void UDMRelicRevealHUDWidget::HandleRelicRevealChanged(
    ADMMatchGameState* Source, FDMRelicRevealState /*NewState*/
)
{
    if (Source == BoundGameState.Get())
    {
        RefreshPresentation();
    }
}

void UDMRelicRevealHUDWidget::RefreshPresentation()
{
    const ADMMatchGameState* GameState = BoundGameState.Get();
    if (!bPresentationActive || !IsValid(GameState) || GameState->GetCurrentPhase() != EDMMatchPhase::Playing)
    {
        HideReveal();
        return;
    }

    const FDMRelicRevealState State = GameState->GetRelicRevealState();
    if (!State.HasPayload() || GameState->GetServerWorldTimeSeconds() >= State.EndServerTime)
    {
        HideReveal();
        return;
    }

    if (DisplayedEndServerTime != State.EndServerTime)
    {
        if (!RebuildMarkers(State.Entries))
        {
            HideReveal();
            return;
        }
        DisplayedEndServerTime = State.EndServerTime;
    }

    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    RefreshRemainingTime();

    if (UWorld* World = GetWorld())
    {
        if (!World->GetTimerManager().IsTimerActive(RefreshTimerHandle))
        {
            World->GetTimerManager().SetTimer(
                RefreshTimerHandle, this, &ThisClass::RefreshRemainingTime, RelicRevealRefreshInterval, true
            );
        }
    }
}

void UDMRelicRevealHUDWidget::RefreshRemainingTime()
{
    const ADMMatchGameState* GameState = BoundGameState.Get();
    if (!bPresentationActive || !IsValid(GameState) || GameState->GetCurrentPhase() != EDMMatchPhase::Playing)
    {
        HideReveal();
        return;
    }

    const FDMRelicRevealState State = GameState->GetRelicRevealState();
    const double Remaining = State.EndServerTime - GameState->GetServerWorldTimeSeconds();
    if (!State.HasPayload() || Remaining <= 0.0)
    {
        HideReveal();
        return;
    }

    if (IsValid(TXT_RevealTime))
    {
        TXT_RevealTime->SetText(FText::FromString(FString::Printf(TEXT("%.1f초"), Remaining)));
    }
}

void UDMRelicRevealHUDWidget::HideReveal()
{
    StopRefreshTimer();
    DisplayedEndServerTime = 0.0;
    if (IsValid(MarkerCanvas))
    {
        MarkerCanvas->ClearChildren();
    }
    SetVisibility(ESlateVisibility::Collapsed);
}

void UDMRelicRevealHUDWidget::StopRefreshTimer()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(RefreshTimerHandle);
    }
    RefreshTimerHandle.Invalidate();
}
#pragma endregion
