#pragma once

#include "CoreMinimal.h"
#include "Core/DMMatchTypes.h"
#include "Templates/SubclassOf.h"

class UDMSpectatorCountdownWidget;

/** 관전 중 민감 정보 표시 제한이 바뀌었음을 UI에 알린다. */
DECLARE_MULTICAST_DELEGATE_OneParam(FDMOnSpectatorInformationRestrictionChanged, bool);

/** 로컬 관전 Presentation 단계가 바뀌었음을 UI에 알린다. */
DECLARE_MULTICAST_DELEGATE_OneParam(FDMOnLocalSpectatorPhaseChanged, EDMSpectatorPhase);

/**
 * 관전 연출 설정값.
 *
 * 원본은 BP_DMMatchPlayerController의 EditDefaultsOnly 값이며,
 * 관전 컴포넌트는 사용할 때마다 Controller에서 읽어 온다.
 */
struct FDMSpectatorPresentationSettings
{
    float DeathCamDistance = 350.0f;

    float DeathCamHeight = 120.0f;

    float DeathCamBlendTime = 0.2f;

    float KillerViewBlendTime = 0.25f;

    float FreeCameraMaxSpeed = 900.0f;

    float SpectatorFadeToBlackDuration = 0.25f;

    TSubclassOf<UDMSpectatorCountdownWidget> SpectatorCountdownWidgetClass;
};
