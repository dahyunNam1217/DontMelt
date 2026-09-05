


#include "Framework/Match/DMMatchGameMode.h"

#include "Characters/DMPlayerCharacter.h"
#include "Data/DMMatchRuleDataAsset.h"
#include "Framework/Match/DMMatchGameState.h"
#include "Framework/Match/DMMatchPlayerController.h"
#include "Framework/Match/DMMatchPlayerState.h"
#include "EngineUtils.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Controller.h"

DEFINE_LOG_CATEGORY_STATIC(
    LogDMMatchGameMode,
    Log,
    All
);

ADMMatchGameMode::ADMMatchGameMode()
{
    GameStateClass = ADMMatchGameState::StaticClass();
    PlayerStateClass = ADMMatchPlayerState::StaticClass();
    PlayerControllerClass =
        ADMMatchPlayerController::StaticClass();

    // C++ fallback.
    // BP_DMMatchGameMode에서 BP_DMPlayerCharacter로 덮어쓴다.
    DefaultPawnClass = ADMPlayerCharacter::StaticClass();
}

void ADMMatchGameMode::InitGame(
    const FString& MapName,
    const FString& Options,
    FString& ErrorMessage
)
{
    Super::InitGame(
        MapName,
        Options,
        ErrorMessage
    );

    if (!ErrorMessage.IsEmpty())
    {
        return;
    }

    if (!IsValid(MatchRuleData))
    {
        ErrorMessage =
            TEXT("MatchRuleData is not assigned.");

        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT("%s"),
            *ErrorMessage
        );

        return;
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
        ErrorMessage =
            TEXT("Match player rules are invalid.");

        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "%s Min=%d, Max=%d"
            ),
            *ErrorMessage,
            MinPlayerCount,
            MaxPlayerCount
        );

        return;
    }

    const FString ExpectedPlayersOption =
        UGameplayStatics::ParseOption(
            Options,
            TEXT("ExpectedPlayers")
        );

    const int32 ParsedExpectedPlayerCount =
        FCString::Atoi(*ExpectedPlayersOption);

    if (
        !FMath::IsWithinInclusive(
            ParsedExpectedPlayerCount,
            MinPlayerCount,
            MaxPlayerCount
        )
        )
    {
        ErrorMessage = FString::Printf(
            TEXT(
                "ExpectedPlayers option is invalid. "
                "Received=%s"
            ),
            *ExpectedPlayersOption
        );

        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT("%s"),
            *ErrorMessage
        );

        return;
    }

    ExpectedPlayerCount =
        ParsedExpectedPlayerCount;

    UE_LOG(
        LogDMMatchGameMode,
        Log,
        TEXT(
            "Match initialized. "
            "ExpectedPlayers=%d"
        ),
        ExpectedPlayerCount
    );
}

void ADMMatchGameMode::PreLogin(
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

    if (
        !IsValid(MatchRuleData)
        || ExpectedPlayerCount <= 0
        )
    {
        ErrorMessage =
            TEXT("Match configuration is invalid.");

        return;
    }

    if (IsStartingRosterLocked())
    {
        ErrorMessage =
            TEXT("Match roster is locked.");

        UE_LOG(
            LogDMMatchGameMode,
            Warning,
            TEXT(
                "Match login rejected: "
                "starting roster is locked."
            )
        );

        return;
    }

    if (GetNumPlayers() >= ExpectedPlayerCount)
    {
        ErrorMessage =
            TEXT("Match has reached its expected player count.");

        UE_LOG(
            LogDMMatchGameMode,
            Warning,
            TEXT(
                "Match login rejected. "
                "CurrentPlayers=%d, ExpectedPlayers=%d"
            ),
            GetNumPlayers(),
            ExpectedPlayerCount
        );
    }
}

bool ADMMatchGameMode::UpdatePlayerStartSpot(
    AController* Player,
    const FString& Portal,
    FString& OutErrorMessage
)
{
    if (!IsValid(Player))
    {
        OutErrorMessage =
            TEXT("Player controller is invalid.");

        return false;
    }

    ADMMatchPlayerState* MatchPlayerState =
        Player->GetPlayerState<ADMMatchPlayerState>();

    if (!IsValid(MatchPlayerState))
    {
        OutErrorMessage =
            TEXT("MatchPlayerState is unavailable.");

        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "UpdatePlayerStartSpot failed: "
                "MatchPlayerState is unavailable."
            )
        );

        return false;
    }

    MatchPlayerState->SetLifeState(
        EDMPlayerLifeState::Respawning
    );

    /*
     * 반드시 부모 UpdatePlayerStartSpot보다 먼저 실행한다.
     *
     * 부모 함수 내부의 FindPlayerStart →
     * ChoosePlayerStart 흐름에서 BaseSlot이 필요하다.
     */
    if (!TryAssignBaseSlot(MatchPlayerState))
    {
        OutErrorMessage =
            TEXT("No valid BaseSlot is available.");

        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "UpdatePlayerStartSpot failed: "
                "BaseSlot assignment failed. "
                "PlayerName=%s"
            ),
            *MatchPlayerState->GetPlayerName()
        );

        return false;
    }

    UE_LOG(
        LogDMMatchGameMode,
        Log,
        TEXT(
            "BaseSlot assigned before PlayerStart selection. "
            "PlayerName=%s, BaseSlot=%d"
        ),
        *MatchPlayerState->GetPlayerName(),
        MatchPlayerState->GetBaseSlot()
    );

    const bool bStartSpotUpdated =
        Super::UpdatePlayerStartSpot(
            Player,
            Portal,
            OutErrorMessage
        );

    if (!bStartSpotUpdated)
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "Engine PlayerStart initialization failed. "
                "PlayerName=%s, BaseSlot=%d, Reason=%s"
            ),
            *MatchPlayerState->GetPlayerName(),
            MatchPlayerState->GetBaseSlot(),
            *OutErrorMessage
        );

        /*
         * 로그인 초기화가 실패했다면 해당 슬롯을 다시 비운다.
         */
        MatchPlayerState->SetBaseSlot(INDEX_NONE);

        return false;
    }

    return true;
}

