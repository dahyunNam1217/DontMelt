
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"

#include "DMStatusHUDWidget.generated.h"

class UTextBlock;
class UPanelWidget;
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

    /** 투명화 전용 상태 문구. 공용 SkillStatusPanel과 독립적으로 표시한다. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_Invisible = nullptr;

    /** 투명화 효과의 남은 시간을 표시한다. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_InvisibleRemaining = nullptr;

    /** 투명화 효과의 남은 시간 비율을 표시한다. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UProgressBar> PB_InvisibleRemaining = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_SpawnProtected = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_VisionDistorted = nullptr;

    /** 다른 플레이어가 자신의 발소리를 증폭해 듣고 있을 때 표시한다. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_FootstepObserved = nullptr;

    /** 감속 상태 표시. 기존 WBP와의 전환 호환을 위해 선택 바인딩한다. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_SlowMovement = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_SlowMovementRemaining = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UProgressBar> PB_SlowMovementRemaining = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_SizeAmplified = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_SizeAmplifiedRemaining = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UProgressBar> PB_SizeAmplifiedRemaining = nullptr;

    /** 투명화를 제외한 네 스킬 상태를 한 곳에 표시하는 공용 부모 패널. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UPanelWidget> SkillStatusPanel = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_Skill = nullptr;

    /** 사용자 지정 철자 Remaning을 그대로 사용한다. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_SkillRemaning = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UProgressBar> PB_Skill = nullptr;

#pragma endregion

private:
#pragma region Presentation

    void HandleStatusChanged();

    void HandleVisionPresentationChanged(bool bActive);

    /** 발소리 증폭의 예정 종료 시각을 피해자 HUD에 반영한다. */
    void HandleFootstepAmplifyWarning(double EffectStartServerTime, double EffectEndServerTime);

    void StopSkillStatusRefreshTimer();

    void RefreshPresentation();

    TWeakObjectPtr<UDMCharacterStateComponent> BoundStateComponent;

    TWeakObjectPtr<ADMMatchPlayerController> BoundController;

    /** HUD 표시 가능 여부만 저장한다. */
    bool bPresentationActive = false;

    /** 서버가 알려준 효과의 시작·종료 시각. */
    double FootstepWarningStartServerTime = 0.0;

    double FootstepWarningEndServerTime = 0.0;

    /** 투명화 또는 공용 스킬 상태가 있을 때만 남은 시간을 갱신한다. */
    FTimerHandle SkillStatusRefreshTimerHandle;

    bool bMissingCommonSkillWidgetsReported = false;

#pragma endregion
};
