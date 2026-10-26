#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DMLobbyGameMode.generated.h"

class ADMLobbyPlayerController;
class UDMMatchRuleDataAsset;
class UWorld;
class UDMOnlineSessionSubsystem;

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
#pragma region Lifecycle

    ADMLobbyGameMode();

    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

    virtual void PreLogin(
        const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage
    ) override;

    virtual void PostLogin(APlayerController* NewPlayer) override;

    virtual void StartPlay() override;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#pragma endregion

#pragma region Requests

    void HandleReadyRequest(ADMLobbyPlayerController* RequestingController, bool bNewReady);

    void HandleStartMatchRequest(ADMLobbyPlayerController* RequestingController);

#pragma endregion

private:
#pragma region Host

    void AssignHostIfNeeded(ADMLobbyPlayerController* NewPlayerController);

    bool HasAssignedHost();

    bool IsHostController(ADMLobbyPlayerController* RequestingController);

#pragma endregion

#pragma region Match Start

    bool CanStartMatch(FString& OutFailureReason, int32& OutConnectedPlayerCount);

    bool CommitMatchTravel(int32 ExpectedPlayerCount);

#pragma endregion

#pragma region Steam Match Start

    // 이전 매치에서 돌아온 참가자가 모두 도착했는지 확인한다.
    void TryReopenReturningLobby();

    // Steam 세션 광고 상태 갱신 완료를 처리한다.
    void HandleLobbyStatusUpdated(bool bWasSuccessful, const FString& Detail);

    // 경기 시작 실패 후 광고 상태를 Lobby로 복구한다.
    void ScheduleLobbyStatusRestore();
    void RestoreLobbyStatus();

    // GameMode에서 등록한 완료 이벤트만 제거한다.
    void UnbindLobbyStatusCompletion();

    // 경기 시작 잠금(bTravelCommitted)과 복구 상태를 해제한다.
    // 복구가 실패해도 로비가 영구 잠기지 않도록 모든 실패 경로에서 호출한다.
    void ReleaseMatchStartLock(const TCHAR* Reason);

    // 로비 상태 복구 실패 시 제한 횟수만큼 재시도하고, 한도를 넘기면 시작 잠금을 해제한다.
    void RetryOrReleaseLobbyStatusRestore(const TCHAR* Reason);

    // 복귀 인원이 제한 시간 안에 모두 도착하지 않았을 때 도착 인원 기준으로 로비를 재개한다.
    void HandleReturningPlayersTimeout();

#pragma endregion

#pragma region Configuration

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DM|Lobby|Rules", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UDMMatchRuleDataAsset> MatchRuleData = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DM|Lobby|Travel", meta = (AllowPrivateAccess = "true"))
    TSoftObjectPtr<UWorld> MatchMap;

    // 복귀 인원 대기 최대 시간(초). 초과 시 도착한 인원 기준으로 로비를 재개한다.
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "DM|Lobby|Travel",
        meta = (AllowPrivateAccess = "true", ClampMin = "5.0")
    )
    float ReturningPlayersTimeoutSeconds = 30.0f;

#pragma endregion

#pragma region Runtime State

    // 같은 프레임의 중복 시작 요청과 중복 ServerTravel을 차단한다.
    bool bTravelCommitted = false;

    // 세션 갱신 요청 전 승인한 참가자 수다.
    int32 PendingMatchPlayerCount = 0;

    // 매치 결과 후 복귀해야 하는 기존 참가자 수다.
    int32 ExpectedReturningPlayers = 0;

    // 기존 참가자의 복귀 완료 후 광고 상태를 열고 있는지 나타낸다.
    bool bReopeningReturningLobby = false;

    // 복구 요청과 최초 Starting 갱신을 구별한다.
    bool bRestoringLobbyStatus = false;

    // 비동기 작업 중 Subsystem 객체를 약한 참조로 유지한다.
    TWeakObjectPtr<UDMOnlineSessionSubsystem> BoundLobbySession;

    FDelegateHandle LobbyStatusUpdateHandle;

    // 세션 작업 중이라 복구를 시작하지 못했을 때의 재시도 타이머와 횟수.
    FTimerHandle LobbyStatusRestoreRetryTimer;

    int32 LobbyStatusRestoreAttemptCount = 0;

    static constexpr int32 MaxLobbyStatusRestoreAttempts = 5;

    // 복귀 인원 대기 타임아웃 및 재개 실패 재시도 타이머.
    FTimerHandle ReturningPlayersTimeoutTimer;

#pragma endregion
};
