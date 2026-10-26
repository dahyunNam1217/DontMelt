#include "Framework/Lobby/DMLobbyGameMode.h"

#include "Core/DMMatchConstants.h"

#include "Data/DMMatchRuleDataAsset.h"
#include "Framework/Lobby/DMLobbyGameState.h"
#include "Framework/Lobby/DMLobbyPlayerController.h"
#include "Framework/Lobby/DMLobbyPlayerState.h"
#include "Engine/World.h"
#include "Online/DMOnlineSessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerState.h"
#include "UObject/SoftObjectPath.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogDMLobbyGameMode, Log, All);

#pragma region Lifecycle

ADMLobbyGameMode::ADMLobbyGameMode()
{
    GameStateClass = ADMLobbyGameState::StaticClass();
    PlayerStateClass = ADMLobbyPlayerState::StaticClass();
    PlayerControllerClass = ADMLobbyPlayerController::StaticClass();

    DefaultPawnClass = nullptr;

    // Lobby와 Match는 맵 전환 시 Framework 객체를 새로 생성한다.
    bUseSeamlessTravel = false;
}

void ADMLobbyGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);

    if (!ErrorMessage.IsEmpty())
    {
        return;
    }

    // 일반 방 생성이면 0, 이전 매치에서 복귀 중이면 1~4다.
    ExpectedReturningPlayers = UGameplayStatics::GetIntOption(Options, TEXT("ReturningPlayers"), 0);

    if (ExpectedReturningPlayers < 0 || ExpectedReturningPlayers > DMMatch::MaxPlayers)
    {
        ErrorMessage = TEXT("Invalid returning lobby player count.");
        return;
    }

    if (!ensureMsgf(IsValid(MatchRuleData), TEXT("MatchRuleData is not assigned.")))
    {
        return;
    }

    if (!IsValid(GameSession))
    {
        UE_LOG(LogDMLobbyGameMode, Error, TEXT("GameSession was not created."));

        return;
    }

    GameSession->MaxPlayers = FMath::Clamp(MatchRuleData->MaxPlayerCount, 2, 4);

    UE_LOG(
        LogDMLobbyGameMode,
        Log,
        TEXT("Lobby capacity initialized. RuleMaxPlayers=%d, GameSessionMaxPlayers=%d"),
        MatchRuleData->MaxPlayerCount,
        GameSession->MaxPlayers
    );
}

void ADMLobbyGameMode::PreLogin(
    const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage
)
{
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);

    if (!ErrorMessage.IsEmpty())
    {
        return;
    }

    // 매치에서 로비로 돌아오는 동안은 이전 참가자만 허용한다.
    if (ExpectedReturningPlayers > 0)
    {
        UDMOnlineSessionSubsystem* Session =
            GetGameInstance() ? GetGameInstance()->GetSubsystem<UDMOnlineSessionSubsystem>() : nullptr;

        if (!IsValid(Session) || !Session->HasApprovedMatchRoster() || !Session->IsApprovedMatchPlayer(UniqueId))
        {
            ErrorMessage = TEXT("Not approved for returning lobby.");
            return;
        }

        if (GetNumPlayers() >= ExpectedReturningPlayers)
        {
            ErrorMessage = TEXT("Returning lobby is full.");
            return;
        }
    }

    if (bTravelCommitted)
    {
        ErrorMessage = TEXT("Match is starting.");
        return;
    }

    if (!ensureMsgf(
            IsValid(MatchRuleData), TEXT("MatchRuleData is not assigned. Maximum player validation was skipped.")
        ))
    {
        ErrorMessage = TEXT("Lobby configuration is invalid.");
        return;
    }

    UE_LOG(
        LogDMLobbyGameMode,
        Log,
        TEXT("PreLogin capacity check. CurrentPlayers=%d, RuleMaxPlayers=%d, GameSessionMaxPlayers=%d"),
        GetNumPlayers(),
        MatchRuleData->MaxPlayerCount,
        IsValid(GameSession) ? GameSession->MaxPlayers : -1
    );

    if (GetNumPlayers() >= MatchRuleData->MaxPlayerCount)
    {
        UE_LOG(
            LogDMLobbyGameMode,
            Verbose,
            TEXT("PreLogin rejected: lobby is full. CurrentPlayers=%d, MaxPlayers=%d"),
            GetNumPlayers(),
            MatchRuleData->MaxPlayerCount
        );

        ErrorMessage = TEXT("Lobby is full.");
    }
}

void ADMLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    ADMLobbyPlayerController* LobbyPlayerController = Cast<ADMLobbyPlayerController>(NewPlayer);

    if (!ensureMsgf(LobbyPlayerController != nullptr, TEXT("PostLogin received an invalid LobbyPlayerController.")))
    {
        return;
    }

    ADMLobbyPlayerState* LobbyPlayerState = LobbyPlayerController->GetPlayerState<ADMLobbyPlayerState>();

    if (!ensureMsgf(LobbyPlayerState != nullptr, TEXT("LobbyPlayerState was not created for the joining player.")))
    {
        return;
    }

    AssignHostIfNeeded(LobbyPlayerController);

    // 이전 매치 참가자가 모두 도착했다면 Steam 로비를 재개한다.
    TryReopenReturningLobby();

    UE_LOG(
        LogDMLobbyGameMode,
        Log,
        TEXT("Lobby player joined. PlayerName=%s, ConnectedPlayers=%d"),
        *LobbyPlayerState->GetPlayerName(),
        GetNumPlayers()
    );
}

void ADMLobbyGameMode::StartPlay()
{
    Super::StartPlay();

    // 복귀 인원이 끝내 도착하지 않아도 로비가 영구 잠기지 않도록 타임아웃을 예약한다.
    if (ExpectedReturningPlayers > 0)
    {
        GetWorldTimerManager().SetTimer(
            ReturningPlayersTimeoutTimer,
            this,
            &ADMLobbyGameMode::HandleReturningPlayersTimeout,
            ReturningPlayersTimeoutSeconds,
            false
        );
    }

    ADMLobbyGameState* LobbyGameState = GetGameState<ADMLobbyGameState>();

    if (!IsValid(LobbyGameState) || !IsValid(MatchRuleData))
    {
        return;
    }

    LobbyGameState->SetLobbyPlayerLimits(MatchRuleData->MinPlayerCount, MatchRuleData->MaxPlayerCount);

    TryReopenReturningLobby();
}

void ADMLobbyGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 로비 월드가 종료되어도 Subsystem은 남을 수 있으므로 구독을 해제한다.
    UnbindLobbyStatusCompletion();

    // 이 GameMode가 예약한 복구·복귀 타이머를 정리한다.
    GetWorldTimerManager().ClearTimer(LobbyStatusRestoreRetryTimer);
    GetWorldTimerManager().ClearTimer(ReturningPlayersTimeoutTimer);

    PendingMatchPlayerCount = 0;
    bRestoringLobbyStatus = false;

    Super::EndPlay(EndPlayReason);
}

#pragma endregion

#pragma region Requests

void ADMLobbyGameMode::HandleReadyRequest(ADMLobbyPlayerController* RequestingController, const bool bNewReady)
{
    if (bTravelCommitted)
    {
        return;
    }

    if (!IsValid(RequestingController))
    {
        return;
    }

    if (RequestingController->GetWorld() != GetWorld())
    {
        return;
    }

    ADMLobbyPlayerState* LobbyPlayerState = RequestingController->GetPlayerState<ADMLobbyPlayerState>();

    if (LobbyPlayerState == nullptr)
    {
        return;
    }

    const bool bPreviousReady = LobbyPlayerState->IsReady();

    LobbyPlayerState->SetReadyState(bNewReady);

    UE_LOG(
        LogDMLobbyGameMode,
        Log,
        TEXT("Ready request processed. PlayerName=%s, Requested=%s, Previous=%s, Current=%s"),
        *LobbyPlayerState->GetPlayerName(),
        bNewReady ? TEXT("true") : TEXT("false"),
        bPreviousReady ? TEXT("true") : TEXT("false"),
        LobbyPlayerState->IsReady() ? TEXT("true") : TEXT("false")
    );
}

