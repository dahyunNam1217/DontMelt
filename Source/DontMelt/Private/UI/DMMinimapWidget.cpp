#include "UI/DMMinimapWidget.h"

#include "Characters/DMPlayerCharacter.h"
#include "Framework/Match/DMMatchPlayerState.h"
#include "Relic/DMBaseZone.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Widget.h"
#include "Engine/World.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogDMMinimapWidget, Log, All);

void UDMMinimapWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::Collapsed);
}

void UDMMinimapWidget::SetDataSources(ADMMatchPlayerState* InPlayerState, ADMPlayerCharacter* InPawn)
{
    BoundPlayerState = InPlayerState;
    BoundPawn = InPawn;
    ResolveOwnBaseLocation();
    RefreshMinimap();
}

void UDMMinimapWidget::SetPresentationActive(const bool bActive)
{
    bPresentationActive = bActive;
    if (!bPresentationActive)
    {
        StopRefreshTimer();
        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }
    RefreshMinimap();
}

bool UDMMinimapWidget::IsMappingValid() const
{
    const FVector2D Extent = WorldBoundsMax - WorldBoundsMin;
    return Extent.X > UE_KINDA_SMALL_NUMBER && Extent.Y > UE_KINDA_SMALL_NUMBER;
}

bool UDMMinimapWidget::TryWorldToMap(const FVector& WorldLocation, FVector2D& OutMapPosition) const
{
    if (!IsMappingValid() || !IsValid(MinimapCanvas))
    {
        return false;
    }

    const FVector2D CanvasSize = MinimapCanvas->GetCachedGeometry().GetLocalSize();
    if (CanvasSize.X <= UE_KINDA_SMALL_NUMBER || CanvasSize.Y <= UE_KINDA_SMALL_NUMBER)
    {
        return false;
    }

    const float Radians = FMath::DegreesToRadians(MapRotationDegrees);
    const float CosAngle = FMath::Cos(Radians);
    const float SinAngle = FMath::Sin(Radians);
    const FVector2D RotatedLocation(
        WorldLocation.X * CosAngle - WorldLocation.Y * SinAngle,
        WorldLocation.X * SinAngle + WorldLocation.Y * CosAngle
    );

    const FVector2D Extent = WorldBoundsMax - WorldBoundsMin;
    FVector2D Normalized = (RotatedLocation - WorldBoundsMin) / Extent;
    if (!FMath::IsFinite(Normalized.X) || !FMath::IsFinite(Normalized.Y))
    {
        return false;
    }

    if (bInvertMapY)
    {
        Normalized.Y = 1.0f - Normalized.Y;
    }
    if (bClampIconsToMap)
    {
        Normalized.X = FMath::Clamp(Normalized.X, 0.0f, 1.0f);
        Normalized.Y = FMath::Clamp(Normalized.Y, 0.0f, 1.0f);
    }

    OutMapPosition = Normalized * CanvasSize;
    return true;
}

bool UDMMinimapWidget::SetIconWorldPosition(UWidget* Icon, const FVector& WorldLocation) const
{
    if (!IsValid(Icon))
    {
        return false;
    }

    FVector2D MapPosition;
    UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Icon->Slot);
    if (!TryWorldToMap(WorldLocation, MapPosition) || !IsValid(CanvasSlot))
    {
        Icon->SetVisibility(ESlateVisibility::Collapsed);
        return false;
    }

    CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f));
    CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
    CanvasSlot->SetPosition(MapPosition);
    Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
    return true;
}

void UDMMinimapWidget::ResolveOwnBaseLocation()
{
    bHasOwnBaseLocation = false;
    const ADMMatchPlayerState* PlayerState = BoundPlayerState.Get();
    UWorld* World = GetWorld();
    if (!IsValid(PlayerState) || !PlayerState->HasAssignedBaseSlot() || !IsValid(World))
    {
        return;
    }

    const int32 OwnBaseSlot = PlayerState->GetBaseSlot();
    for (TActorIterator<ADMBaseZone> It(World); It; ++It)
    {
        const ADMBaseZone* BaseZone = *It;
        if (IsValid(BaseZone) && BaseZone->GetBaseSlot() == OwnBaseSlot && BaseZone->IsActiveBase())
        {
            OwnBaseWorldLocation = BaseZone->GetActorLocation();
            bHasOwnBaseLocation = true;
            return;
        }
    }

    UE_LOG(LogDMMinimapWidget, Warning, TEXT("Own BaseZone is unavailable. BaseSlot=%d"), OwnBaseSlot);
}

void UDMMinimapWidget::RefreshMinimap()
{
    ADMPlayerCharacter* Pawn = BoundPawn.Get();
    if (!bPresentationActive || !IsValid(Pawn) || !IsMappingValid())
    {
        StopRefreshTimer();
        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    const bool bPlayerVisible = SetIconWorldPosition(PlayerLocationIcon, Pawn->GetActorLocation());
    if (IsValid(OwnBaseLocationIcon)
        && (!bHasOwnBaseLocation || !SetIconWorldPosition(OwnBaseLocationIcon, OwnBaseWorldLocation)))
    {
        OwnBaseLocationIcon->SetVisibility(ESlateVisibility::Collapsed);
    }

    SetVisibility(bPlayerVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

    UWorld* World = GetWorld();
    if (bPlayerVisible && IsValid(World) && !World->GetTimerManager().IsTimerActive(RefreshTimerHandle))
    {
        World->GetTimerManager().SetTimer(
            RefreshTimerHandle, this, &ThisClass::RefreshMinimap, FMath::Max(0.05f, LocationRefreshInterval), true
        );
    }
}

void UDMMinimapWidget::StopRefreshTimer()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(RefreshTimerHandle);
    }
    RefreshTimerHandle.Invalidate();
}

void UDMMinimapWidget::NativeDestruct()
{
    bPresentationActive = false;
    StopRefreshTimer();
    BoundPlayerState.Reset();
    BoundPawn.Reset();
    bHasOwnBaseLocation = false;
    Super::NativeDestruct();
}
