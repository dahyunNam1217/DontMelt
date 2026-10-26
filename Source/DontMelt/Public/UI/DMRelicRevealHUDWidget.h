#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "Core/DMMatchTypes.h"

#include "DMRelicRevealHUDWidget.generated.h"

class UCanvasPanel;
class UTextBlock;
class UTexture2D;
class ADMMatchGameState;

/** 공개가 허용된 3초 동안 항공 지도 위에 유물 마커를 표시한다. */
UCLASS(Abstract, Blueprintable)
class DONTMELT_API UDMRelicRevealHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
#pragma region Data Source
    void SetGameState(ADMMatchGameState* InGameState);
    void SetPresentationActive(bool bActive);
#pragma endregion

protected:
#pragma region Lifecycle
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
#pragma endregion

#pragma region Configuration
    /** 지도 이미지의 좌하단과 우상단에 해당하는 월드 XY. */
    UPROPERTY(EditDefaultsOnly, Category = "DM|UI|Relic Reveal")
    FVector2D MapWorldMin = FVector2D::ZeroVector;

    UPROPERTY(EditDefaultsOnly, Category = "DM|UI|Relic Reveal")
    FVector2D MapWorldMax = FVector2D::ZeroVector;

    /** 마커 Canvas와 지도 이미지의 동일한 설계 해상도. */
    UPROPERTY(EditDefaultsOnly, Category = "DM|UI|Relic Reveal")
    FVector2D MapPixelSize = FVector2D(640.0, 640.0);

    UPROPERTY(EditDefaultsOnly, Category = "DM|UI|Relic Reveal")
    TObjectPtr<UTexture2D> MarkerIconTexture = nullptr;
#pragma endregion

#pragma region Widget References
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCanvasPanel> MarkerCanvas = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_RevealTime = nullptr;
#pragma endregion

private:
#pragma region Presentation
    void HandleRelicRevealChanged(ADMMatchGameState* Source, FDMRelicRevealState NewState);
    void RefreshPresentation();
    void RefreshRemainingTime();
    void HideReveal();
    void StopRefreshTimer();

    bool RebuildMarkers(const TArray<FDMRelicRevealEntry>& Entries);
    bool WorldToMapPosition(const FVector& WorldLocation, FVector2D& OutMapPosition) const;

    TWeakObjectPtr<ADMMatchGameState> BoundGameState;
    FTimerHandle RefreshTimerHandle;

    // 중복 마커 생성을 피하기 위한 UI 표시 이력이다.
    double DisplayedEndServerTime = 0.0;
    bool bPresentationActive = false;
#pragma endregion
};
