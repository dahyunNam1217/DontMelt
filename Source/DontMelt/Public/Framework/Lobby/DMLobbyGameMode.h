// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DMLobbyGameMode.generated.h"

class ADMLobbyPlayerController;
class UDMMatchRuleDataAsset;
class UWorld;

/**
 * 로비 맵의 서버 전용 규칙 클래스.
 *
 * 플레이어 입장, Host 지정, Ready 요청 검증,
 * Match 시작 조건 판정과 ServerTravel을 담당한다.
 */

UCLASS()
class DONTMELT_API ADMLobbyGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ADMLobbyGameMode();

    virtual void InitGame(
        const FString& MapName,
        const FString& Options,
        FString& ErrorMessage
    ) override;

    /*
    현재 최대 인원 원본은 DA_DMMatchRules::MaxPlayerCount = 4 지만, 엔진 로그인 승인에는 AGameSession도 관여하기 때문에 
    GameSession의 최대 인원과 Data Asset 값이 다르면 Super::PreLogin() 단계에서 커스텀 검사보다 먼저 접속이 거부될 수 있음
	InitGame()은 다른 초기화 함수보다 먼저 호출되고 GameMode의 보조클래스를 초기화 하는 지점이므로 여기서 GameSession의 최대 인원을 Data Asset 값으로 동기화한다.
    */

    virtual void PreLogin(
        const FString& Options,
        const FString& Address,
        const FUniqueNetIdRepl& UniqueId,
        FString& ErrorMessage
    ) override;

    virtual void PostLogin(
        APlayerController* NewPlayer
    ) override;

    /**
     * PlayerController의 Server RPC로부터 Ready 요청을 받는다.
     *
     * GameMode 서버 인스턴스에서만 호출된다.
     */
    void HandleReadyRequest(
        ADMLobbyPlayerController* RequestingController,
        bool bNewReady
    );

    /**
     * PlayerController의 Server RPC로부터 시작 요청을 받는다.
     *
     * 요청자 Host 여부와 로비 상태를 모두 재검증한다.
     */
    void HandleStartMatchRequest(
        ADMLobbyPlayerController* RequestingController
    );

private:
    /**
     * 로비 시작 인원과 시간 설정의 원본 Data Asset.
     *
     * BP_DMLobbyGameMode에서 DA_DMMatchRules를 연결한다.
     */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "DM|Lobby|Rules",
        meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<UDMMatchRuleDataAsset> MatchRuleData = nullptr;

    /**
     * Match 시작 시 이동할 맵.
     *
     * BP_DMLobbyGameMode에서 L_Match_Test를 연결한다.
     */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "DM|Lobby|Travel",
        meta = (AllowPrivateAccess = "true")
    )
    TSoftObjectPtr<UWorld> MatchMap;

    /**
     * ServerTravel이 이미 확정됐는지 나타내는 서버 전용 잠금.
     *
     * 같은 프레임의 중복 클릭과 중복 RPC를 차단한다.
     */
    bool bTravelCommitted = false;

    /** Listen Server 로컬 플레이어를 방장으로 지정한다. */
    void AssignHostIfNeeded(
        ADMLobbyPlayerController* NewPlayerController
    );

    /** 이미 Host PlayerState가 있는지 확인한다. */
    bool HasAssignedHost();

    /** 요청자가 실제 Listen Server 방장인지 검증한다. */
    bool IsHostController(
        ADMLobbyPlayerController* RequestingController
    );

    /**
     * 전체 Match 시작 조건을 검사한다.
     *
     * 성공하면 OutConnectedPlayerCount에 시작 인원을 반환한다.
     */
    bool CanStartMatch(
        FString& OutFailureReason,
        int32& OutConnectedPlayerCount
    );

    /** Travel 잠금을 획득하고 ServerTravel을 실행한다. */
    bool CommitMatchTravel(
        int32 ExpectedPlayerCount
    );
};