void ADMLobbyGameMode::HandleStartMatchRequest(ADMLobbyPlayerController* RequestingController)
{
    const ADMLobbyPlayerState* RequestingPlayerState =
        IsValid(RequestingController) ? RequestingController->GetPlayerState<ADMLobbyPlayerState>() : nullptr;

    UE_LOG(
        LogDMLobbyGameMode,
        Log,
        TEXT("Start request received. PlayerName=%s, LocalController=%s, Host=%s"),
        RequestingPlayerState != nullptr ? *RequestingPlayerState->GetPlayerName() : TEXT("Invalid"),
        IsValid(RequestingController) && RequestingController->IsLocalController() ? TEXT("true") : TEXT("false"),
        RequestingPlayerState != nullptr && RequestingPlayerState->IsHost() ? TEXT("true") : TEXT("false")
    );

    // 복귀 로비의 Steam 상태가 정상화되기 전에는 시작하지 않는다.
    if (ExpectedReturningPlayers > 0 || bReopeningReturningLobby)
    {
        UE_LOG(LogDMLobbyGameMode, Verbose, TEXT("Match start rejected: returning lobby is not ready."));
        return;
    }

    if (bTravelCommitted)
    {
        UE_LOG(LogDMLobbyGameMode, Verbose, TEXT("Start request ignored: travel is already committed."));

        return;
    }

    if (!IsHostController(RequestingController))
    {
        UE_LOG(LogDMLobbyGameMode, Verbose, TEXT("Start request rejected: requester is not the listen-server host."));

        return;
    }

    FString FailureReason;
    int32 ConnectedPlayerCount = 0;

    if (!CanStartMatch(FailureReason, ConnectedPlayerCount))
    {
        UE_LOG(LogDMLobbyGameMode, Verbose, TEXT("Start request rejected: %s"), *FailureReason);

        return;
    }

    UGameInstance* GameInstance = GetGameInstance();

    UDMOnlineSessionSubsystem* Session =
        IsValid(GameInstance) ? GameInstance->GetSubsystem<UDMOnlineSessionSubsystem>() : nullptr;

    if (!IsValid(Session))
    {
        UE_LOG(LogDMLobbyGameMode, Error, TEXT("Match start rejected: session subsystem unavailable."));

        return;
    }

    if (Session->IsBusy())
    {
        UE_LOG(LogDMLobbyGameMode, Verbose, TEXT("Match start rejected: session operation in progress."));

        return;
    }

    // 비동기 요청 중에는 Ready 변경, 신규 입장, 중복 시작을 막는다.
    bTravelCommitted = true;
    PendingMatchPlayerCount = ConnectedPlayerCount;
    bRestoringLobbyStatus = false;

    BoundLobbySession = Session;

    LobbyStatusUpdateHandle = Session->OnUpdateCompleted.AddUObject(this, &ADMLobbyGameMode::HandleLobbyStatusUpdated);

    UE_LOG(LogDMLobbyGameMode, Log, TEXT("Match start approved. Updating Steam lobby status."));

    const bool bRequestStarted = Session->UpdateLobbyMatchStarting(true);

    if (!bRequestStarted && bTravelCommitted && !bRestoringLobbyStatus)
    {
        // 즉시 실패하여 완료 이벤트가 실행되지 않은 경우를 처리한다.
        UnbindLobbyStatusCompletion();

        bTravelCommitted = false;
        PendingMatchPlayerCount = 0;

        UE_LOG(LogDMLobbyGameMode, Error, TEXT("Match start failed: Steam update request not started."));
    }
}

#pragma endregion

#pragma region Host

