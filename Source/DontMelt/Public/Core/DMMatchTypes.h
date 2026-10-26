#pragma once

#include "CoreMinimal.h"
#include "DMMatchTypes.generated.h"

class APawn;

/**
 * 매치 전체 진행 단계.
 *
 * 각 값은 서로 동시에 존재할 수 없는 단일 Phase다.
 */
UENUM(BlueprintType)
enum class EDMMatchPhase : uint8
{
    Waiting UMETA(DisplayName = "Waiting"),
    Countdown UMETA(DisplayName = "Countdown"),
    Playing UMETA(DisplayName = "Playing"),
    OvertimeIntro UMETA(DisplayName = "Overtime Intro"),
    Overtime UMETA(DisplayName = "Overtime"),
    Finished UMETA(DisplayName = "Finished"),
    Aborted UMETA(DisplayName = "Aborted")
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
    Alive UMETA(DisplayName = "Alive"),
    Dead UMETA(DisplayName = "Dead"),
    Spectating UMETA(DisplayName = "Spectating"),
    Respawning UMETA(DisplayName = "Respawning"),
    Disconnected UMETA(DisplayName = "Disconnected")
};

/**
 * 사망 후 30초 관전 구간의 로컬 Presentation 단계.
 *
 * Gameplay 생명주기 원본은 EDMPlayerLifeState이며,
 * 이 Enum은 LifeState == Spectating인 동안
 * 소유 Client의 카메라 표현 단계만 구분한다.
 *
 * Server Gameplay 판정이나 Replication 원본으로 사용하지 않는다.
 */
UENUM(BlueprintType)
enum class EDMSpectatorPhase : uint8
{
    Corpse UMETA(DisplayName = "Corpse"),
    KillerView UMETA(DisplayName = "Killer View"),
    FreeCamera UMETA(DisplayName = "Free Camera"),
    Fade UMETA(DisplayName = "Fade")
};

/**
 * Local Player에게 적용할 Voice Routing / Playback 정책.
 *
 * 실제 Voice Backend 상태의 SSOT가 아니며,
 * UDMVoiceChatSubsystem이 소비하는 Gameplay Policy다.
 */
UENUM(BlueprintType)
enum class EDMVoiceMode : uint8
{
    /**
     * Voice 송수신 비활성.
     */
    Disabled UMETA(DisplayName = "Disabled"),

    /**
     * Lobby 음성 정책.
     */
    Lobby UMETA(DisplayName = "Lobby"),

    /**
     * 정상 생존 Gameplay.
     *
     * World 거리 기반 근접 음성을 사용한다.
     */
    AliveProximity UMETA(DisplayName = "Alive Proximity"),

    /**
     * 사망 후 Respawn 완료 전까지의 Voice 정책.
     *
     * 별도의 사망자 전용 채널이 아니다.
     *
     * - 모든 정상 접속 플레이어의 현재 음성을 청취
     * - 거리 감쇠 없음
     * - 방향성 없음
     * - 화자 위치 표시 없음
     * - 화자 이름 표시 없음
     * - Local 사망자도 송신 가능
     */
    DeadGlobal UMETA(DisplayName = "Dead Global")
};

/**
 * 사망자 Client가 30초 관전 Sequence를 시작할 때
 * Server에서 한 번 전달받는 Presentation Context.
 *
 * Gameplay 상태의 새로운 원본이 아니라
 * 관전 카메라 시작에 필요한 일회성 Snapshot이다.
 */
USTRUCT()
struct DONTMELT_API FDMSpectatorContext
{
    GENERATED_BODY()

    /**
     * 서버가 사망을 확정한 순간의 피해자 위치.
     *
     * DeathCam의 기준 위치 및
     * 사망 Pawn을 사용할 수 없는 경우의 fallback에 사용한다.
     */
    UPROPERTY()
    FVector DeathLocation = FVector::ZeroVector;

    /**
     * 사망을 발생시킨 공격자의 Pawn.
     *
     * Killer View를 위한 일회성 참조이며,
     * Killer가 사망하거나 제거되면 이후 단계에서
     * fallback Camera 정책을 적용한다.
     */
    UPROPERTY()
    TObjectPtr<APawn> KillerPawn = nullptr;

    /**
     * 30초 Spectator Sequence가 시작된 서버 시간.
     *
     * Client RPC 도착 지연이 있더라도
     * 현재 ServerTime과 비교해 Presentation 진행 위치를
     * 보정할 수 있도록 사용한다.
     */
    UPROPERTY()
    double SpectatorStartServerTime = 0.0;
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
    None UMETA(DisplayName = "None"),
    RelicVictory UMETA(DisplayName = "Relic Victory"),
    EliminationVictory UMETA(DisplayName = "Elimination Victory"),
    RegularTimeVictory UMETA(DisplayName = "Regular Time Victory"),
    OvertimeVictory UMETA(DisplayName = "Overtime Victory"),
    AllDefeated UMETA(DisplayName = "All Defeated"),
    PlayerDisconnect UMETA(DisplayName = "Player Disconnect"),
    HostLeft UMETA(DisplayName = "Host Left"),
    // 연장전 종료 시 등록 유물 최다 참가자가 여러 명인 경우다.
    OvertimeDraw UMETA(DisplayName = "Overtime Draw"),
    // 서버 내부 검증 실패로 정상 결과를 만들 수 없어 중단된 경우다.
    InternalError UMETA(DisplayName = "Internal Error")
};