void ADMMatchGameMode::PostLogin(
    APlayerController* NewPlayer
)
{
    
    ADMMatchPlayerController* MatchPlayerController =
        Cast<ADMMatchPlayerController>(NewPlayer);

    if (MatchPlayerController == nullptr)
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "PostLogin received an invalid "
                "MatchPlayerController."
            )
        );

        return;
    }

    /*
     * Super::PostLogin을 호출하면 엔진의
     * HandleStartingNewPlayer → Pawn Spawn 흐름으로 이어질 수 있다.
     *
     * 따라서 스폰에 필요한 참가자 검증과 BaseSlot 할당을
     * 반드시 Super 호출보다 먼저 끝낸다.
     */


    /*
     * PreLogin과 실제 Login 사이의 상태가 변했을 경우를 위한
     * 서버 측 최종 방어 검사다.
     */
    if (
        IsStartingRosterLocked()
        || GetNumPlayers() > ExpectedPlayerCount
        )
    {
        UE_LOG(
            LogDMMatchGameMode,
            Warning,
            TEXT(
                "PostLogin rejected a late player. "
                "CurrentPlayers=%d, ExpectedPlayers=%d"
            ),
            GetNumPlayers(),
            ExpectedPlayerCount
        );

        if (IsValid(GameSession))
        {
            GameSession->KickPlayer(
                NewPlayer,
                FText::FromString(
                    TEXT("Match roster is locked.")
                )
            );
        }

        return;
    }

    ADMMatchPlayerState* MatchPlayerState =
        MatchPlayerController
        ->GetPlayerState<ADMMatchPlayerState>();

    if (MatchPlayerState == nullptr)
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "MatchPlayerState was not created "
                "for the joining player."
            )
        );

        return;
    }

    /*
 * BaseSlot은 UpdatePlayerStartSpot에서
 * 이미 확정되어 있어야 한다.
 */
    if (!MatchPlayerState->HasAssignedBaseSlot())
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "PostLogin rejected: "
                "BaseSlot was not assigned before login completion. "
                "PlayerName=%s"
            ),
            *MatchPlayerState->GetPlayerName()
        );

        if (IsValid(GameSession))
        {
            GameSession->KickPlayer(
                NewPlayer,
                FText::FromString(
                    TEXT("BaseSlot initialization failed.")
                )
            );
        }

        return;
    }

    UE_LOG(
        LogDMMatchGameMode,
        Log,
        TEXT(
            "Match player prepared for spawn. "
            "PlayerName=%s, BaseSlot=%d, "
            "Connected=%d/%d"
        ),
        *MatchPlayerState->GetPlayerName(),
        MatchPlayerState->GetBaseSlot(),
        GetNumPlayers(),
        ExpectedPlayerCount
    );

    /*
     * 이미 StartSpot과 BaseSlot이 정상적으로 설정된 상태에서
     * 기본 PostLogin → HandleStartingNewPlayer 흐름을 실행한다.
     */
    Super::PostLogin(NewPlayer);

}

void ADMMatchGameMode::Logout(
    AController* Exiting
)
{
    /*
     * Super::Logout 이후에는 PlayerState가 제거될 수 있으므로
     * 필요한 정보를 먼저 저장한다.
     */
    ADMMatchPlayerState* ExitingPlayerState =
        Exiting != nullptr
        ? Exiting
        ->GetPlayerState<ADMMatchPlayerState>()
        : nullptr;

    const bool bHasMatchPlayerState =
        IsValid(ExitingPlayerState);

    const bool bWasAdmittedParticipant =
        bHasMatchPlayerState
        && ExitingPlayerState->HasAssignedBaseSlot();

    const bool bIsListenServerHost =
        Exiting != nullptr
        && Exiting->IsLocalController();

    const FString ExitingPlayerName =
        bHasMatchPlayerState
        && !ExitingPlayerState
        ->GetPlayerName()
        .IsEmpty()
        ? ExitingPlayerState->GetPlayerName()
        : GetNameSafe(Exiting);

    const int32 ExitingBaseSlot =
        bHasMatchPlayerState
        ? ExitingPlayerState->GetBaseSlot()
        : INDEX_NONE;

    ADMMatchGameState* MatchGameState =
        GetGameState<ADMMatchGameState>();

    const EDMMatchPhase PhaseBeforeLogout =
        IsValid(MatchGameState)
        ? MatchGameState->GetCurrentPhase()
        : EDMMatchPhase::Waiting;

    const EDMMatchEndReason ReasonBeforeLogout =
        IsValid(MatchGameState)
        ? MatchGameState->GetMatchEndReason()
        : EDMMatchEndReason::None;

    /*
     * PlayerDisconnect로 Match를 중단해야 하는 조건:
     *
     * 1. Listen Server Host가 아님
     * 2. BaseSlot을 할당받은 실제 참가자임
     * 3. MatchGameState가 유효함
     * 4. 아직 결과가 확정되지 않음
     * 5. 현재 Phase에서 Aborted 전환이 허용됨
     */
    const bool bShouldAbortForDisconnect =
        !bIsListenServerHost
        && bWasAdmittedParticipant
        && IsValid(MatchGameState)
        && ReasonBeforeLogout
        == EDMMatchEndReason::None
        && IsPhaseTransitionAllowed(
            PhaseBeforeLogout,
            EDMMatchPhase::Aborted
        );

    if (bShouldAbortForDisconnect)
    {
        /*
         * 이 값은 Logout 정리 전의 서버 생명주기 상태다.
         * 최종 공개 결과는 MatchEndReason이 담당한다.
         */
        ExitingPlayerState->SetLifeState(
            EDMPlayerLifeState::Disconnected
        );

        UE_LOG(
            LogDMMatchGameMode,
            Log,
            TEXT(
                "Remote participant logout detected. "
                "PlayerName=%s, BaseSlot=%d, Phase=%s"
            ),
            *ExitingPlayerName,
            ExitingBaseSlot,
            *UEnum::GetValueAsString(
                PhaseBeforeLogout
            )
        );
    }
    else if (bIsListenServerHost)
    {
        UE_LOG(
            LogDMMatchGameMode,
            Log,
            TEXT(
                "Listen Server host logout detected. "
                "Remote clients recover through "
                "UDMGameInstance. "
                "PlayerName=%s"
            ),
            *ExitingPlayerName
        );
    }
    else if (!bWasAdmittedParticipant)
    {
        UE_LOG(
            LogDMMatchGameMode,
            Log,
            TEXT(
                "Non-admitted Controller logout ignored. "
                "Controller=%s"
            ),
            *GetNameSafe(Exiting)
        );
    }
    else
    {
        /*
         * Finished 또는 Aborted 이후의 Logout 등.
         * 기존 Match 결과를 변경하지 않는다.
         */
        UE_LOG(
            LogDMMatchGameMode,
            Log,
            TEXT(
                "Participant logout did not alter "
                "the locked Match result. "
                "PlayerName=%s, Phase=%s, Reason=%s"
            ),
            *ExitingPlayerName,
            *UEnum::GetValueAsString(
                PhaseBeforeLogout
            ),
            *UEnum::GetValueAsString(
                ReasonBeforeLogout
            )
        );
    }

    /*
     * PlayerController, PlayerState, PlayerArray 등
     * 엔진 소유 생명주기 정리는 부모에게 맡긴다.
     */
    Super::Logout(Exiting);

    /*
     * PlayerState가 PlayerArray에서 제거된 뒤
     * Match 중단 결과를 확정한다.
     */
    if (bShouldAbortForDisconnect)
    {
        const bool bAbortSucceeded =
            TryAbortMatch(
                EDMMatchEndReason::PlayerDisconnect
            );

        if (!bAbortSucceeded)
        {
            UE_LOG(
                LogDMMatchGameMode,
                Error,
                TEXT(
                    "Remote participant logout was "
                    "detected, but Match abort failed. "
                    "PlayerName=%s, BaseSlot=%d"
                ),
                *ExitingPlayerName,
                ExitingBaseSlot
            );

            return;
        }

        int32 ConnectedAfterLogout = 0;

        if (UWorld* World = GetWorld())
        {
            for (
                FConstPlayerControllerIterator It =
                World->GetPlayerControllerIterator();
                It;
                ++It
                )
            {
                APlayerController* PlayerController =
                    It->Get();

                /*
                 * Logout 처리 중인 Controller는
                 * Iterator에 아직 남아 있을 수 있으므로 제외한다.
                 */
                if (
                    !IsValid(PlayerController)
                    || PlayerController == Exiting
                    )
                {
                    continue;
                }

                const ADMMatchPlayerState* PlayerState =
                    PlayerController
                    ->GetPlayerState<
                    ADMMatchPlayerState
                    >();

                /*
                 * BaseSlot을 배정받은 실제 Match 참가자만
                 * 현재 연결 참가자로 계산한다.
                 */
                if (
                    IsValid(PlayerState)
                    && PlayerState
                    ->HasAssignedBaseSlot()
                    )
                {
                    ++ConnectedAfterLogout;
                }
            }
        }

        UE_LOG(
            LogDMMatchGameMode,
            Log,
            TEXT(
                "Match aborted after remote "
                "participant logout. "
                "PlayerName=%s, BaseSlot=%d, "
                "ConnectedAfterLogout=%d"
            ),
            *ExitingPlayerName,
            ExitingBaseSlot,
            ConnectedAfterLogout
        );
    }
}


