
#include "Framework/Lobby/DMLobbyGameMode.h"

#include "Data/DMMatchRuleDataAsset.h"
#include "Framework/Lobby/DMLobbyGameState.h"
#include "Framework/Lobby/DMLobbyPlayerController.h"
#include "Framework/Lobby/DMLobbyPlayerState.h"
#include "Engine/World.h"
#include "GameFramework/GameSession.h"
#include "UObject/SoftObjectPath.h"


DEFINE_LOG_CATEGORY_STATIC(
    LogDMLobbyGameMode,
    Log,
    All
);


ADMLobbyGameMode::ADMLobbyGameMode()
{
    GameStateClass = ADMLobbyGameState::StaticClass();
    PlayerStateClass = ADMLobbyPlayerState::StaticClass();
    PlayerControllerClass = ADMLobbyPlayerController::StaticClass();

    DefaultPawnClass = nullptr;

    // 현재 단계에서는 Lobby와 Match의 Framework 객체를
   // 새 맵에서 명확하게 다시 생성한다.
    bUseSeamlessTravel = false;
}

void ADMLobbyGameMode::InitGame(
    const FString& MapName,
    const FString& Options,
    FString& ErrorMessage
)
{
    Super::InitGame(MapName, Options, ErrorMessage);

    if (!ErrorMessage.IsEmpty())
    {
        return;
    }

    if (!IsValid(MatchRuleData))
    {
        UE_LOG(
            LogDMLobbyGameMode,
            Error,
            TEXT("MatchRuleData is not assigned.")
        );

        return;
    }

    if (!IsValid(GameSession))
    {
        UE_LOG(
            LogDMLobbyGameMode,
            Error,
            TEXT("GameSession was not created.")
        );

        return;
    }

    GameSession->MaxPlayers =
        FMath::Clamp(MatchRuleData->MaxPlayerCount, 2, 4);

    UE_LOG(
        LogDMLobbyGameMode,
        Log,
        TEXT(
            "Lobby capacity initialized. "
            "RuleMaxPlayers=%d, GameSessionMaxPlayers=%d"
        ),
        MatchRuleData->MaxPlayerCount,
        GameSession->MaxPlayers
    );
}


void ADMLobbyGameMode::PreLogin(
    const FString& Options,
    const FString& Address,
    const FUniqueNetIdRepl& UniqueId,
    FString& ErrorMessage
)
{
    Super::PreLogin(
        Options,
        Address,
        UniqueId,
        ErrorMessage
    );

    if (!ErrorMessage.IsEmpty())
    {
        return;
    }

    if (bTravelCommitted)
    {
        ErrorMessage = TEXT("Match is starting.");
        return;
    }

    if (!IsValid(MatchRuleData))
    {
        UE_LOG(
            LogDMLobbyGameMode,
            Error,
            TEXT(
                "MatchRuleData is not assigned. "
                "Maximum player validation was skipped."
            )
        );

        ErrorMessage = TEXT("Lobby configuration is invalid.");
        return;
    }

    UE_LOG(
        LogDMLobbyGameMode,
        Log,
        TEXT(
            "PreLogin capacity check. "
            "CurrentPlayers=%d, RuleMaxPlayers=%d, "
            "GameSessionMaxPlayers=%d"
        ),
        GetNumPlayers(),
        MatchRuleData->MaxPlayerCount,
        IsValid(GameSession)
        ? GameSession->MaxPlayers
        : -1
    );

    if (GetNumPlayers() >= MatchRuleData->MaxPlayerCount)
    {
        UE_LOG(
            LogDMLobbyGameMode,
            Warning,
            TEXT(
                "PreLogin rejected: lobby is full. "
                "CurrentPlayers=%d, MaxPlayers=%d"
            ),
            GetNumPlayers(),
            MatchRuleData->MaxPlayerCount
        );

        ErrorMessage = TEXT("Lobby is full.");
    }
}

void ADMLobbyGameMode::PostLogin(
    APlayerController* NewPlayer
)
{
    Super::PostLogin(NewPlayer);

    ADMLobbyPlayerController* LobbyPlayerController =
        Cast<ADMLobbyPlayerController>(NewPlayer);

    if (LobbyPlayerController == nullptr)
    {
        UE_LOG(
            LogDMLobbyGameMode,
            Error,
            TEXT(
                "PostLogin received an invalid "
                "LobbyPlayerController."
            )
        );

        return;
    }

    ADMLobbyPlayerState* LobbyPlayerState =
        LobbyPlayerController
        ->GetPlayerState<ADMLobbyPlayerState>();

    if (LobbyPlayerState == nullptr)
    {
        UE_LOG(
            LogDMLobbyGameMode,
            Error,
            TEXT(
                "LobbyPlayerState was not created "
                "for the joining player."
            )
        );

        return;
    }

    AssignHostIfNeeded(LobbyPlayerController);

    UE_LOG(
        LogDMLobbyGameMode,
        Log,
        TEXT(
            "Lobby player joined. "
            "PlayerName=%s, ConnectedPlayers=%d"
        ),
        *LobbyPlayerState->GetPlayerName(),
        GetNumPlayers()
    );
}