void ADMLobbyGameMode::AssignHostIfNeeded(ADMLobbyPlayerController* NewPlayerController)
{
    if (!IsValid(NewPlayerController))
    {
        return;
    }

    if (HasAssignedHost())
    {
        return;
    }

    const ENetMode NetMode = GetNetMode();

    const bool bIsSupportedLocalHost =
        NewPlayerController->IsLocalController() && (NetMode == NM_ListenServer || NetMode == NM_Standalone);

    if (!bIsSupportedLocalHost)
    {
        return;
    }

    ADMLobbyPlayerState* LobbyPlayerState = NewPlayerController->GetPlayerState<ADMLobbyPlayerState>();

    if (LobbyPlayerState == nullptr)
    {
        return;
    }

    LobbyPlayerState->SetHostState(true);

    UE_LOG(LogDMLobbyGameMode, Log, TEXT("Lobby host assigned. PlayerName=%s"), *LobbyPlayerState->GetPlayerName());
}

bool ADMLobbyGameMode::HasAssignedHost()
{
    const ADMLobbyGameState* LobbyGameState = GetGameState<ADMLobbyGameState>();

    if (LobbyGameState == nullptr)
    {
        return false;
    }

    for (APlayerState* PlayerState : LobbyGameState->PlayerArray)
    {
        const ADMLobbyPlayerState* LobbyPlayerState = Cast<ADMLobbyPlayerState>(PlayerState);

        if (LobbyPlayerState != nullptr && LobbyPlayerState->IsHost())
        {
            return true;
        }
    }

    return false;
}

bool ADMLobbyGameMode::IsHostController(ADMLobbyPlayerController* RequestingController)
{
    if (!IsValid(RequestingController))
    {
        return false;
    }

    if (RequestingController->GetWorld() != GetWorld())
    {
        return false;
    }

    const ADMLobbyPlayerState* LobbyPlayerState = RequestingController->GetPlayerState<ADMLobbyPlayerState>();

    if (LobbyPlayerState == nullptr)
    {
        return false;
    }

    return RequestingController->IsLocalController() && LobbyPlayerState->IsHost();
}

#pragma endregion

#pragma region Match Start

bool ADMLobbyGameMode::CanStartMatch(FString& OutFailureReason, int32& OutConnectedPlayerCount)
{
    OutFailureReason.Reset();
    OutConnectedPlayerCount = 0;

    if (!IsValid(MatchRuleData))
    {
        OutFailureReason = TEXT("MatchRuleData is not assigned.");

        return false;
    }

    const int32 MinPlayerCount = MatchRuleData->MinPlayerCount;

    const int32 MaxPlayerCount = MatchRuleData->MaxPlayerCount;

    if (MinPlayerCount < DMMatch::MinPlayers || MaxPlayerCount > DMMatch::MaxPlayers || MinPlayerCount > MaxPlayerCount)
    {
        OutFailureReason = TEXT("Match player rule values are invalid.");

        return false;
    }

    if (MatchMap.IsNull())
    {
        OutFailureReason = TEXT("MatchMap is not assigned.");

        return false;
    }

    const ADMLobbyGameState* LobbyGameState = GetGameState<ADMLobbyGameState>();

    if (LobbyGameState == nullptr)
    {
        OutFailureReason = TEXT("LobbyGameState is not available.");

        return false;
    }

    int32 HostCount = 0;
    FString FirstNotReadyPlayerName;

    for (APlayerState* PlayerState : LobbyGameState->PlayerArray)
    {
        const ADMLobbyPlayerState* LobbyPlayerState = Cast<ADMLobbyPlayerState>(PlayerState);

        /*
         * 반드시 LobbyPlayerState를 사용하기 전에
         * 유효성을 검사해야 한다.
         */
        if (LobbyPlayerState == nullptr)
        {
            OutFailureReason = TEXT("Lobby PlayerArray contains an invalid PlayerState class.");

            return false;
        }

        UE_LOG(
            LogDMLobbyGameMode,
            Log,
            TEXT("Lobby player state check. PlayerName=%s, Ready=%s, Host=%s"),
            *LobbyPlayerState->GetPlayerName(),
            LobbyPlayerState->IsReady() ? TEXT("true") : TEXT("false"),
            LobbyPlayerState->IsHost() ? TEXT("true") : TEXT("false")
        );

        ++OutConnectedPlayerCount;

        if (LobbyPlayerState->IsHost())
        {
            ++HostCount;
        }

        if (!LobbyPlayerState->IsReady() && FirstNotReadyPlayerName.IsEmpty())
        {
            FirstNotReadyPlayerName = LobbyPlayerState->GetPlayerName();
        }
    }

    /*
     * PlayerArray와 GameMode가 인식하는 실제 Player 수가
     * 일치하지 않으면 아직 동기화가 완료되지 않은 상태다.
     */
    if (OutConnectedPlayerCount != GetNumPlayers())
    {
        OutFailureReason = TEXT("PlayerState synchronization is not complete.");

        return false;
    }

    if (OutConnectedPlayerCount < MinPlayerCount || OutConnectedPlayerCount > MaxPlayerCount)
    {
        OutFailureReason = FString::Printf(
            TEXT("Player count must be between %d and %d. Current=%d"),
            MinPlayerCount,
            MaxPlayerCount,
            OutConnectedPlayerCount
        );

        return false;
    }

    if (HostCount != 1)
    {
        OutFailureReason = FString::Printf(TEXT("Lobby must have exactly one host. CurrentHostCount=%d"), HostCount);

        return false;
    }

    if (!FirstNotReadyPlayerName.IsEmpty())
    {
        OutFailureReason =
            FString::Printf(TEXT("Not every player is ready. FirstNotReadyPlayer=%s"), *FirstNotReadyPlayerName);

        return false;
    }

    return true;
}