void ADMMatchGameMode::
HandleStartingNewPlayer_Implementation(
    APlayerController* NewPlayer
)
{
    if (!IsValid(NewPlayer))
    {
        return;
    }

    ADMMatchPlayerState* MatchPlayerState =
        NewPlayer->GetPlayerState<ADMMatchPlayerState>();

    if (
        MatchPlayerState == nullptr
        || !MatchPlayerState->HasAssignedBaseSlot()
        )
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "Initial spawn rejected: "
                "valid MatchPlayerState or BaseSlot is missing."
            )
        );

        return;
    }

    /*
     * 엔진 기본 흐름:
     * FindPlayerStart → ChoosePlayerStart
     * → Pawn Spawn → Possess
     */
    Super::HandleStartingNewPlayer_Implementation(
        NewPlayer
    );

    ADMPlayerCharacter* PlayerCharacter =
        NewPlayer->GetPawn<ADMPlayerCharacter>();

    const bool bSpawnSucceeded =
        IsValid(PlayerCharacter)
        && PlayerCharacter->GetController() == NewPlayer;

    if (!bSpawnSucceeded)
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "Initial spawn validation failed. "
                "PlayerName=%s, BaseSlot=%d"
            ),
            *MatchPlayerState->GetPlayerName(),
            MatchPlayerState->GetBaseSlot()
        );

        return;
    }

    MatchPlayerState->SetLifeState(
        EDMPlayerLifeState::Alive
    );

    UE_LOG(
        LogDMMatchGameMode,
        Log,
        TEXT(
            "Initial spawn validated. "
            "PlayerName=%s, BaseSlot=%d, "
            "LifeState=Alive"
        ),
        *MatchPlayerState->GetPlayerName(),
        MatchPlayerState->GetBaseSlot()
    );

    TryFinalizeStartingRoster();
}

AActor* ADMMatchGameMode::
ChoosePlayerStart_Implementation(
    AController* Player
)
{
    if (!IsValid(Player))
    {
        return nullptr;
    }

    const ADMMatchPlayerState* MatchPlayerState =
        Player->GetPlayerState<ADMMatchPlayerState>();

    if (
        MatchPlayerState == nullptr
        || !MatchPlayerState->HasAssignedBaseSlot()
        )
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "ChoosePlayerStart failed: "
                "BaseSlot is not assigned."
            )
        );

        return nullptr;
    }

    return FindPlayerStartForBaseSlot(
        MatchPlayerState->GetBaseSlot()
    );
}

bool ADMMatchGameMode::
IsStartingRosterLocked() const
{
    if (StartingPlayerCountSnapshot != 0)
    {
        return true;
    }

    const ADMMatchGameState* MatchGameState =
        GetGameState<ADMMatchGameState>();

    return IsValid(MatchGameState)
        && MatchGameState->GetCurrentPhase()
        != EDMMatchPhase::Waiting;
}

bool ADMMatchGameMode::TryAssignBaseSlot(
    ADMMatchPlayerState* MatchPlayerState
)
{
    if (!IsValid(MatchPlayerState))
    {
        return false;
    }

    if (MatchPlayerState->HasAssignedBaseSlot())
    {
        return true;
    }

    const int32 AvailableBaseSlot =
        FindFirstAvailableBaseSlot();

    if (AvailableBaseSlot == INDEX_NONE)
    {
        return false;
    }

    MatchPlayerState->SetBaseSlot(
        AvailableBaseSlot
    );

    return MatchPlayerState->GetBaseSlot()
        == AvailableBaseSlot;
}

int32 ADMMatchGameMode::
FindFirstAvailableBaseSlot() const
{
    if (!IsValid(MatchRuleData))
    {
        return INDEX_NONE;
    }

    const ADMMatchGameState* MatchGameState =
        GetGameState<ADMMatchGameState>();

    if (MatchGameState == nullptr)
    {
        return INDEX_NONE;
    }

    for (
        int32 CandidateSlot = 0;
        CandidateSlot < MatchRuleData->MaxPlayerCount;
        ++CandidateSlot
        )
    {
        bool bSlotAlreadyUsed = false;

        for (
            APlayerState* PlayerState
            : MatchGameState->PlayerArray
            )
        {
            const ADMMatchPlayerState*
                MatchPlayerState =
                Cast<ADMMatchPlayerState>(
                    PlayerState
                );

            if (
                MatchPlayerState != nullptr
                && MatchPlayerState->GetBaseSlot()
                == CandidateSlot
                )
            {
                bSlotAlreadyUsed = true;
                break;
            }
        }

        if (!bSlotAlreadyUsed)
        {
            return CandidateSlot;
        }
    }

    return INDEX_NONE;
}

APlayerStart*
ADMMatchGameMode::FindPlayerStartForBaseSlot(
    const int32 BaseSlot
) const
{
    if (
        !FMath::IsWithinInclusive(
            BaseSlot,
            0,
            3
        )
        )
    {
        return nullptr;
    }

    UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return nullptr;
    }

    const FName RequiredPlayerStartTag(
        *FString::Printf(
            TEXT("DM_Base_%d"),
            BaseSlot
        )
    );

    APlayerStart* FoundPlayerStart = nullptr;

    for (
        TActorIterator<APlayerStart> Iterator(World);
        Iterator;
        ++Iterator
        )
    {
        APlayerStart* PlayerStart = *Iterator;

        if (
            PlayerStart->PlayerStartTag
            != RequiredPlayerStartTag
            )
        {
            continue;
        }

        if (FoundPlayerStart != nullptr)
        {
            UE_LOG(
                LogDMMatchGameMode,
                Error,
                TEXT(
                    "Duplicate PlayerStartTag found. "
                    "Tag=%s"
                ),
                *RequiredPlayerStartTag.ToString()
            );

            return nullptr;
        }

        FoundPlayerStart = PlayerStart;
    }

    if (FoundPlayerStart == nullptr)
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "Required PlayerStart was not found. "
                "Tag=%s"
            ),
            *RequiredPlayerStartTag.ToString()
        );
    }

    return FoundPlayerStart;
}

