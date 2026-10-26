#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Interaction/DMInteractionTypes.h"
#include "DMRelicHUDWidget.generated.h"

class ADMMatchPlayerController;
class ADMMatchPlayerState;
class ADMPlayerCharacter;
class UDMInteractionComponent;

/**
 * Relic 시스템용 최소 HUD.
 *
 * Gameplay 상태를 소유하지 않는다.
 *
 * PlayerState / InteractionComponent /
 * PlayerController의 Read Model을 구독하고
 * Blueprint 표현 이벤트만 호출한다.
 */
UCLASS(Abstract, Blueprintable)
class DONTMELT_API UDMRelicHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /**
     * Local Player의 현재 Gameplay Source와
     * HUD를 연결한다.
     *
     * Pawn 교체 시 다시 호출 가능하다.
     */
    void InitializeRelicHUD(ADMMatchPlayerController* InController, ADMPlayerCharacter* InCharacter);

    /**
     * Delegate / Timer를 정리한다.
     */
    void ShutdownRelicHUD();

protected:
    virtual void NativeDestruct() override;

#pragma region Blueprint Presentation

    /**
     * 본인이 현재 Relic을 운반 중인지 표시한다.
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "DM|UI|Relic")
    void BP_SetCarryState(bool bIsCarryingRelic);

    /**
     * Pickup / Steal / Deposit 진행도를 표시한다.
     *
     * bVisible == false이면
     * Progress UI를 숨긴다.
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "DM|UI|Relic")
    void BP_SetInteractionProgress(bool bVisible, EDMInteractionType InteractionType, float Progress01);

#pragma endregion

private:
#pragma region Event Handlers

    void HandleCarryStateChanged(ADMMatchPlayerState* PlayerState, bool bIsCarryingRelic);

    void HandleInteractionSessionChanged(const FDMInteractionSession& Session);

    void HandleChestInteractionChanged(bool bOpen, int32 BaseSlot, int32 RegisteredRelicCount);

#pragma endregion

#pragma region Progress

    void StartInteractionProgressUpdates();

    void StopInteractionProgressUpdates();

    void UpdateInteractionProgress();

#pragma endregion

#pragma region Sources

    TWeakObjectPtr<ADMMatchPlayerController> OwningMatchController;

    TWeakObjectPtr<ADMMatchPlayerState> BoundPlayerState;

    TWeakObjectPtr<UDMInteractionComponent> BoundInteractionComponent;

#pragma endregion

#pragma region Runtime

    FTimerHandle InteractionProgressTimerHandle;

    EDMInteractionType ActiveInteractionType = EDMInteractionType::PickupRelic;

#pragma endregion
};