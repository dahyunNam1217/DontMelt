#pragma once

#include "CoreMinimal.h"
#include "DMMatchTypes.generated.h"

/**
 * 매치 전체 진행 단계.
 *
 * 각 값은 서로 동시에 존재할 수 없는 단일 Phase다.
 */
UENUM(BlueprintType)
enum class EDMMatchPhase : uint8
{
    Waiting         UMETA(DisplayName = "Waiting"),
    Countdown       UMETA(DisplayName = "Countdown"),
    Playing         UMETA(DisplayName = "Playing"),
    OvertimeIntro   UMETA(DisplayName = "Overtime Intro"),
    Overtime        UMETA(DisplayName = "Overtime"),
    Finished        UMETA(DisplayName = "Finished"),
    Aborted         UMETA(DisplayName = "Aborted")
};

/**
 * 플레이어의 매치 생명주기 상태.
 *
 * SpawnProtected는 Alive 위에 중첩되는 상태 효과이므로
 * 이 Enum에 포함하지 않는다.
 */
UENUM(BlueprintType)
enum class EDMPlayerLifeState : uint8
{
    Alive           UMETA(DisplayName = "Alive"),
    Dead            UMETA(DisplayName = "Dead"),
    Spectating      UMETA(DisplayName = "Spectating"),
    Respawning      UMETA(DisplayName = "Respawning"),
    Disconnected    UMETA(DisplayName = "Disconnected")
};

/**
 * 매치가 종료된 구체적인 원인.
 *
 * Finished 또는 Aborted는 MatchPhase이고,
 * 이 Enum은 해당 상태에 진입한 이유를 표현한다.
 */
UENUM(BlueprintType)
enum class EDMMatchEndReason : uint8
{
    None                    UMETA(DisplayName = "None"),
    RelicVictory            UMETA(DisplayName = "Relic Victory"),
    EliminationVictory      UMETA(DisplayName = "Elimination Victory"),
    RegularTimeVictory      UMETA(DisplayName = "Regular Time Victory"),
    OvertimeVictory         UMETA(DisplayName = "Overtime Victory"),
    AllDefeated             UMETA(DisplayName = "All Defeated"),
    PlayerDisconnect        UMETA(DisplayName = "Player Disconnect"),
    HostLeft                UMETA(DisplayName = "Host Left")
};

/**
 * 클라이언트에 함께 전달할 매치 Phase 상태.
 *
 * CurrentPhase와 PhaseEndServerTime을 하나의 Struct로 묶어
 * 이후 하나의 RepNotify 단위로 관리한다.
 */
USTRUCT(BlueprintType)
struct DONTMELT_API FDMMatchPhaseState
{
    GENERATED_BODY()

    /** 현재 매치 Phase. */
    UPROPERTY(BlueprintReadOnly, Category = "DM|Match")
    EDMMatchPhase CurrentPhase = EDMMatchPhase::Waiting;

    /**
     * 현재 Phase가 종료될 서버 시간.
     *
     * Waiting, Finished, Aborted처럼 종료 Timer가 없는 Phase에서는
     * 0.0을 사용한다.
     */
    UPROPERTY(BlueprintReadOnly, Category = "DM|Match")
    double PhaseEndServerTime = 0.0;
};