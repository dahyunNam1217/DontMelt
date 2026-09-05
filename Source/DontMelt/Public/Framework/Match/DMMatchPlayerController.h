// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DMMatchPlayerController.generated.h"

/**
 * 플레이어 입력, HUD, 관전 카메라 및 개인 응답을 담당할 Controller.
 *
 * 현재 단계에서는 입력과 UI를 구현하지 않는다.
 */

UCLASS()
class DONTMELT_API ADMMatchPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    /**
     * 개발 검증용: Playing Match를 Finished로 전환한다.
     */
    UFUNCTION(Exec)
    void DMTestFinishMatch();

    /**
     * 개발 검증용: 현재 Match를 Aborted로 전환한다.
     */
    UFUNCTION(Exec)
    void DMTestAbortMatch();

    /**
     * 개발 검증용:
     * 정규 시간 만료 후 OvertimeIntro를 시작한다.
     */
    UFUNCTION(Exec)
    void DMTestStartOvertime();

    /**
     * 개발 검증용:
     * Overtime Phase를 정상 종료한다.
     */
    UFUNCTION(Exec)
    void DMTestFinishOvertime();
};