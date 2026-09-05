// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DMLobbyPlayerController.generated.h"

/**
 * 로비 UI와 로비 요청의 소유 클라이언트 경계.
 *
 * 게임 데이터는 저장하지 않으며,
 * 로컬 입력 의도를 서버로 전달한다.
 */

UCLASS()
class DONTMELT_API ADMLobbyPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    /**
     * 자신의 Ready 상태 변경을 요청한다.
     *
     * Blueprint Lobby UI와 개발 콘솔에서 동일한 함수를 사용한다.
     *
     * 콘솔 예:
     * RequestSetReady 1
     * RequestSetReady 0
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "DM|Lobby|Requests"
    )
    void RequestSetReady(bool bNewReady);

    /**
     * Match 시작을 요청한다.
     *
     * 실제 방장 여부와 시작 조건은 서버 GameMode가 다시 검증한다.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "DM|Lobby|Requests"
    )
    void RequestStartMatch();

    /** 개발 테스트: 자신의 Ready 상태를 true로 요청한다. */
    UFUNCTION(Exec)
    void DMReady();

    /** 개발 테스트: 자신의 Ready 상태를 false로 요청한다. */
    UFUNCTION(Exec)
    void DMUnready();

    /** 개발 테스트: Match 시작을 요청한다. */
    UFUNCTION(Exec)
    void DMStartMatch();


private:
    /** Ready 변경 요청을 서버로 전달한다. */
    UFUNCTION(Server, Reliable)
    void ServerSetReady(bool bNewReady);

    /** Match 시작 요청을 서버로 전달한다. */
    UFUNCTION(Server, Reliable)
    void ServerRequestStartMatch();


};