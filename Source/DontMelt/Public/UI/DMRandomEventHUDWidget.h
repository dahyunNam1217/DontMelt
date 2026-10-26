
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "Events/DMRandomEventTypes.h"

#include "DMRandomEventHUDWidget.generated.h"

class UTextBlock;
class UProgressBar;
class ADMMatchGameState;
class UDMRandomEventSetDataAsset;

/**
 * 랜덤 이벤트의 이름·설명·진행률을 표시한다.
 *
 * 공개 이벤트 상태는 GameState에서 읽는다.
 * 이벤트의 이름과 설명은 정적 Data Asset에서 조회한다.
 *
 * 서버 이벤트 판정이나 실제 상태이상 적용은 담당하지 않는다.
 */
UCLASS(Abstract, Blueprintable)
class DONTMELT_API UDMRandomEventHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
#pragma region Data Source

    /** 현재 매치의 공개 이벤트 상태에 연결한다. */
    void SetGameState(ADMMatchGameState* InGameState);

    /** Root에서 HUD 표시 가능 여부를 전달한다. */
    void SetPresentationActive(bool bActive);

#pragma endregion

protected:
#pragma region Lifecycle

    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

#pragma endregion

#pragma region Configuration

    /** 이벤트 이름과 설명을 조회할 정적 Data Asset */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DM|UI|Random Event")
    TObjectPtr<UDMRandomEventSetDataAsset> RandomEventSetData = nullptr;

#pragma endregion

#pragma region Widget References

    /** 현재 랜덤 이벤트 이름 */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_EventName = nullptr;

    /** 현재 이벤트의 효과 및 예고 안내 */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_EventDescription = nullptr;

    /** 이벤트 예고 또는 진행 구간의 진행률 */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UProgressBar> PB_EventProgress = nullptr;

#pragma endregion

private:
#pragma region Event Presentation

    void HandleRandomEventPublicStateChanged(ADMMatchGameState* Source, FDMRandomEventRuntimeState NewState);

    void RefreshPresentation();

    const FDMRandomEventDefinition* FindEventDefinition(const FGameplayTag& EventTag) const;

#pragma endregion

#pragma region Presentation Timer

    void StopRefreshTimer();

    TWeakObjectPtr<ADMMatchGameState> BoundGameState;

    FTimerHandle RefreshTimerHandle;

    /** 로컬 HUD 표시 활성화 여부 */
    bool bPresentationActive = false;

#pragma endregion
};
