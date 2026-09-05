// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"
#include "Core/DMMatchTypes.h"
#include "DMMatchGameMode.generated.h"

class ADMMatchPlayerState;
class APlayerStart;
class UDMMatchRuleDataAsset;

/**
 * Match 서버의 권위 규칙 클래스.
 *
 * 참가자 입장, BaseSlot 할당, 초기 스폰 검증,
 * 시작 인원 Snapshot 확정을 담당한다.
 */
UCLASS()
class DONTMELT_API ADMMatchGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ADMMatchGameMode();

    /**
     * Lobby가 Travel URL로 전달한 ExpectedPlayers를 읽는다.
     */
    virtual void InitGame(
        const FString& MapName,
        const FString& Options,
        FString& ErrorMessage
    ) override;

    /**
     * Match 입장 가능 여부를 서버에서 검사한다.
     */
    virtual void PreLogin(
        const FString& Options,
        const FString& Address,
        const FUniqueNetIdRepl& UniqueId,
        FString& ErrorMessage
    ) override;

    /**
     * PlayerController와 PlayerState가 생성된 후
     * BaseSlot을 할당한다.
     */
    virtual void PostLogin(
        APlayerController* NewPlayer
    ) override;


    /**
     * Match에 입장한 Controller가 서버에서 제거될 때 호출된다.
     *
     * 원격 참가자의 실제 연결 종료라면
     * Match를 PlayerDisconnect 사유로 중단한다.
     */
    virtual void Logout(
        AController* Exiting
    ) override;


    /**
     * PostLogin 이후 엔진 기본 Pawn 스폰을 실행하고,
     * 실제 Pawn과 Possess 성공 여부를 검증한다.
     */
    virtual void HandleStartingNewPlayer_Implementation(
        APlayerController* NewPlayer
    ) override;

    /**
     * PlayerState의 BaseSlot에 대응하는 PlayerStart를 반환한다.
     */
    virtual AActor* ChoosePlayerStart_Implementation(
        AController* Player
    ) override;

    /**
     * 정상적인 게임 결과로 Match를 종료한다.
     *
     * 서버 C++ 시스템만 호출한다.
     */
    bool TryFinishMatch(
        EDMMatchEndReason EndReason
    );

    /**
     * 정상 결과를 만들 수 없는 상황에서 Match를 중단한다.
     *
     * 서버 C++ 시스템만 호출한다.
     */
    bool TryAbortMatch(
        EDMMatchEndReason EndReason
    );

    /**
     * 정규 시간 공동 1위 판정 후
     * OvertimeIntro Phase에 진입한다.
     *
     * 실제 공동 1위 판정은 유물 시스템 구현 후 연결한다.
     */
    bool TryStartOvertimeIntro();

protected:
    /**
     * 엔진이 최초 PlayerStart를 선택하기 전에
     * 플레이어의 BaseSlot을 확정한다.
     */
    virtual bool UpdatePlayerStartSpot(
        AController* Player,
        const FString& Portal,
        FString& OutErrorMessage
    ) override;


private:
    /**
     * Match에서 사용할 규칙 원본.
     *
     * BP_DMMatchGameMode에서 DA_DMMatchRules를 연결한다.
     */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "DM|Match|Rules",
        meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<UDMMatchRuleDataAsset> MatchRuleData = nullptr;

    /**
     * Lobby가 URL Option으로 전달한 예상 참가 인원.
     */
    int32 ExpectedPlayerCount = 0;

    /**
     * 실제 입장·스폰 검증 후 고정된 시작 인원.
     *
     * 0은 아직 확정되지 않았다는 뜻이다.
     */
    int32 StartingPlayerCountSnapshot = 0;

    /**
     * 현재 Phase 종료를 담당하는 서버 Timer Handle.
     *
     * Countdown, Playing, OvertimeIntro, Overtime이
     * 동일한 Handle을 순차적으로 재사용한다.
     */
    FTimerHandle PhaseTimerHandle;

    /** 참가자 구성이 이미 잠겼는지 반환한다. */
    bool IsStartingRosterLocked() const;

    /** PlayerState에 비어 있는 BaseSlot을 할당한다. */
    bool TryAssignBaseSlot(
        ADMMatchPlayerState* MatchPlayerState
    );

    /** 현재 사용되지 않은 첫 BaseSlot을 찾는다. */
    int32 FindFirstAvailableBaseSlot() const;

    /**
     * BaseSlot에 정확히 대응하는 PlayerStart를 찾는다.
     *
     * 누락 또는 중복 태그라면 nullptr을 반환한다.
     */
    APlayerStart* FindPlayerStartForBaseSlot(
        int32 BaseSlot
    ) const;

    /**
     * 예상 인원 전원이 정상 스폰됐는지 검사한다.
     */
    bool IsStartingRosterComplete(
        FString& OutFailureReason
    ) const;

    /**
     * 전원 검증이 끝났다면 시작 인원 Snapshot을 확정한다.
     */
    void TryFinalizeStartingRoster();

    /** Starting Roster 완료 후 Countdown을 시작한다. */
    void StartCountdown();

    /** Countdown 서버 Timer가 만료됐을 때 호출된다. */
    void HandleCountdownExpired();

    /**
     * Playing Phase에 진입한다.
     *
     * 정규 경기 종료 Timer는 2-8에서 추가한다.
     */
    void StartPlaying();

    /**
     * 정규 경기 서버 Timer가 만료됐을 때 호출된다.
     *
     * 실제 승자·연장전 판정은 이후 단계에서 연결한다.
     */
    void HandleRegularTimeExpired();

    /**
     * 모든 Match Phase 변경이 통과하는 중앙 함수.
     */
    bool TryTransitionPhase(
        EDMMatchPhase TargetPhase,
        double PhaseEndServerTime
    );

    /** From → To 전환이 허용되는지 검사한다. */
    static bool IsPhaseTransitionAllowed(
        EDMMatchPhase FromPhase,
        EDMMatchPhase ToPhase
    );

    /** Finished에 사용할 수 있는 종료 원인인지 검사한다. */
    static bool IsFinishReason(
        EDMMatchEndReason EndReason
    );

    /**
     * 현재 Match Phase에서 해당 정상 종료 사유를
     * 사용할 수 있는지 검사한다.
     */
    static bool IsFinishReasonAllowedForPhase(
        EDMMatchPhase Phase,
        EDMMatchEndReason EndReason
    );

    /** Aborted에 사용할 수 있는 종료 원인인지 검사한다. */
    static bool IsAbortReason(
        EDMMatchEndReason EndReason
    );

    /** OvertimeIntro Timer 만료 콜백. */
    void HandleOvertimeIntroExpired();

    /** OvertimeIntro에서 Overtime으로 진입한다. */
    void StartOvertime();

    /** Overtime Timer 만료 콜백. */
    void HandleOvertimeExpired();



};