bool ADMMatchGameMode::
IsStartingRosterComplete(
    FString& OutFailureReason
) const
{
    OutFailureReason.Reset();

    if (ExpectedPlayerCount <= 0)
    {
        OutFailureReason =
            TEXT("ExpectedPlayerCount is invalid.");

        return false;
    }

    const ADMMatchGameState* MatchGameState =
        GetGameState<ADMMatchGameState>();

    if (MatchGameState == nullptr)
    {
        OutFailureReason =
            TEXT("MatchGameState is unavailable.");

        return false;
    }

    if (
        MatchGameState->PlayerArray.Num()
        != ExpectedPlayerCount
        )
    {
        OutFailureReason = FString::Printf(
            TEXT(
                "Waiting for players. "
                "Current=%d, Expected=%d"
            ),
            MatchGameState->PlayerArray.Num(),
            ExpectedPlayerCount
        );

        return false;
    }

    TSet<int32> AssignedBaseSlots;

    for (
        APlayerState* PlayerState
        : MatchGameState->PlayerArray
        )
    {
        const ADMMatchPlayerState*
            MatchPlayerState =
            Cast<ADMMatchPlayerState>(
                PlayerState
            );

        if (MatchPlayerState == nullptr)
        {
            OutFailureReason =
                TEXT("Invalid MatchPlayerState.");

            return false;
        }

        const int32 BaseSlot =
            MatchPlayerState->GetBaseSlot();

        if (
            !FMath::IsWithinInclusive(
                BaseSlot,
                0,
                3
            )
            )
        {
            OutFailureReason =
                TEXT("A player has no valid BaseSlot.");

            return false;
        }

        if (AssignedBaseSlots.Contains(BaseSlot))
        {
            OutFailureReason =
                TEXT("Duplicate BaseSlot detected.");

            return false;
        }

        AssignedBaseSlots.Add(BaseSlot);

        if (
            MatchPlayerState->GetLifeState()
            != EDMPlayerLifeState::Alive
            )
        {
            OutFailureReason = FString::Printf(
                TEXT(
                    "Player is not Alive. "
                    "PlayerName=%s"
                ),
                *MatchPlayerState->GetPlayerName()
            );

            return false;
        }

        const ADMPlayerCharacter* PlayerCharacter =
            MatchPlayerState
            ->GetPawn<ADMPlayerCharacter>();

        if (
            !IsValid(PlayerCharacter)
            || PlayerCharacter->GetController()
            == nullptr
            || PlayerCharacter
            ->GetPlayerState<ADMMatchPlayerState>()
            != MatchPlayerState
            )
        {
            OutFailureReason = FString::Printf(
                TEXT(
                    "Pawn or Possess validation failed. "
                    "PlayerName=%s"
                ),
                *MatchPlayerState->GetPlayerName()
            );

            return false;
        }
    }

    return true;
}

void ADMMatchGameMode::
TryFinalizeStartingRoster()
{
    if (StartingPlayerCountSnapshot != 0)
    {
        return;
    }

    FString FailureReason;

    if (!IsStartingRosterComplete(FailureReason))
    {
        UE_LOG(
            LogDMMatchGameMode,
            Verbose,
            TEXT(
                "Starting roster is not complete: %s"
            ),
            *FailureReason
        );

        return;
    }

    ADMMatchGameState* MatchGameState =
        GetGameState<ADMMatchGameState>();

    if (MatchGameState == nullptr)
    {
        return;
    }

    StartingPlayerCountSnapshot =
        MatchGameState->PlayerArray.Num();

    MatchGameState->SetStartingPlayerCount(
        StartingPlayerCountSnapshot
    );

    if (
        MatchGameState->GetStartingPlayerCount()
        != StartingPlayerCountSnapshot
        )
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "StartingPlayerCount projection failed. "
                "Snapshot=%d, GameState=%d"
            ),
            StartingPlayerCountSnapshot,
            MatchGameState
            ->GetStartingPlayerCount()
        );

        StartingPlayerCountSnapshot = 0;
        return;
    }

    UE_LOG(
        LogDMMatchGameMode,
        Log,
        TEXT(
            "Starting roster frozen. "
            "StartingPlayerCount=%d"
        ),
        StartingPlayerCountSnapshot
    );

    StartCountdown();

}

void ADMMatchGameMode::StartCountdown()
{
    ADMMatchGameState* MatchGameState =
        GetGameState<ADMMatchGameState>();

    if (!IsValid(MatchGameState))
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "StartCountdown failed: "
                "MatchGameState is unavailable."
            )
        );

        return;
    }

    if (!IsValid(MatchRuleData))
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "StartCountdown failed: "
                "MatchRuleData is unavailable."
            )
        );

        return;
    }

    /*
    * 실제 Phase 변경은 TryTransitionPhase()가 다시 검사하지만,
    * StartCountdown 자체의 호출 조건을 명확히 하기 위해
    * 여기서도 Waiting 여부를 먼저 검사한다.
    */

    if (
        MatchGameState->GetCurrentPhase()
        != EDMMatchPhase::Waiting
        )
    {
        UE_LOG(
            LogDMMatchGameMode,
            Warning,
            TEXT(
                "StartCountdown rejected: "
                "current phase is not Waiting."
            )
        );

        return;
    }

    if (
        GetWorldTimerManager()
        .IsTimerActive(PhaseTimerHandle)
        )
    {
        UE_LOG(
            LogDMMatchGameMode,
            Warning,
            TEXT(
                "StartCountdown rejected: "
                "PhaseTimer is already active."
            )
        );

        return;
    }

    if (
        !FMath::IsWithinInclusive(
            StartingPlayerCountSnapshot,
            2,
            4
        )
        )
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "StartCountdown failed: "
                "StartingPlayerCountSnapshot is invalid. "
                "Snapshot=%d"
            ),
            StartingPlayerCountSnapshot
        );

        return;
    }

    if (
        MatchGameState->GetStartingPlayerCount()
        != StartingPlayerCountSnapshot
        )
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "StartCountdown failed: "
                "StartingPlayerCount projection mismatch. "
                "Snapshot=%d, GameState=%d"
            ),
            StartingPlayerCountSnapshot,
            MatchGameState->GetStartingPlayerCount()
        );

        return;
    }

    if (
        MatchGameState->GetCurrentPlayerStateCount()
        != StartingPlayerCountSnapshot
        )
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "StartCountdown failed: "
                "connected player count mismatch. "
                "Connected=%d, Snapshot=%d"
            ),
            MatchGameState
            ->GetCurrentPlayerStateCount(),
            StartingPlayerCountSnapshot
        );

        return;
    }

    const float CountdownDuration =
        MatchRuleData->CountdownDuration;

    if (CountdownDuration <= 0.0f)
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "StartCountdown failed: "
                "CountdownDuration must be positive. "
                "Duration=%.3f"
            ),
            CountdownDuration
        );

        return;
    }

    const double ServerNow =
        MatchGameState->GetServerWorldTimeSeconds();

    const double CountdownEndServerTime =
        ServerNow + CountdownDuration;


    /*
     * 현재 Phase를 종료할 서버 Timer를 먼저 등록한다.
     */

    GetWorldTimerManager().SetTimer(
        PhaseTimerHandle,
        this,
        &ADMMatchGameMode::HandleCountdownExpired,
        CountdownDuration,
        false
    );

    if (
        !GetWorldTimerManager()
        .IsTimerActive(PhaseTimerHandle)
        )
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "StartCountdown failed: "
                "Countdown Timer was not activated."
            )
        );

        return;
    }

    /*
     * 기존 MatchGameState->SetPhaseState() 대신
     * 중앙 Phase 전환 함수를 사용한다.
     */
    if (
        !TryTransitionPhase(
            EDMMatchPhase::Countdown,
            CountdownEndServerTime
        )
        )
    {
        /*
         * Phase 전환에 실패했다면
         * 먼저 등록한 Timer도 제거한다.
         */
        GetWorldTimerManager().ClearTimer(
            PhaseTimerHandle
        );

        return;
    }

    UE_LOG(
        LogDMMatchGameMode,
        Log,
        TEXT(
            "Countdown started. "
            "Players=%d, Duration=%.3f, "
            "ServerNow=%.3f, EndServerTime=%.3f"
        ),
        StartingPlayerCountSnapshot,
        CountdownDuration,
        ServerNow,
        CountdownEndServerTime
    );

}