bool ADMLobbyGameMode::CommitMatchTravel(const int32 ExpectedPlayerCount)
{
    // Steam 세션 갱신을 승인한 흐름에서만 호출한다.
    if (!bTravelCommitted)
    {
        return false;
    }

    UWorld* World = GetWorld();

    if (World == nullptr || MatchMap.IsNull())
    {
        return false;
    }

    const FString MatchMapPackageName = MatchMap.ToSoftObjectPath().GetLongPackageName();

    if (MatchMapPackageName.IsEmpty())
    {
        return false;
    }

    const FString TravelURL =
        FString::Printf(TEXT("%s?listen?ExpectedPlayers=%d"), *MatchMapPackageName, ExpectedPlayerCount);

    UE_LOG(LogDMLobbyGameMode, Log, TEXT("Match travel committed. URL=%s"), *TravelURL);

    const bool bTravelStarted = World->ServerTravel(TravelURL, false, false);

    /*
     * 실패 시 잠금 해제는 호출자가 ScheduleLobbyStatusRestore()로
     * Steam 광고 상태를 되돌린 뒤 처리한다.
     * 여기서 먼저 bTravelCommitted를 false로 바꾸면
     * ScheduleLobbyStatusRestore()가 즉시 return되어
     * Steam 로비가 '경기 시작 중' 상태로 남는다.
     */

    return bTravelStarted;
}

#pragma endregion

#pragma region Steam Match Start

void ADMLobbyGameMode::TryReopenReturningLobby()
{
    if (!HasAuthority() || bTravelCommitted || bReopeningReturningLobby || ExpectedReturningPlayers <= 0
        || GetNumPlayers() != ExpectedReturningPlayers)
    {
        return;
    }

    UDMOnlineSessionSubsystem* Session =
        GetGameInstance() ? GetGameInstance()->GetSubsystem<UDMOnlineSessionSubsystem>() : nullptr;

    const ADMLobbyGameState* LobbyGameState = GetGameState<ADMLobbyGameState>();

    if (!IsValid(Session) || Session->IsBusy() || !Session->HasApprovedMatchRoster() || !IsValid(LobbyGameState)
        || LobbyGameState->PlayerArray.Num() != ExpectedReturningPlayers)
    {
        return;
    }

    // 복귀한 모든 PlayerState가 승인 명단에 있는지 마지막으로 확인한다.
    for (const APlayerState* PlayerState : LobbyGameState->PlayerArray)
    {
        if (!IsValid(PlayerState) || !Session->IsApprovedMatchPlayer(PlayerState->GetUniqueId()))
        {
            return;
        }
    }

    bReopeningReturningLobby = true;
    BoundLobbySession = Session;

    LobbyStatusUpdateHandle = Session->OnUpdateCompleted.AddUObject(this, &ADMLobbyGameMode::HandleLobbyStatusUpdated);

    UE_LOG(LogDMLobbyGameMode, Log, TEXT("Returning participants arrived. Requesting Steam lobby reopen."));

    const bool bStarted = Session->UpdateLobbyMatchStarting(false);

    if (!bStarted && bReopeningReturningLobby)
    {
        UnbindLobbyStatusCompletion();
        bReopeningReturningLobby = false;

        UE_LOG(LogDMLobbyGameMode, Error, TEXT("Failed to request Steam lobby reopen."));
    }
}

