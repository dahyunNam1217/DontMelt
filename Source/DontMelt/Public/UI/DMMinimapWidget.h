#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"

#include "DMMinimapWidget.generated.h"

class ADMMatchPlayerState;
class ADMPlayerCharacter;
class UCanvasPanel;
class UWidget;

/** 고정 지도 이미지 위에 로컬 플레이어와 자신의 진영 위치만 표시한다. */
UCLASS(Abstract, Blueprintable)
class DONTMELT_API UDMMinimapWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetDataSources(ADMMatchPlayerState* InPlayerState, ADMPlayerCharacter* InPawn);
    void SetPresentationActive(bool bActive);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DM|Minimap|Mapping")
    FVector2D WorldBoundsMin = FVector2D::ZeroVector;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DM|Minimap|Mapping")
    FVector2D WorldBoundsMax = FVector2D::ZeroVector;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DM|Minimap|Mapping")
    float MapRotationDegrees = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DM|Minimap|Mapping")
    bool bInvertMapY = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DM|Minimap|Mapping")
    bool bClampIconsToMap = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DM|Minimap|Presentation", meta = (ClampMin = "0.05"))
    float LocationRefreshInterval = 0.1f;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCanvasPanel> MinimapCanvas = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWidget> PlayerLocationIcon = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> OwnBaseLocationIcon = nullptr;

private:
    bool IsMappingValid() const;
    bool TryWorldToMap(const FVector& WorldLocation, FVector2D& OutMapPosition) const;
    bool SetIconWorldPosition(UWidget* Icon, const FVector& WorldLocation) const;
    void ResolveOwnBaseLocation();
    void RefreshMinimap();
    void StopRefreshTimer();

    TWeakObjectPtr<ADMMatchPlayerState> BoundPlayerState;
    TWeakObjectPtr<ADMPlayerCharacter> BoundPawn;
    FVector OwnBaseWorldLocation = FVector::ZeroVector;
    bool bHasOwnBaseLocation = false;
    bool bPresentationActive = false;
    FTimerHandle RefreshTimerHandle;
};
