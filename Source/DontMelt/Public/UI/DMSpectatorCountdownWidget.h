#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DMSpectatorCountdownWidget.generated.h"

/**
 * Spectator Fade Phase의
 * 5·4·3·2·1 숫자 Presentation 전용 Widget.
 *
 * 시간 계산이나 Respawn 판정을 소유하지 않는다.
 */
UCLASS(Abstract, Blueprintable)
class DONTMELT_API UDMSpectatorCountdownWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /**
     * MatchPlayerController가 계산한
     * 현재 Countdown 값을 Presentation에 전달한다.
     *
     * 0이면 숫자를 숨기는 의미로 사용한다.
     */
    void SetCountdownValue(int32 InCountdownValue);

protected:
#pragma region Blueprint Presentation

    UFUNCTION(BlueprintImplementableEvent, Category = "DM|UI|Spectator")
    void BP_SetCountdownValue(int32 CountdownValue);

#pragma endregion
};