void ADMLobbyGameMode::HandleLobbyStatusUpdated(const bool bWasSuccessful, const FString& Detail)
{
    // 이전 매치에서 돌아온 로비의 광고 상태 복원 결과다.
    if (bReopeningReturningLobby)
    {
        UDMOnlineSessionSubsystem* Session = BoundLobbySession.Get();

        UnbindLobbyStatusCompletion();
        bReopeningReturningLobby = false;

        if (bWasSuccessful && IsValid(Session))
        {
            Session->ClearApprovedMatchRoster();
            ExpectedReturningPlayers = 0;

            // 재개가 완료되었으므로 복귀 대기 타임아웃을 해제한다.
            GetWorldTimerManager().ClearTimer(ReturningPlayersTimeoutTimer);

            UE_LOG(LogDMLobbyGameMode, Log, TEXT("Steam lobby is joinable again."));
        }
        else
        {
            UE_LOG(LogDMLobbyGameMode, Error, TEXT("Steam lobby reopen failed: %s"), *Detail);

            // 재개 실패 시 로비가 영구 잠기지 않도록 5초 후 다시 시도한다.
            GetWorldTimerManager().SetTimer(
                ReturningPlayersTimeoutTimer, this, &ADMLobbyGameMode::HandleReturningPlayersTimeout, 5.0f, false
            );
        }

        return;
    }

    if (!bTravelCommitted)
    {
        return;
    }

    const bool bWasRestoring = bRestoringLobbyStatus;

    // 현재 완료된 요청의 구독을 먼저 제거한다.
    UnbindLobbyStatusCompletion();

    if (bWasRestoring)
    {
        if (bWasSuccessful)
        {
            // 복구에 성공했으므로 새로운 시작 요청을 허용한다.
            ReleaseMatchStartLock(TEXT("Lobby status restored. Match start unlocked."));
        }
        else
        {
            UE_LOG(LogDMLobbyGameMode, Error, TEXT("Lobby status restore failed: %s"), *Detail);

            // 복구 실패 시 제한 횟수만큼 다시 복구하고,
            // 그래도 실패하면 로비가 영구 잠기지 않도록 시작 잠금을 해제한다.
            RetryOrReleaseLobbyStatusRestore(TEXT("Lobby status restore failed."));
        }

        return;
    }

    if (!bWasSuccessful)
    {
        UE_LOG(LogDMLobbyGameMode, Error, TEXT("Steam lobby status update failed: %s"), *Detail);

        ScheduleLobbyStatusRestore();
        return;
    }

    // 갱신 도중 참가자가 나갔을 수 있으므로 서버 조건을 재검사한다.
    FString FailureReason;
    int32 CurrentPlayerCount = 0;

    if (!CanStartMatch(FailureReason, CurrentPlayerCount) || CurrentPlayerCount != PendingMatchPlayerCount)
    {
        UE_LOG(
            LogDMLobbyGameMode,
            Warning,
            TEXT("Match start cancelled: lobby state changed. Reason=%s, Current=%d, Expected=%d"),
            *FailureReason,
            CurrentPlayerCount,
            PendingMatchPlayerCount
        );

        ScheduleLobbyStatusRestore();
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();

    UDMOnlineSessionSubsystem* Session =
        IsValid(GameInstance) ? GameInstance->GetSubsystem<UDMOnlineSessionSubsystem>() : nullptr;

    const ADMLobbyGameState* LobbyGameState = GetGameState<ADMLobbyGameState>();

    if (!IsValid(Session) || !IsValid(LobbyGameState))
    {
        UE_LOG(LogDMLobbyGameMode, Error, TEXT("Match roster capture failed: subsystem or GameState missing."));

        ScheduleLobbyStatusRestore();
        return;
    }

    TArray<FUniqueNetIdRepl> ApprovedPlayerIds;
    ApprovedPlayerIds.Reserve(CurrentPlayerCount);

    for (const APlayerState* PlayerState : LobbyGameState->PlayerArray)
    {
        if (!IsValid(PlayerState))
        {
            ScheduleLobbyStatusRestore();
            return;
        }

        const FUniqueNetIdRepl& PlayerId = PlayerState->GetUniqueId();

        if (!PlayerId.IsValid())
        {
            UE_LOG(LogDMLobbyGameMode, Error, TEXT("Match start rejected: invalid player online ID."));

            ScheduleLobbyStatusRestore();
            return;
        }

        ApprovedPlayerIds.Add(PlayerId);
    }

    if (ApprovedPlayerIds.Num() != CurrentPlayerCount || !Session->SetApprovedMatchRoster(ApprovedPlayerIds))
    {
        UE_LOG(LogDMLobbyGameMode, Error, TEXT("Match start rejected: approved roster is invalid."));

        ScheduleLobbyStatusRestore();
        return;
    }

    UE_LOG(
        LogDMLobbyGameMode, Log, TEXT("Steam lobby status updated. Starting travel for %d players."), CurrentPlayerCount
    );

    if (!CommitMatchTravel(CurrentPlayerCount))
    {
        UE_LOG(LogDMLobbyGameMode, Error, TEXT("ServerTravel failed. Restoring lobby status."));

        ScheduleLobbyStatusRestore();
    }
}

void ADMLobbyGameMode::ScheduleLobbyStatusRestore()
{
    if (!bTravelCommitted || bRestoringLobbyStatus)
    {
        return;
    }

    // 매치 시작을 취소하므로 승인 명단도 무효화한다.
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UDMOnlineSessionSubsystem* Session = GameInstance->GetSubsystem<UDMOnlineSessionSubsystem>())
        {
            Session->ClearApprovedMatchRoster();
        }
    }

    bRestoringLobbyStatus = true;

    // 새 복구 시퀀스를 시작하므로 재시도 횟수를 초기화한다.
    LobbyStatusRestoreAttemptCount = 0;

    // 완료 Delegate가 Broadcast 중이므로 복구 요청은 다음 틱에 시작한다.
    GetWorldTimerManager().SetTimerForNextTick(this, &ADMLobbyGameMode::RestoreLobbyStatus);
}

