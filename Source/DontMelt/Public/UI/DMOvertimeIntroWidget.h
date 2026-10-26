
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "Core/DMMatchTypes.h"

#include "DMOvertimeIntroWidget.generated.h"

class UTextBlock;
class UVerticalBox;
class ADMMatchGameState;

/**
 * OvertimeIntro 진입 중 진영별 등록 유물 수를 표시한다.
 *
 * 실제 등록 개수는 서버 Snapshot을 사용하며,
 * 종료 카운트다운은 GameState의 Phase 시간에서 계산한다.
 */
UCLASS(Abstract, Blueprintable)
class DONTMELT_API UDMOvertimeIntroWidget : public UUserWidget
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

#pragma region Widget References

    /** 진영별 등록 개수 TextBlock을 추가할 컨테이너 */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UVerticalBox> VB_BaseRows = nullptr;

    /** 연장전 시작까지 남은 시간 */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_IntroTime = nullptr;

#pragma endregion

private:
#pragma region Data Binding

    void HandlePhaseStateChanged(ADMMatchGameState* Source, FDMMatchPhaseState NewPhase);

    void HandleOvertimeIntroStateChanged(ADMMatchGameState* Source, FDMOvertimeIntroState NewState);

#pragma endregion

#pragma region Presentation

    void RefreshPresentation();

    bool RebuildBaseRows(const TArray<FDMOvertimeBaseCount>& Entries);

    void RefreshRemainingTime();

    void HideIntro();
    void StopRefreshTimer();

    TWeakObjectPtr<ADMMatchGameState> BoundGameState;

    FTimerHandle RefreshTimerHandle;

    // 같은 Snapshot으로 목록을 반복 생성하지 않기 위한 표시 이력.
    double DisplayedIntroEndServerTime = 0.0;

    // Root에서 전달받은 로컬 UI 표시 가능 여부.
    bool bPresentationActive = false;

#pragma endregion
};