void ADMMatchGameMode::HandleCountdownExpired()
{
    ADMMatchGameState* MatchGameState =
        GetGameState<ADMMatchGameState>();

    if (!IsValid(MatchGameState))
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "HandleCountdownExpired failed: "
                "MatchGameState is unavailable."
            )
        );

        return;
    }

    if (
        MatchGameState->GetCurrentPhase()
        != EDMMatchPhase::Countdown
        )
    {
        UE_LOG(
            LogDMMatchGameMode,
            Warning,
            TEXT(
                "Countdown expiration ignored: "
                "current phase is no longer Countdown."
            )
        );

        return;
    }

    UE_LOG(
        LogDMMatchGameMode,
        Log,
        TEXT(
            "Countdown expired. "
            "Transitioning to Playing."
        )
    );

    StartPlaying();
}

void ADMMatchGameMode::StartPlaying()
{
    ADMMatchGameState* MatchGameState =
        GetGameState<ADMMatchGameState>();

    if (!IsValid(MatchGameState))
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "StartPlaying failed: "
                "MatchGameState is unavailable."
            )
        );

        return;
    }

    if (!IsValid(MatchRuleData))
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "StartPlaying failed: "
                "MatchRuleData is unavailable."
            )
        );

        return;
    }

    if (
        MatchGameState->GetCurrentPhase()
        != EDMMatchPhase::Countdown
        )
    {
        UE_LOG(
            LogDMMatchGameMode,
            Warning,
            TEXT(
                "StartPlaying rejected: "
                "current phase is not Countdown."
            )
        );

        return;
    }

    const float MatchDuration =
        MatchRuleData->MatchDuration;

    if (MatchDuration <= 0.0f)
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "StartPlaying failed: "
                "MatchDuration must be positive. "
                "Duration=%.3f"
            ),
            MatchDuration
        );

        return;
    }

    /*
     * Countdown Timer가 만료된 뒤 같은 Handle을
     * 정규 경기 Timer 용도로 재사용한다.
     */
    GetWorldTimerManager().ClearTimer(
        PhaseTimerHandle
    );

    const double ServerNow =
        MatchGameState->GetServerWorldTimeSeconds();

    const double RegularEndServerTime =
        ServerNow + MatchDuration;

    GetWorldTimerManager().SetTimer(
        PhaseTimerHandle,
        this,
        &ADMMatchGameMode::HandleRegularTimeExpired,
        MatchDuration,
        false
    );

    if (
        !GetWorldTimerManager()
        .IsTimerActive(PhaseTimerHandle)
        )
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "StartPlaying failed: "
                "regular match timer was not activated."
            )
        );

        return;
    }

    /*
     * 기존 MatchGameState->SetPhaseState() 대신
     * 중앙 Phase 전환 함수를 사용한다.
     */
    if (
        !TryTransitionPhase(
            EDMMatchPhase::Playing,
            RegularEndServerTime
        )
        )
    {
        /*
         * Phase 전환 실패 시 정규 경기 Timer도 제거한다.
         */
        GetWorldTimerManager().ClearTimer(
            PhaseTimerHandle
        );

        return;
    }

    UE_LOG(
        LogDMMatchGameMode,
        Log,
        TEXT(
            "Regular match timer started. "
            "Players=%d, Duration=%.3f, "
            "ServerNow=%.3f, EndServerTime=%.3f"
        ),
        StartingPlayerCountSnapshot,
        MatchDuration,
        ServerNow,
        RegularEndServerTime
    );
}

void ADMMatchGameMode::HandleRegularTimeExpired()
{
    /*
     * One-shot Timer는 실행 후 비활성화되지만,
     * 현재 Phase Timer의 종료를 명시적으로 정리한다.
     */
    GetWorldTimerManager().ClearTimer(
        PhaseTimerHandle
    );

    ADMMatchGameState* MatchGameState =
        GetGameState<ADMMatchGameState>();

    if (!IsValid(MatchGameState))
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "HandleRegularTimeExpired failed: "
                "MatchGameState is unavailable."
            )
        );

        return;
    }

    if (
        MatchGameState->GetCurrentPhase()
        != EDMMatchPhase::Playing
        )
    {
        UE_LOG(
            LogDMMatchGameMode,
            Warning,
            TEXT(
                "Regular time expiration ignored: "
                "current phase is not Playing."
            )
        );

        return;
    }

    const FDMMatchPhaseState CurrentPhaseState =
        MatchGameState->GetPhaseState();

    const double ServerNow =
        MatchGameState->GetServerWorldTimeSeconds();

    const double ExpirationDrift =
        ServerNow
        - CurrentPhaseState.PhaseEndServerTime;

    const double RemainingRegularTime =
        MatchGameState->GetRemainingRegularTime();

    UE_LOG(
        LogDMMatchGameMode,
        Log,
        TEXT(
            "Regular time expired. "
            "Result evaluation boundary reached. "
            "ServerNow=%.3f, Deadline=%.3f, "
            "Drift=%.3f, Remaining=%.3f"
        ),
        ServerNow,
        CurrentPhaseState.PhaseEndServerTime,
        ExpirationDrift,
        RemainingRegularTime
    );

 
}