void ADMLobbyGameMode::HandleReadyRequest(
    ADMLobbyPlayerController* RequestingController,
    const bool bNewReady
)
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

    ADMLobbyPlayerState* LobbyPlayerState =
        RequestingController
        ->GetPlayerState<ADMLobbyPlayerState>();

    if (LobbyPlayerState == nullptr)
    {
        return;
    }

    const bool bPreviousReady =
        LobbyPlayerState->IsReady();

    LobbyPlayerState->SetReadyState(bNewReady);

    UE_LOG(
        LogDMLobbyGameMode,
        Log,
        TEXT(
            "Ready request processed. "
            "PlayerName=%s, Requested=%s, "
            "Previous=%s, Current=%s"
        ),
        *LobbyPlayerState->GetPlayerName(),
        bNewReady ? TEXT("true") : TEXT("false"),
        bPreviousReady ? TEXT("true") : TEXT("false"),
        LobbyPlayerState->IsReady()
        ? TEXT("true")
        : TEXT("false")
    );

}

void ADMLobbyGameMode::HandleStartMatchRequest(
    ADMLobbyPlayerController* RequestingController
)
{
    const ADMLobbyPlayerState* RequestingPlayerState =
        IsValid(RequestingController)
        ? RequestingController
        ->GetPlayerState<ADMLobbyPlayerState>()
        : nullptr;

    UE_LOG(
        LogDMLobbyGameMode,
        Log,
        TEXT(
            "Start request received. "
            "PlayerName=%s, LocalController=%s, Host=%s"
        ),
        RequestingPlayerState != nullptr
        ? *RequestingPlayerState->GetPlayerName()
        : TEXT("Invalid"),
        IsValid(RequestingController)
        && RequestingController->IsLocalController()
        ? TEXT("true")
        : TEXT("false"),
        RequestingPlayerState != nullptr
        && RequestingPlayerState->IsHost()
        ? TEXT("true")
        : TEXT("false")
    );


    if (bTravelCommitted)
    {
        UE_LOG(
            LogDMLobbyGameMode,
            Warning,
            TEXT(
                "Start request ignored: "
                "travel is already committed."
            )
        );

        return;
    }

    if (!IsHostController(RequestingController))
    {
        UE_LOG(
            LogDMLobbyGameMode,
            Warning,
            TEXT(
                "Start request rejected: "
                "requester is not the listen-server host."
            )
        );

        return;
    }

    FString FailureReason;
    int32 ConnectedPlayerCount = 0;

    if (!CanStartMatch(
        FailureReason,
        ConnectedPlayerCount
    ))
    {
        UE_LOG(
            LogDMLobbyGameMode,
            Warning,
            TEXT(
                "Start request rejected: %s"
            ),
            *FailureReason
        );

        return;
    }

    if (!CommitMatchTravel(ConnectedPlayerCount))
    {
        UE_LOG(
            LogDMLobbyGameMode,
            Error,
            TEXT(
                "ServerTravel could not be started."
            )
        );
    }
}

void ADMLobbyGameMode::AssignHostIfNeeded(
    ADMLobbyPlayerController* NewPlayerController
)
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
        NewPlayerController->IsLocalController()
        && (
            NetMode == NM_ListenServer
            || NetMode == NM_Standalone
            );

    if (!bIsSupportedLocalHost)
    {
        return;
    }

    ADMLobbyPlayerState* LobbyPlayerState =
        NewPlayerController
        ->GetPlayerState<ADMLobbyPlayerState>();

    if (LobbyPlayerState == nullptr)
    {
        return;
    }

    LobbyPlayerState->SetHostState(true);

    UE_LOG(
        LogDMLobbyGameMode,
        Log,
        TEXT(
            "Lobby host assigned. PlayerName=%s"
        ),
        *LobbyPlayerState->GetPlayerName()
    );
}

bool ADMLobbyGameMode::HasAssignedHost()
{
    const ADMLobbyGameState* LobbyGameState =
        GetGameState<ADMLobbyGameState>();

    if (LobbyGameState == nullptr)
    {
        return false;
    }

    for (
        APlayerState* PlayerState
        : LobbyGameState->PlayerArray
        )
    {
        const ADMLobbyPlayerState* LobbyPlayerState =
            Cast<ADMLobbyPlayerState>(PlayerState);

        if (
            LobbyPlayerState != nullptr
            && LobbyPlayerState->IsHost()
            )
        {
            return true;
        }
    }

    return false;
}

bool ADMLobbyGameMode::IsHostController(
    ADMLobbyPlayerController* RequestingController
)
{
    if (!IsValid(RequestingController))
    {
        return false;
    }

    if (RequestingController->GetWorld() != GetWorld())
    {
        return false;
    }

    const ADMLobbyPlayerState* LobbyPlayerState =
        RequestingController
        ->GetPlayerState<ADMLobbyPlayerState>();

    if (LobbyPlayerState == nullptr)
    {
        return false;
    }

    return RequestingController->IsLocalController()
        && LobbyPlayerState->IsHost();
}

