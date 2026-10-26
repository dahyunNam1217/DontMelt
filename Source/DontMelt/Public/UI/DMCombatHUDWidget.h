
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"

#include "DMCombatHUDWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UDMCombatComponent;

/**
 * 공격·방어 쿨다운 표시 전용 Widget.
 *
 * 실제 쿨다운은 CombatComponent가 소유한다.
 * Widget은 남은 시간과 진행 바만 표시한다.
 */
UCLASS(Abstract, Blueprintable)
class DONTMELT_API UDMCombatHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
#pragma region Data Source

    void SetCombatComponent(UDMCombatComponent* InCombatComponent);

    void SetPresentationActive(bool bActive);

#pragma endregion

protected:
    virtual void NativeDestruct() override;

#pragma region Widget References

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_AttackCooldown = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UProgressBar> PB_AttackCooldown = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_DefenseCooldown = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UProgressBar> PB_DefenseCooldown = nullptr;

#pragma endregion

private:
#pragma region Cooldown Presentation

    void HandleCooldownChanged();

    void RefreshCooldowns();

    void StopDisplayTimer();

    TWeakObjectPtr<UDMCombatComponent> BoundCombatComponent;

    FTimerHandle DisplayTimerHandle;

    // 실제 생존 상태가 아닌 HUD 갱신 활성화 여부만 저장한다.
    bool bPresentationActive = false;

#pragma endregion
};