bool ADMMatchGameMode::TryTransitionPhase(
    const EDMMatchPhase TargetPhase,
    const double PhaseEndServerTime
)
{
    if (!HasAuthority())
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "Phase transition rejected: "
                "server authority is required."
            )
        );

        return false;
    }

    ADMMatchGameState* MatchGameState =
        GetGameState<ADMMatchGameState>();

    if (!IsValid(MatchGameState))
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "Phase transition failed: "
                "MatchGameState is unavailable."
            )
        );

        return false;
    }

    const EDMMatchPhase CurrentPhase =
        MatchGameState->GetCurrentPhase();

    if (CurrentPhase == TargetPhase)
    {
        UE_LOG(
            LogDMMatchGameMode,
            Warning,
            TEXT(
                "Phase transition rejected: "
                "current and target phases are identical. "
                "Phase=%s"
            ),
            *UEnum::GetValueAsString(CurrentPhase)
        );

        return false;
    }

    if (
        !IsPhaseTransitionAllowed(
            CurrentPhase,
            TargetPhase
        )
        )
    {
        UE_LOG(
            LogDMMatchGameMode,
            Warning,
            TEXT(
                "Phase transition rejected: "
                "transition is not allowed. "
                "From=%s, To=%s"
            ),
            *UEnum::GetValueAsString(CurrentPhase),
            *UEnum::GetValueAsString(TargetPhase)
        );

        return false;
    }

    const bool bTimedPhase =
        TargetPhase == EDMMatchPhase::Countdown
        || TargetPhase == EDMMatchPhase::Playing
        || TargetPhase == EDMMatchPhase::OvertimeIntro
        || TargetPhase == EDMMatchPhase::Overtime;

    const double ServerNow =
        MatchGameState->GetServerWorldTimeSeconds();

    if (
        bTimedPhase
        && PhaseEndServerTime <= ServerNow
        )
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "Phase transition failed: "
                "timed phase requires a future deadline. "
                "Target=%s, ServerNow=%.3f, Deadline=%.3f"
            ),
            *UEnum::GetValueAsString(TargetPhase),
            ServerNow,
            PhaseEndServerTime
        );

        return false;
    }

    if (
        !bTimedPhase
        && !FMath::IsNearlyZero(
            PhaseEndServerTime
        )
        )
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "Phase transition failed: "
                "untimed phase requires a zero deadline. "
                "Target=%s, Deadline=%.3f"
            ),
            *UEnum::GetValueAsString(TargetPhase),
            PhaseEndServerTime
        );

        return false;
    }

    FDMMatchPhaseState NewPhaseState;
    NewPhaseState.CurrentPhase = TargetPhase;
    NewPhaseState.PhaseEndServerTime =
        PhaseEndServerTime;

    MatchGameState->SetPhaseState(
        NewPhaseState
    );

    const FDMMatchPhaseState AppliedState =
        MatchGameState->GetPhaseState();

    const bool bPhaseApplied =
        AppliedState.CurrentPhase == TargetPhase
        && FMath::IsNearlyEqual(
            AppliedState.PhaseEndServerTime,
            PhaseEndServerTime
        );

    if (!bPhaseApplied)
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "Phase transition failed: "
                "GameState did not apply the requested state. "
                "Requested=%s, Applied=%s"
            ),
            *UEnum::GetValueAsString(TargetPhase),
            *UEnum::GetValueAsString(
                AppliedState.CurrentPhase
            )
        );

        return false;
    }

    UE_LOG(
        LogDMMatchGameMode,
        Log,
        TEXT(
            "Match phase transitioned. "
            "From=%s, To=%s, EndServerTime=%.3f"
        ),
        *UEnum::GetValueAsString(CurrentPhase),
        *UEnum::GetValueAsString(TargetPhase),
        PhaseEndServerTime
    );

    return true;
}

bool ADMMatchGameMode::IsPhaseTransitionAllowed(
    const EDMMatchPhase FromPhase,
    const EDMMatchPhase ToPhase
)
{
    switch (FromPhase)
    {
    case EDMMatchPhase::Waiting:
        return ToPhase == EDMMatchPhase::Countdown
            || ToPhase == EDMMatchPhase::Aborted;

    case EDMMatchPhase::Countdown:
        return ToPhase == EDMMatchPhase::Playing
            || ToPhase == EDMMatchPhase::Aborted;

    case EDMMatchPhase::Playing:
        return ToPhase
            == EDMMatchPhase::OvertimeIntro
            || ToPhase == EDMMatchPhase::Finished
            || ToPhase == EDMMatchPhase::Aborted;

    case EDMMatchPhase::OvertimeIntro:
        return ToPhase == EDMMatchPhase::Overtime
            || ToPhase == EDMMatchPhase::Aborted;

    case EDMMatchPhase::Overtime:
        return ToPhase == EDMMatchPhase::Finished
            || ToPhase == EDMMatchPhase::Aborted;

    case EDMMatchPhase::Finished:
    case EDMMatchPhase::Aborted:
    default:
        return false;
    }
}

bool ADMMatchGameMode::IsFinishReason(
    const EDMMatchEndReason EndReason
)
{
    switch (EndReason)
    {
    case EDMMatchEndReason::RelicVictory:
    case EDMMatchEndReason::EliminationVictory:
    case EDMMatchEndReason::RegularTimeVictory:
    case EDMMatchEndReason::OvertimeVictory:
    case EDMMatchEndReason::AllDefeated:
        return true;

    case EDMMatchEndReason::None:
    case EDMMatchEndReason::PlayerDisconnect:
    case EDMMatchEndReason::HostLeft:
    default:
        return false;
    }
}


bool ADMMatchGameMode::IsFinishReasonAllowedForPhase(
    const EDMMatchPhase Phase,
    const EDMMatchEndReason EndReason
)
{
    switch (EndReason)
    {
    case EDMMatchEndReason::RelicVictory:
    case EDMMatchEndReason::EliminationVictory:
    case EDMMatchEndReason::AllDefeated:
        return Phase == EDMMatchPhase::Playing
            || Phase == EDMMatchPhase::Overtime;

    case EDMMatchEndReason::RegularTimeVictory:
        return Phase == EDMMatchPhase::Playing;

    case EDMMatchEndReason::OvertimeVictory:
        return Phase == EDMMatchPhase::Overtime;

    case EDMMatchEndReason::None:
    case EDMMatchEndReason::PlayerDisconnect:
    case EDMMatchEndReason::HostLeft:
    default:
        return false;
    }
}


bool ADMMatchGameMode::IsAbortReason(
    const EDMMatchEndReason EndReason
)
{
    return EndReason
        == EDMMatchEndReason::PlayerDisconnect
        || EndReason
        == EDMMatchEndReason::HostLeft;
}