/**
 * 최종 경기 결과에 포함할 참가자 한 명의 통계.
 */
USTRUCT(BlueprintType)
struct DONTMELT_API FDMMatchResultEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "DM|Match|Result")
    FString PlayerName;

    UPROPERTY(BlueprintReadOnly, Category = "DM|Match|Result")
    int32 BaseSlot = INDEX_NONE;

    // 종료 순간 상자에 등록된 유물 수. 결과표 등수 계산 기준이다.
    UPROPERTY(BlueprintReadOnly, Category = "DM|Match|Result")
    int32 RelicCount = 0;

    // 경기 중 실제로 획득한 누적 유물 수. 결과표 '획득한 유물 수' 열에 표시한다.
    UPROPERTY(BlueprintReadOnly, Category = "DM|Match|Result")
    int32 AcquiredRelicCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DM|Match|Result")
    int32 KillCount = 0;

    // 등록 유물 수 기준 등수. 같은 등록 수는 같은 등수(공동 순위)를 가진다.
    // INDEX_NONE이면 등수를 표시하지 않는다.
    UPROPERTY(BlueprintReadOnly, Category = "DM|Match|Result")
    int32 Rank = INDEX_NONE;
};

/**
 * Match의 최종 결과.
 *
 * GameMode가 서버에서 한 번 확정하고
 * GameState가 전체 Client에 복제한다.
 */
USTRUCT(BlueprintType)
struct DONTMELT_API FDMMatchResult
{
    GENERATED_BODY()

    /** Match 종료 원인. */
    UPROPERTY(BlueprintReadOnly, Category = "DM|Match|Result")
    EDMMatchEndReason EndReason = EDMMatchEndReason::None;

    /**
     * 승리한 진영.
     *
     * 승자가 없는 결과 또는 Abort에서는
     * INDEX_NONE을 사용한다.
     */
    UPROPERTY(BlueprintReadOnly, Category = "DM|Match|Result")
    int32 WinningBaseSlot = INDEX_NONE;

    /** 결과가 서버에서 확정된 서버 시간. */
    UPROPERTY(BlueprintReadOnly, Category = "DM|Match|Result")
    double ResultServerTime = 0.0;

    /** 서버가 결과를 확정할 때 발급한다. 수신 확인과 중복 보관 방지에 사용한다. */
    UPROPERTY(BlueprintReadOnly, Category = "DM|Match|Result")
    FGuid ResultId;

    UPROPERTY(BlueprintReadOnly, Category = "DM|Match|Result")
    TArray<FDMMatchResultEntry> PlayerResults;
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

/** 공개 시작 순간의 유물 한 개 위치. 운반자나 진영 정보는 포함하지 않는다. */
USTRUCT(BlueprintType)
struct DONTMELT_API FDMRelicRevealEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "DM|Match|Reveal")
    int32 MarkerId = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly, Category = "DM|Match|Reveal")
    FVector WorldLocation = FVector::ZeroVector;
};

/** 서버가 허용한 3초 동안에만 채워지는 공개용 위치 Snapshot. */
USTRUCT(BlueprintType)
struct DONTMELT_API FDMRelicRevealState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "DM|Match|Reveal")
    double EndServerTime = 0.0;

    UPROPERTY(BlueprintReadOnly, Category = "DM|Match|Reveal")
    TArray<FDMRelicRevealEntry> Entries;

    bool HasPayload() const
    {
        return EndServerTime > 0.0 && !Entries.IsEmpty();
    }
};

/**
 * 연장전 진입 시 공개할 진영 하나의 등록 유물 개수.
 * 플레이어 식별 정보는 포함하지 않는다.
 */
USTRUCT(BlueprintType)
struct DONTMELT_API FDMOvertimeBaseCount
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "DM|Match|Overtime")
    int32 BaseSlot = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly, Category = "DM|Match|Overtime")
    int32 RegisteredRelicCount = 0;
};

/**
 * OvertimeIntro에서만 공개할 진영별 유물 개수 Snapshot.
 *
 * IntroEndServerTime은 기존 Phase와 Snapshot이
 * 동일한 연장전 진입에 속하는지 검증하기 위한 값이다.
 */
USTRUCT(BlueprintType)
struct DONTMELT_API FDMOvertimeIntroState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "DM|Match|Overtime")
    double IntroEndServerTime = 0.0;

    UPROPERTY(BlueprintReadOnly, Category = "DM|Match|Overtime")
    TArray<FDMOvertimeBaseCount> BaseCounts;

    bool HasPayload() const
    {
        return IntroEndServerTime > 0.0 && !BaseCounts.IsEmpty();
    }
};