void ADMLobbyGameMode::RestoreLobbyStatus()
{
    if (!bRestoringLobbyStatus || !bTravelCommitted)
    {
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();

    UDMOnlineSessionSubsystem* Session =
        IsValid(GameInstance) ? GameInstance->GetSubsystem<UDMOnlineSessionSubsystem>() : nullptr;

    if (!IsValid(Session) || Session->IsBusy())
    {
        UE_LOG(LogDMLobbyGameMode, Error, TEXT("Lobby status restore cannot be started."));

        // 다른 세션 작업이 끝날 때까지 재시도하고,
        // 한도를 넘기면 로비가 영구 잠기지 않도록 시작 잠금을 해제한다.
        RetryOrReleaseLobbyStatusRestore(TEXT("Lobby status restore cannot be started."));
        return;
    }

    BoundLobbySession = Session;

    LobbyStatusUpdateHandle = Session->OnUpdateCompleted.AddUObject(this, &ADMLobbyGameMode::HandleLobbyStatusUpdated);

    const bool bRequestStarted = Session->UpdateLobbyMatchStarting(false);

    if (!bRequestStarted && bRestoringLobbyStatus)
    {
        UnbindLobbyStatusCompletion();

        UE_LOG(LogDMLobbyGameMode, Error, TEXT("Lobby status restore request failed."));

        // 요청 시작 실패도 재시도 후 한도를 넘기면 시작 잠금을 해제한다.
        RetryOrReleaseLobbyStatusRestore(TEXT("Lobby status restore request failed."));
    }
}

void ADMLobbyGameMode::RetryOrReleaseLobbyStatusRestore(const TCHAR* Reason)
{
    ++LobbyStatusRestoreAttemptCount;

    if (LobbyStatusRestoreAttemptCount < MaxLobbyStatusRestoreAttempts)
    {
        // 복구 상태(bRestoringLobbyStatus/bTravelCommitted)는 유지한 채 1초 후 다시 복구한다.
        bRestoringLobbyStatus = true;

        GetWorldTimerManager().SetTimer(
            LobbyStatusRestoreRetryTimer, this, &ADMLobbyGameMode::RestoreLobbyStatus, 1.0f, false
        );

        UE_LOG(
            LogDMLobbyGameMode,
            Warning,
            TEXT("Scheduled lobby state recovery retry. Attempt=%d/%d, Reason=%s"),
            LobbyStatusRestoreAttemptCount,
            MaxLobbyStatusRestoreAttempts,
            Reason
        );

        return;
    }

    // Steam 광고 상태는 '시작 중'으로 남을 수 있으나,
    // 호스트가 다시 시작하거나 로비를 나갈 수 있도록 로컬 잠금은 해제한다.
    ReleaseMatchStartLock(Reason);
}

void ADMLobbyGameMode::ReleaseMatchStartLock(const TCHAR* Reason)
{
    GetWorldTimerManager().ClearTimer(LobbyStatusRestoreRetryTimer);

    bRestoringLobbyStatus = false;
    bTravelCommitted = false;
    PendingMatchPlayerCount = 0;
    LobbyStatusRestoreAttemptCount = 0;

    UE_LOG(LogDMLobbyGameMode, Log, TEXT("Match start lock released. Reason=%s"), Reason);
}

void ADMLobbyGameMode::HandleReturningPlayersTimeout()
{
    if (!HasAuthority() || ExpectedReturningPlayers <= 0 || bTravelCommitted)
    {
        return;
    }

    // Steam 재개 응답을 기다리는 중이면 잠시 후 다시 확인한다.
    if (bReopeningReturningLobby)
    {
        GetWorldTimerManager().SetTimer(
            ReturningPlayersTimeoutTimer, this, &ADMLobbyGameMode::HandleReturningPlayersTimeout, 5.0f, false
        );

        return;
    }

    const int32 ArrivedPlayerCount = GetNumPlayers();

    UE_LOG(
        LogDMLobbyGameMode,
        Warning,
        TEXT("Timed out waiting for returning players. Expected=%d, Arrived=%d"),
        ExpectedReturningPlayers,
        ArrivedPlayerCount
    );

    // 도착한 인원 기준으로 복귀를 마무리한다(호스트는 항상 포함되므로 최소 1).
    ExpectedReturningPlayers = FMath::Max(1, ArrivedPlayerCount);

    TryReopenReturningLobby();

    // 재개 요청을 시작하지 못했다면(세션 Busy 등) 일정 시간 후 다시 시도한다.
    if (ExpectedReturningPlayers > 0 && !bReopeningReturningLobby)
    {
        GetWorldTimerManager().SetTimer(
            ReturningPlayersTimeoutTimer, this, &ADMLobbyGameMode::HandleReturningPlayersTimeout, 5.0f, false
        );
    }
}

void ADMLobbyGameMode::UnbindLobbyStatusCompletion()
{
    if (UDMOnlineSessionSubsystem* Session = BoundLobbySession.Get())
    {
        if (LobbyStatusUpdateHandle.IsValid())
        {
            Session->OnUpdateCompleted.Remove(LobbyStatusUpdateHandle);
        }
    }

    LobbyStatusUpdateHandle.Reset();
    BoundLobbySession.Reset();
}

#pragma endregion