bool ADMMatchGameMode::TryFinishMatch(
    const EDMMatchEndReason EndReason
)
{
    if (!HasAuthority())
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "Finish Match rejected: "
                "server authority is required."
            )
        );

        return false;
    }

    if (!IsFinishReason(EndReason))
    {
        UE_LOG(
            LogDMMatchGameMode,
            Warning,
            TEXT(
                "Finish Match rejected: "
                "invalid finish reason. Reason=%s"
            ),
            *UEnum::GetValueAsString(EndReason)
        );

        return false;
    }

    ADMMatchGameState* MatchGameState =
        GetGameState<ADMMatchGameState>();

    if (!IsValid(MatchGameState))
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "Finish Match failed: "
                "MatchGameState is unavailable."
            )
        );

        return false;
    }

    const EDMMatchPhase CurrentPhase =
        MatchGameState->GetCurrentPhase();

    if (
        !IsFinishReasonAllowedForPhase(
            CurrentPhase,
            EndReason
        )
        )
    {
        UE_LOG(
            LogDMMatchGameMode,
            Warning,
            TEXT(
                "Finish Match rejected: "
                "finish reason is not valid "
                "for the current phase. "
                "Phase=%s, Reason=%s"
            ),
            *UEnum::GetValueAsString(CurrentPhase),
            *UEnum::GetValueAsString(EndReason)
        );

        return false;
    }

    if (
        MatchGameState->GetMatchEndReason()
        != EDMMatchEndReason::None
        )
    {
        UE_LOG(
            LogDMMatchGameMode,
            Warning,
            TEXT(
                "Finish Match rejected: "
                "match result is already locked. "
                "CurrentReason=%s"
            ),
            *UEnum::GetValueAsString(
                MatchGameState->GetMatchEndReason()
            )
        );

        return false;
    }

    if (
        !IsPhaseTransitionAllowed(
            CurrentPhase,
            EDMMatchPhase::Finished
        )
        )
    {
        UE_LOG(
            LogDMMatchGameMode,
            Warning,
            TEXT(
                "Finish Match rejected: "
                "current phase cannot transition "
                "to Finished. CurrentPhase=%s"
            ),
            *UEnum::GetValueAsString(CurrentPhase)
        );

        return false;
    }

    /*
     * 종료 원인을 먼저 설정한다.
     *
     * 클라이언트에서 Phase가 Finished로 관찰될 때
     * 종료 원인이 이미 준비된 상태를 우선한다.
     */
    MatchGameState->SetMatchEndReason(
        EndReason
    );

    if (
        MatchGameState->GetMatchEndReason()
        != EndReason
        )
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "Finish Match failed: "
                "MatchEndReason was not applied."
            )
        );

        return false;
    }

    GetWorldTimerManager().ClearTimer(
        PhaseTimerHandle
    );

    if (
        !TryTransitionPhase(
            EDMMatchPhase::Finished,
            0.0
        )
        )
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "Finish Match invariant failure: "
                "reason was applied but terminal "
                "phase transition failed."
            )
        );

        return false;
    }

    UE_LOG(
        LogDMMatchGameMode,
        Log,
        TEXT(
            "Match finished. Reason=%s"
        ),
        *UEnum::GetValueAsString(EndReason)
    );

    return true;
}

bool ADMMatchGameMode::TryAbortMatch(
    const EDMMatchEndReason EndReason
)
{
    if (!HasAuthority())
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "Abort Match rejected: "
                "server authority is required."
            )
        );

        return false;
    }

    if (!IsAbortReason(EndReason))
    {
        UE_LOG(
            LogDMMatchGameMode,
            Warning,
            TEXT(
                "Abort Match rejected: "
                "invalid abort reason. Reason=%s"
            ),
            *UEnum::GetValueAsString(EndReason)
        );

        return false;
    }

    ADMMatchGameState* MatchGameState =
        GetGameState<ADMMatchGameState>();

    if (!IsValid(MatchGameState))
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "Abort Match failed: "
                "MatchGameState is unavailable."
            )
        );

        return false;
    }

    if (
        MatchGameState->GetMatchEndReason()
        != EDMMatchEndReason::None
        )
    {
        UE_LOG(
            LogDMMatchGameMode,
            Warning,
            TEXT(
                "Abort Match rejected: "
                "match result is already locked. "
                "CurrentReason=%s"
            ),
            *UEnum::GetValueAsString(
                MatchGameState->GetMatchEndReason()
            )
        );

        return false;
    }

    if (
        !IsPhaseTransitionAllowed(
            MatchGameState->GetCurrentPhase(),
            EDMMatchPhase::Aborted
        )
        )
    {
        UE_LOG(
            LogDMMatchGameMode,
            Warning,
            TEXT(
                "Abort Match rejected: "
                "current phase cannot transition "
                "to Aborted. CurrentPhase=%s"
            ),
            *UEnum::GetValueAsString(
                MatchGameState->GetCurrentPhase()
            )
        );

        return false;
    }

    MatchGameState->SetMatchEndReason(
        EndReason
    );

    if (
        MatchGameState->GetMatchEndReason()
        != EndReason
        )
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "Abort Match failed: "
                "MatchEndReason was not applied."
            )
        );

        return false;
    }

    GetWorldTimerManager().ClearTimer(
        PhaseTimerHandle
    );

    if (
        !TryTransitionPhase(
            EDMMatchPhase::Aborted,
            0.0
        )
        )
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "Abort Match invariant failure: "
                "reason was applied but terminal "
                "phase transition failed."
            )
        );

        return false;
    }

    UE_LOG(
        LogDMMatchGameMode,
        Log,
        TEXT(
            "Match aborted. Reason=%s"
        ),
        *UEnum::GetValueAsString(EndReason)
    );

    return true;
}


