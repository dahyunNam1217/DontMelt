
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"

#include "DMStatusHUDWidget.generated.h"

class UTextBlock;
class UDMCharacterStateComponent;
class ADMMatchPlayerController;
class UProgressBar;

/**
 * 로컬 플레이어에게 적용된 상태이상을 표시한다.
 * 서버가 소유한 상태이상 태그를 조회하며 직접 변경하지 않는다.
 */
UCLASS(Abstract, Blueprintable)
class DONTMELT_API UDMStatusHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
#pragma region Data Source

    void SetDataSources(UDMCharacterStateComponent* InStateComponent, ADMMatchPlayerController* InController);

    void SetPresentationActive(bool bActive);

#pragma endregion

protected:
    virtual void NativeDestruct() override;

#pragma region Widget References

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_Stunned = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_Invisible = nullptr;

    /** 투명화 효과의 남은 시간을 표시한다. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_InvisibleRemaining = nullptr;

    /** 투명화 효과의 남은 시간 비율을 표시한다. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UProgressBar> PB_InvisibleRemaining = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_SpawnProtected = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_VisionDistorted = nullptr;

    /** 다른 플레이어가 자신의 발소리를 증폭해 듣고 있을 때 표시한다. */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_FootstepObserved = nullptr;

#pragma endregion

private:
#pragma region Presentation

    void HandleStatusChanged();

    void HandleVisionPresentationChanged(bool bActive);

    /** 발소리 증폭의 예정 종료 시각을 피해자 HUD에 반영한다. */
    void HandleFootstepAmplifyWarning(double EffectEndServerTime);

    /** 예정된 발소리 경고 표시가 끝났을 때 호출한다. */
    void HandleFootstepWarningExpired();

    /** 로컬 표시 Timer를 정리한다. */
    void StopFootstepWarningTimer();

    /** 투명화 HUD의 로컬 시간 갱신 Timer를 정리한다. */
    void StopInvisibleRefreshTimer();

    void RefreshPresentation();

    TWeakObjectPtr<UDMCharacterStateComponent> BoundStateComponent;

    TWeakObjectPtr<ADMMatchPlayerController> BoundController;

    /** HUD 표시 가능 여부만 저장한다. */
    bool bPresentationActive = false;

    /** 서버가 알려준 효과의 예정 종료 시각. */
    double FootstepWarningEndServerTime = 0.0;

    /** 발소리 경고 메시지를 닫는 로컬 Timer. */
    FTimerHandle FootstepWarningTimerHandle;

    /** 투명화 남은 시간 표시만 갱신하는 로컬 Timer. */
    FTimerHandle InvisibleRefreshTimerHandle;

#pragma endregion
};