bool ADMLobbyGameMode::CanStartMatch(
    FString& OutFailureReason,
    int32& OutConnectedPlayerCount
)
{
    OutFailureReason.Reset();
    OutConnectedPlayerCount = 0;

    if (!IsValid(MatchRuleData))
    {
        OutFailureReason =
            TEXT("MatchRuleData is not assigned.");

        return false;
    }

    const int32 MinPlayerCount =
        MatchRuleData->MinPlayerCount;

    const int32 MaxPlayerCount =
        MatchRuleData->MaxPlayerCount;

    if (
        MinPlayerCount < 2
        || MaxPlayerCount > 4
        || MinPlayerCount > MaxPlayerCount
        )
    {
        OutFailureReason =
            TEXT("Match player rule values are invalid.");

        return false;
    }

    if (MatchMap.IsNull())
    {
        OutFailureReason =
            TEXT("MatchMap is not assigned.");

        return false;
    }

    const ADMLobbyGameState* LobbyGameState =
        GetGameState<ADMLobbyGameState>();

    if (LobbyGameState == nullptr)
    {
        OutFailureReason =
            TEXT("LobbyGameState is not available.");

        return false;
    }

    int32 HostCount = 0;
    FString FirstNotReadyPlayerName;

    for (
        APlayerState* PlayerState
        : LobbyGameState->PlayerArray
        )
    {
        const ADMLobbyPlayerState* LobbyPlayerState =
            Cast<ADMLobbyPlayerState>(PlayerState);

        /*
         * 반드시 LobbyPlayerState를 사용하기 전에
         * 유효성을 검사해야 한다.
         */
        if (LobbyPlayerState == nullptr)
        {
            OutFailureReason =
                TEXT(
                    "Lobby PlayerArray contains "
                    "an invalid PlayerState class."
                );

            return false;
        }

        UE_LOG(
            LogDMLobbyGameMode,
            Log,
            TEXT(
                "Lobby player state check. "
                "PlayerName=%s, Ready=%s, Host=%s"
            ),
            *LobbyPlayerState->GetPlayerName(),
            LobbyPlayerState->IsReady()
            ? TEXT("true")
            : TEXT("false"),
            LobbyPlayerState->IsHost()
            ? TEXT("true")
            : TEXT("false")
        );

        ++OutConnectedPlayerCount;

        if (LobbyPlayerState->IsHost())
        {
            ++HostCount;
        }

        if (
            !LobbyPlayerState->IsReady()
            && FirstNotReadyPlayerName.IsEmpty()
            )
        {
            FirstNotReadyPlayerName =
                LobbyPlayerState->GetPlayerName();
        }
    }

    /*
     * PlayerArray와 GameMode가 인식하는 실제 Player 수가
     * 일치하지 않으면 아직 동기화가 완료되지 않은 상태다.
     */
    if (
        OutConnectedPlayerCount
        != GetNumPlayers()
        )
    {
        OutFailureReason =
            TEXT(
                "PlayerState synchronization "
                "is not complete."
            );

        return false;
    }

    if (
        OutConnectedPlayerCount < MinPlayerCount
        || OutConnectedPlayerCount > MaxPlayerCount
        )
    {
        OutFailureReason = FString::Printf(
            TEXT(
                "Player count must be between %d and %d. "
                "Current=%d"
            ),
            MinPlayerCount,
            MaxPlayerCount,
            OutConnectedPlayerCount
        );

        return false;
    }

    if (HostCount != 1)
    {
        OutFailureReason = FString::Printf(
            TEXT(
                "Lobby must have exactly one host. "
                "CurrentHostCount=%d"
            ),
            HostCount
        );

        return false;
    }

    if (!FirstNotReadyPlayerName.IsEmpty())
    {
        OutFailureReason = FString::Printf(
            TEXT(
                "Not every player is ready. "
                "FirstNotReadyPlayer=%s"
            ),
            *FirstNotReadyPlayerName
        );

        return false;
    }

    return true;
}


bool ADMLobbyGameMode::CommitMatchTravel(
    const int32 ExpectedPlayerCount
)
{
    if (bTravelCommitted)
    {
        return false;
    }

    UWorld* World = GetWorld();

    if (World == nullptr || MatchMap.IsNull())
    {
        return false;
    }

    const FString MatchMapPackageName =
        MatchMap
        .ToSoftObjectPath()
        .GetLongPackageName();

    if (MatchMapPackageName.IsEmpty())
    {
        return false;
    }

    const FString TravelURL = FString::Printf(
        TEXT(
            "%s?listen?ExpectedPlayers=%d"
        ),
        *MatchMapPackageName,
        ExpectedPlayerCount
    );

    bTravelCommitted = true;

    UE_LOG(
        LogDMLobbyGameMode,
        Log,
        TEXT(
            "Match travel committed. URL=%s"
        ),
        *TravelURL
    );

    const bool bTravelStarted =
        World->ServerTravel(
            TravelURL,
            false,
            false
        );

    if (!bTravelStarted)
    {
        bTravelCommitted = false;
    }

    return bTravelStarted;
}

