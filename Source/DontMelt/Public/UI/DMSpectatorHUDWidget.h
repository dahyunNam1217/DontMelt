
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "Core/DMMatchTypes.h"

#include "DMSpectatorHUDWidget.generated.h"

class UTextBlock;
class ADMMatchPlayerController;

/**
 * 사망 후 로컬 관전 단계와 남은 시간을 표시한다.
 *
 * 서버 Respawn, 카메라 전환, 입력 처리는 담당하지 않는다.
 * 마지막 Fade 단계에서는 기존 CountdownWidget을 사용한다.
 */
UCLASS(Abstract, Blueprintable)
class DONTMELT_API UDMSpectatorHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
#pragma region Data Source

    /** 현재 소유 Controller의 관전 표시 상태에 연결한다. */
    void SetController(ADMMatchPlayerController* InController);

    /** Root가 관전 HUD의 표시 가능 여부를 전달한다. */
    void SetPresentationActive(bool bActive);

#pragma endregion

protected:
#pragma region Lifecycle

    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

#pragma endregion

#pragma region Widget References

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_SpectatorPhase = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_SpectatorHint = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_RespawnTime = nullptr;

#pragma endregion

private:
#pragma region Presentation

    /** Controller의 로컬 관전 단계가 바뀌면 호출한다. */
    void HandleLocalSpectatorPhaseChanged(EDMSpectatorPhase NewPhase);

    /** 현재 관전 단계와 표시 문구를 갱신한다. */
    void RefreshPresentation();

    /** 서버 기준 남은 관전시간 문구를 갱신한다. */
    void RefreshRemainingTime();

    /** HUD를 숨기고 갱신 Timer를 정리한다. */
    void HideSpectatorHUD();

    void StopRefreshTimer();

    TWeakObjectPtr<ADMMatchPlayerController> BoundController;

    FTimerHandle RefreshTimerHandle;

    /** Gameplay 상태가 아닌 로컬 UI 표시 가능 여부. */
    bool bPresentationActive = false;

#pragma endregion
};
