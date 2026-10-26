
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"

#include "Interaction/DMInteractionTypes.h"

#include "DMChestPanelWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UBorder;
class UDMInteractionComponent;

/**
 * 서버가 승인한 상자 정보와 등록·훔치기 진행률을 표시한다.
 *
 * 상자 등록 개수는 Controller Snapshot을 사용하며,
 * 진행률은 현재 InteractionComponent에서 직접 조회한다.
 */
UCLASS(Abstract, Blueprintable)
class DONTMELT_API UDMChestPanelWidget : public UUserWidget
{
    GENERATED_BODY()

public:
#pragma region Data Source

    void SetInteractionComponent(UDMInteractionComponent* InComponent);

    void SetChestSnapshot(bool bIsOwnChest, int32 RegisteredCount, int32 RequiredCount, bool bIsCarryingRelic);

    void SetChestOpen(bool bOpen);

#pragma endregion

protected:
    virtual void NativeDestruct() override;

#pragma region Widget References

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ChestTitle = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_RelicCount = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ChestAction = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UBorder> BRD_ProgressPanel = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ProgressState = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UProgressBar> PB_ChestProgress = nullptr;

#pragma endregion

private:
#pragma region Interaction Progress

    void HandleInteractionSessionChanged(const FDMInteractionSession& Session);

    void RefreshInteractionProgress();

    void StopProgressTimer();

    TWeakObjectPtr<UDMInteractionComponent> BoundInteractionComponent;

    FTimerHandle ProgressTimerHandle;

    // 실제 상자 상태의 원본이 아닌 로컬 표시 활성화 여부다.
    bool bModalOpen = false;

#pragma endregion
};