bool ADMMatchGameMode::TryStartOvertimeIntro()
{
    if (!HasAuthority())
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "Start OvertimeIntro rejected: "
                "server authority is required."
            )
        );

        return false;
    }

    ADMMatchGameState* MatchGameState =
        GetGameState<ADMMatchGameState>();

    if (!IsValid(MatchGameState))
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "Start OvertimeIntro failed: "
                "MatchGameState is unavailable."
            )
        );

        return false;
    }

    if (!IsValid(MatchRuleData))
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "Start OvertimeIntro failed: "
                "MatchRuleData is unavailable."
            )
        );

        return false;
    }

    if (
        MatchGameState->GetCurrentPhase()
        != EDMMatchPhase::Playing
        )
    {
        UE_LOG(
            LogDMMatchGameMode,
            Warning,
            TEXT(
                "Start OvertimeIntro rejected: "
                "current phase is not Playing. "
                "CurrentPhase=%s"
            ),
            *UEnum::GetValueAsString(
                MatchGameState->GetCurrentPhase()
            )
        );

        return false;
    }

    if (
        MatchGameState->GetMatchEndReason()
        != EDMMatchEndReason::None
        )
    {
        UE_LOG(
            LogDMMatchGameMode,
            Warning,
            TEXT(
                "Start OvertimeIntro rejected: "
                "match result is already locked. "
                "Reason=%s"
            ),
            *UEnum::GetValueAsString(
                MatchGameState->GetMatchEndReason()
            )
        );

        return false;
    }

    /*
     * 정규 경기 시간이 실제로 끝난 이후에만
     * 연장전 진입을 허용한다.
     */
    const double RemainingRegularTime =
        MatchGameState->GetRemainingRegularTime();

    if (RemainingRegularTime > KINDA_SMALL_NUMBER)
    {
        UE_LOG(
            LogDMMatchGameMode,
            Warning,
            TEXT(
                "Start OvertimeIntro rejected: "
                "regular time has not expired. "
                "Remaining=%.3f"
            ),
            RemainingRegularTime
        );

        return false;
    }

    const float OvertimeIntroDuration =
        MatchRuleData->OvertimeIntroDuration;

    if (OvertimeIntroDuration <= 0.0f)
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "Start OvertimeIntro failed: "
                "OvertimeIntroDuration must be positive. "
                "Duration=%.3f"
            ),
            OvertimeIntroDuration
        );

        return false;
    }

    GetWorldTimerManager().ClearTimer(
        PhaseTimerHandle
    );

    const double ServerNow =
        MatchGameState->GetServerWorldTimeSeconds();

    const double OvertimeIntroEndServerTime =
        ServerNow + OvertimeIntroDuration;

    GetWorldTimerManager().SetTimer(
        PhaseTimerHandle,
        this,
        &ADMMatchGameMode::HandleOvertimeIntroExpired,
        OvertimeIntroDuration,
        false
    );

    if (
        !GetWorldTimerManager()
        .IsTimerActive(PhaseTimerHandle)
        )
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "Start OvertimeIntro failed: "
                "PhaseTimer was not activated."
            )
        );

        return false;
    }

    if (
        !TryTransitionPhase(
            EDMMatchPhase::OvertimeIntro,
            OvertimeIntroEndServerTime
        )
        )
    {
        GetWorldTimerManager().ClearTimer(
            PhaseTimerHandle
        );

        return false;
    }

    UE_LOG(
        LogDMMatchGameMode,
        Log,
        TEXT(
            "OvertimeIntro started. "
            "Duration=%.3f, ServerNow=%.3f, "
            "EndServerTime=%.3f"
        ),
        OvertimeIntroDuration,
        ServerNow,
        OvertimeIntroEndServerTime
    );

    return true;
}

void ADMMatchGameMode::HandleOvertimeIntroExpired()
{
    ADMMatchGameState* MatchGameState =
        GetGameState<ADMMatchGameState>();

    if (!IsValid(MatchGameState))
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "HandleOvertimeIntroExpired failed: "
                "MatchGameState is unavailable."
            )
        );

        return;
    }

    if (
        MatchGameState->GetCurrentPhase()
        != EDMMatchPhase::OvertimeIntro
        )
    {
        UE_LOG(
            LogDMMatchGameMode,
            Warning,
            TEXT(
                "OvertimeIntro expiration ignored: "
                "current phase is no longer "
                "OvertimeIntro."
            )
        );

        return;
    }

    UE_LOG(
        LogDMMatchGameMode,
        Log,
        TEXT(
            "OvertimeIntro expired. "
            "Transitioning to Overtime."
        )
    );

    StartOvertime();
}

void ADMMatchGameMode::StartOvertime()
{
    ADMMatchGameState* MatchGameState =
        GetGameState<ADMMatchGameState>();

    if (!IsValid(MatchGameState))
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "StartOvertime failed: "
                "MatchGameState is unavailable."
            )
        );

        return;
    }

    if (!IsValid(MatchRuleData))
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "StartOvertime failed: "
                "MatchRuleData is unavailable."
            )
        );

        return;
    }

    if (
        MatchGameState->GetCurrentPhase()
        != EDMMatchPhase::OvertimeIntro
        )
    {
        UE_LOG(
            LogDMMatchGameMode,
            Warning,
            TEXT(
                "StartOvertime rejected: "
                "current phase is not OvertimeIntro."
            )
        );

        return;
    }

    const float OvertimeDuration =
        MatchRuleData->OvertimeDuration;

    if (OvertimeDuration <= 0.0f)
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "StartOvertime failed: "
                "OvertimeDuration must be positive. "
                "Duration=%.3f"
            ),
            OvertimeDuration
        );

        return;
    }

    GetWorldTimerManager().ClearTimer(
        PhaseTimerHandle
    );

    const double ServerNow =
        MatchGameState->GetServerWorldTimeSeconds();

    const double OvertimeEndServerTime =
        ServerNow + OvertimeDuration;

    GetWorldTimerManager().SetTimer(
        PhaseTimerHandle,
        this,
        &ADMMatchGameMode::HandleOvertimeExpired,
        OvertimeDuration,
        false
    );

    if (
        !GetWorldTimerManager()
        .IsTimerActive(PhaseTimerHandle)
        )
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "StartOvertime failed: "
                "PhaseTimer was not activated."
            )
        );

        return;
    }

    if (
        !TryTransitionPhase(
            EDMMatchPhase::Overtime,
            OvertimeEndServerTime
        )
        )
    {
        GetWorldTimerManager().ClearTimer(
            PhaseTimerHandle
        );

        return;
    }

    UE_LOG(
        LogDMMatchGameMode,
        Log,
        TEXT(
            "Overtime started. "
            "Duration=%.3f, ServerNow=%.3f, "
            "EndServerTime=%.3f"
        ),
        OvertimeDuration,
        ServerNow,
        OvertimeEndServerTime
    );
}

void ADMMatchGameMode::HandleOvertimeExpired()
{
    GetWorldTimerManager().ClearTimer(
        PhaseTimerHandle
    );

    ADMMatchGameState* MatchGameState =
        GetGameState<ADMMatchGameState>();

    if (!IsValid(MatchGameState))
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT(
                "HandleOvertimeExpired failed: "
                "MatchGameState is unavailable."
            )
        );

        return;
    }

    if (
        MatchGameState->GetCurrentPhase()
        != EDMMatchPhase::Overtime
        )
    {
        UE_LOG(
            LogDMMatchGameMode,
            Warning,
            TEXT(
                "Overtime expiration ignored: "
                "current phase is no longer Overtime."
            )
        );

        return;
    }

    const FDMMatchPhaseState PhaseState =
        MatchGameState->GetPhaseState();

    const double ServerNow =
        MatchGameState->GetServerWorldTimeSeconds();

    const double RemainingOvertime =
        FMath::Max(
            0.0,
            PhaseState.PhaseEndServerTime - ServerNow
        );

    const double ExpirationDrift =
        ServerNow - PhaseState.PhaseEndServerTime;

    UE_LOG(
        LogDMMatchGameMode,
        Log,
        TEXT(
            "Overtime expired. "
            "Overtime result evaluation boundary reached. "
            "ServerNow=%.3f, Deadline=%.3f, "
            "Drift=%.3f, Remaining=%.3f"
        ),
        ServerNow,
        PhaseState.PhaseEndServerTime,
        ExpirationDrift,
        RemainingOvertime
    );

    /*
     * 유물 시스템 구현 후 이 지점에서
     * 연장전 승자를 평가한다.
     *
     * 승자가 있으면:
     * TryFinishMatch(
     *     EDMMatchEndReason::OvertimeVictory
     * );
     *
     * 현재는 Overtime Phase에 머문다.
     */
}





