
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "GameplayTagContainer.h"

#include "DMSkillBarWidget.generated.h"

class ADMMatchPlayerState;
class UDMSkillComponent;
class UDMSkillSlotWidget;

/**
 * 세 개의 스킬 슬롯을 관리하는 HUD.
 *
 * PlayerState와 SkillComponent의 기존 알림을 구독한다.
 * 쿨다운 원본은 복사하지 않고 조회한다.
 */
UCLASS(Abstract, Blueprintable)
class DONTMELT_API UDMSkillBarWidget : public UUserWidget
{
    GENERATED_BODY()

public:
#pragma region Data Source

    void SetPlayerState(ADMMatchPlayerState* InPlayerState);

    void SetPresentationActive(bool bActive);

#pragma endregion

protected:
    virtual void NativeDestruct() override;

#pragma region Widget References

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UDMSkillSlotWidget> SkillSlot1 = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UDMSkillSlotWidget> SkillSlot2 = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UDMSkillSlotWidget> SkillSlot3 = nullptr;

#pragma endregion

private:
#pragma region Data Binding

    void HandleSkillReadModelChanged();

    void RefreshDefinitions();

    void RefreshCooldowns();

    TWeakObjectPtr<ADMMatchPlayerState> BoundPlayerState;

    TWeakObjectPtr<UDMSkillComponent> BoundSkillComponent;

#pragma endregion

#pragma region Presentation Timer

    void StopDisplayTimer();

    FTimerHandle DisplayTimerHandle;

    // 화면 갱신 여부만 관리하며 생존 상태는 저장하지 않는다.
    bool bPresentationActive = false;

#pragma endregion
};
