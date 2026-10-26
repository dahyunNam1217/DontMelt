#include "Framework/Match/DMMatchGameMode.h"

#include "Core/DMGameplayUtils.h"
#include "Core/DMMatchConstants.h"

#include "Characters/DMPlayerCharacter.h"
#include "Characters/DMNPCCharacter.h"
#include "Characters/Components/DMCombatComponent.h"
#include "Characters/Components/DMInteractionComponent.h"
#include "Characters/Components/DMCharacterStateComponent.h"
#include "Characters/Components/DMJellyPhysicsComponent.h"
#include "AI/DMNPCPopulationDirector.h"
#include "Quests/DMQuestDirector.h"
#include "Quests/DMQuestTypes.h"
#include "Core/DMGameplayTags.h"
#include "Framework/Match/DMMatchGameState.h"
#include "Framework/Match/DMMatchPlayerController.h"
#include "Framework/Match/DMMatchPlayerState.h"
#include "Framework/Match/DMRespawnCoordinator.h"
#include "Framework/Match/DMRespawnPoint.h"
#include "Events/DMRandomEventDirector.h"
#include "Events/DMRandomEventTypes.h"
#include "Data/DMRandomEventSetDataAsset.h"
#include "Data/DMRelicRuleDataAsset.h"
#include "Data/DMMatchRuleDataAsset.h"
#include "Data/DMSkillSetDataAsset.h"
#include "Skills/DMSkillComponent.h"
#include "Relic/DMRelic.h"
#include "Relic/DMRelicManager.h"
#include "Relic/DMBaseZone.h"
#include "Relic/DMRelicChest.h"
#include "Online/DMOnlineSessionSubsystem.h"
#include "Core/DMGameInstance.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "UObject/UObjectGlobals.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogDMMatchGameMode, Log, All);

namespace
{
    // 정규 경기 종료 60초 전부터 3초 동안 한 번 공개한다.
    constexpr float RelicRevealLeadSeconds = 60.0f;
    constexpr float RelicRevealDurationSeconds = 3.0f;

    bool ShouldEnforceSteamMatchRoster(const UWorld* World)
    {
#if WITH_EDITOR
        if (World != nullptr && World->WorldType == EWorldType::PIE && World->GetNetMode() == NM_Standalone)
        {
            return false;
        }
#endif

        return true;
    }

}

#pragma region Lifecycle

ADMMatchGameMode::ADMMatchGameMode()
{
    GameStateClass = ADMMatchGameState::StaticClass();
    PlayerStateClass = ADMMatchPlayerState::StaticClass();
    PlayerControllerClass = ADMMatchPlayerController::StaticClass();

    // C++ fallback.
    // BP_DMMatchGameMode에서 BP_DMPlayerCharacter로 덮어쓴다.
    DefaultPawnClass = ADMPlayerCharacter::StaticClass();

    // C++ fallback.
    // BP_DMMatchGameMode에서 BP_DMNPCCharacter로 덮어쓴다.
    NPCClass = ADMNPCCharacter::StaticClass();
}

void ADMMatchGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);

    // 리스폰 수명주기는 전담 객체가 관리한다.
    // 초기화 실패 여부와 무관하게 먼저 만들어 두어 Logout·종료 시 정리 경로가 항상 동작한다.
    RespawnCoordinator = NewObject<UDMRespawnCoordinator>(this);
    RespawnCoordinator->Initialize(this, MatchRuleData, RespawnRetryDelaySeconds);

    // InitGame의 MapName은 짧은 이름(PIE에서는 UEDPIE_ 접두사 포함)이므로
    // 현재 월드 패키지의 롱 패키지 이름에서 PIE 접두사를 제거해 재경기 경로로 저장한다.
    const UWorld* InitWorld = GetWorld();
    const UPackage* InitWorldPackage = IsValid(InitWorld) ? InitWorld->GetPackage() : nullptr;

    CurrentMatchMapPackageName =
        IsValid(InitWorldPackage) ? UWorld::RemovePIEPrefix(InitWorldPackage->GetName()) : MapName;

    if (!ErrorMessage.IsEmpty())
    {
        bMatchInitializationFailed = true;

        return;
    }

    // 에셋 참조 → 규칙 데이터 → Travel 옵션 → Steam 승인 명단 순서로 검증한다.
    const bool bInitialized = ValidateRequiredAssets(ErrorMessage) && ValidateMatchRuleData(ErrorMessage)
        && ParseExpectedPlayerCount(Options, ErrorMessage) && PrepareApprovedMatchRoster(ErrorMessage);

    if (!bInitialized)
    {
        bMatchInitializationFailed = true;

        UE_LOG(LogDMMatchGameMode, Error, TEXT("Match initialization failed: %s"), *ErrorMessage);

        return;
    }

    UE_LOG(LogDMMatchGameMode, Log, TEXT("Match initialized. ExpectedPlayers=%d"), ExpectedPlayerCount);
}

void ADMMatchGameMode::StartPlay()
{
    Super::StartPlay();

    if (bMatchInitializationFailed)
    {
        AbortMatchForInternalFailure(TEXT("Match initialization failed."));
        return;
    }

    const ADMMatchGameState* State = GetGameState<ADMMatchGameState>();
    if (!IsValid(State))
    {
        AbortMatchForInternalFailure(TEXT("StartPlay failed: MatchGameState is unavailable."));
        return;
    }
    if (HasAuthority() && StartingPlayerCountSnapshot == 0
        && State->GetCurrentPhase() == EDMMatchPhase::Waiting)
    {
        GetWorldTimerManager().SetTimer(StartingRosterTimeoutHandle, this,
            &ThisClass::HandleStartingRosterTimeout, StartingRosterTimeoutSeconds, false);
        if (!GetWorldTimerManager().IsTimerActive(StartingRosterTimeoutHandle))
        {
            AbortMatchForInternalFailure(TEXT("Starting roster timeout could not be scheduled."));
        }
    }
}

void ADMMatchGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearPreMatchWaitingState();
    Super::EndPlay(EndPlayReason);
}

bool ADMMatchGameMode::ValidateRequiredAssets(FString& OutErrorMessage) const
{
    if (!IsValid(MatchRuleData))
    {
        OutErrorMessage = TEXT("MatchRuleData is not assigned.");
        return false;
    }

    if (!IsValid(QuestDefinitionTable))
    {
        OutErrorMessage = TEXT("QuestDefinitionTable is not assigned.");
        return false;
    }

    if (!IsValid(RandomEventSetData))
    {
        OutErrorMessage = TEXT("RandomEventSetData is not assigned.");
        return false;
    }

    if (!IsValid(SkillSetData.Get()))
    {
        OutErrorMessage = TEXT("SkillSetData is not assigned.");
        return false;
    }

    FString SkillSetFailureReason;

    if (!SkillSetData->ValidateDefinitionSet(SkillSetFailureReason))
    {
        OutErrorMessage = FString::Printf(TEXT("SkillSetData is invalid. Reason=%s"), *SkillSetFailureReason);
        return false;
    }

    if (!IsValid(RelicRuleData))
    {
        OutErrorMessage = TEXT("RelicRuleData is not assigned.");
        return false;
    }

    if (!RelicClass)
    {
        OutErrorMessage = TEXT("RelicClass is not assigned.");
        return false;
    }

    if (!NPCClass)
    {
        OutErrorMessage = TEXT("NPCClass is not assigned.");
        return false;
    }

    return true;
}

bool ADMMatchGameMode::ValidateMatchRuleData(FString& OutErrorMessage) const
{
    // 규칙값 자체의 검증은 에디터 Data Validation과 같은 함수를 사용한다.
    return MatchRuleData->ValidateRules(OutErrorMessage);
}

bool ADMMatchGameMode::ParseExpectedPlayerCount(const FString& Options, FString& OutErrorMessage)
{
    const FString ExpectedPlayersOption = UGameplayStatics::ParseOption(Options, TEXT("ExpectedPlayers"));

    const int32 ParsedExpectedPlayerCount = FCString::Atoi(*ExpectedPlayersOption);

    if (!FMath::IsWithinInclusive(
            ParsedExpectedPlayerCount, MatchRuleData->MinPlayerCount, MatchRuleData->MaxPlayerCount
        ))
    {
        OutErrorMessage =
            FString::Printf(TEXT("ExpectedPlayers option is invalid. Received=%s"), *ExpectedPlayersOption);
        return false;
    }

    ExpectedPlayerCount = ParsedExpectedPlayerCount;

    return true;
}

bool ADMMatchGameMode::PrepareApprovedMatchRoster(FString& OutErrorMessage)
{
    if (!ShouldEnforceSteamMatchRoster(GetWorld()))
    {
        return true;
    }

    UGameInstance* GameInstance = GetGameInstance();

    UDMOnlineSessionSubsystem* Session =
        IsValid(GameInstance) ? GameInstance->GetSubsystem<UDMOnlineSessionSubsystem>() : nullptr;

    // 매치 입장 명단은 이전 로비 서버가 확정한 값이어야 한다.
    if (!IsValid(Session) || !Session->HasApprovedMatchRoster()
        || Session->GetApprovedMatchPlayerCount() != ExpectedPlayerCount)
    {
        OutErrorMessage =
            FString::Printf(TEXT("Approved Steam match roster is unavailable. Expected=%d"), ExpectedPlayerCount);
        return false;
    }

    // 이번 매치 맵에서는 승인된 플레이어가 다시 최초 입장한다.
    Session->ResetMatchArrivalHistory();

    return true;
}

void ADMMatchGameMode::PreLogin(
    const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage
)
{
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);

    if (!ErrorMessage.IsEmpty())
    {
        return;
    }

    if (bMatchInitializationFailed || !IsValid(MatchRuleData) || ExpectedPlayerCount <= 0)
    {
        ErrorMessage = TEXT("Match configuration is invalid.");

        return;
    }

    // 로비에서 승인된 사용자만 새 매치에 입장시킨다.
    if (ShouldEnforceSteamMatchRoster(GetWorld()))
    {
        UGameInstance* GameInstance = GetGameInstance();

        UDMOnlineSessionSubsystem* Session =
            IsValid(GameInstance) ? GameInstance->GetSubsystem<UDMOnlineSessionSubsystem>() : nullptr;

        if (!IsValid(Session) || !Session->HasApprovedMatchRoster() || !Session->IsApprovedMatchPlayer(UniqueId))
        {
            ErrorMessage = TEXT("Player was not approved by the lobby.");

            UE_LOG(LogDMMatchGameMode, Warning, TEXT("Match login rejected: player not in approved roster."));

            return;
        }

        if (Session->HasMatchPlayerArrived(UniqueId))
        {
            ErrorMessage = TEXT("Rejoining this match is not allowed.");

            UE_LOG(LogDMMatchGameMode, Warning, TEXT("Match login rejected: player already entered."));

            return;
        }
    }

    if (IsStartingRosterLocked())
    {
        ErrorMessage = TEXT("Match roster is locked.");

        UE_LOG(LogDMMatchGameMode, Warning, TEXT("Match login rejected: starting roster is locked."));

        return;
    }

    if (GetNumPlayers() >= ExpectedPlayerCount)
    {
        ErrorMessage = TEXT("Match has reached its expected player count.");

        UE_LOG(
            LogDMMatchGameMode,
            Warning,
            TEXT("Match login rejected. CurrentPlayers=%d, ExpectedPlayers=%d"),
            GetNumPlayers(),
            ExpectedPlayerCount
        );
    }
}

void ADMMatchGameMode::PostLogin(APlayerController* NewPlayer)
{
    ADMMatchPlayerController* MatchPlayerController = Cast<ADMMatchPlayerController>(NewPlayer);

    if (!ensureMsgf(MatchPlayerController != nullptr, TEXT("PostLogin received an invalid MatchPlayerController.")))
    {
        return;
    }

    // 초기화에 실패한 매치는 참가자 준비 없이 Controller만 만들고, StartPlay에서 중단 결과를 표시한다.
    if (bMatchInitializationFailed)
    {
        Super::PostLogin(NewPlayer);

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
    if (IsStartingRosterLocked() || GetNumPlayers() > ExpectedPlayerCount)
    {
        UE_LOG(
            LogDMMatchGameMode,
            Warning,
            TEXT("PostLogin rejected a late player. CurrentPlayers=%d, ExpectedPlayers=%d"),
            GetNumPlayers(),
            ExpectedPlayerCount
        );

        if (IsValid(GameSession))
        {
            GameSession->KickPlayer(NewPlayer, FText::FromString(TEXT("Match roster is locked.")));
        }

        return;
    }

    ADMMatchPlayerState* MatchPlayerState = MatchPlayerController->GetPlayerState<ADMMatchPlayerState>();

    if (!ensureMsgf(MatchPlayerState != nullptr, TEXT("MatchPlayerState was not created for the joining player.")))
    {
        return;
    }

    if (ShouldEnforceSteamMatchRoster(GetWorld()))
    {
        UGameInstance* GameInstance = GetGameInstance();

        UDMOnlineSessionSubsystem* Session =
            IsValid(GameInstance) ? GameInstance->GetSubsystem<UDMOnlineSessionSubsystem>() : nullptr;

        const FUniqueNetIdRepl& PlayerId = MatchPlayerState->GetUniqueId();

        if (!IsValid(Session) || !Session->IsApprovedMatchPlayer(PlayerId) || Session->HasMatchPlayerArrived(PlayerId))
        {
            UE_LOG(LogDMMatchGameMode, Warning, TEXT("PostLogin rejected: unauthorized or duplicate ID."));

            if (IsValid(GameSession))
            {
                GameSession->KickPlayer(NewPlayer, FText::FromString(TEXT("Match entry was not authorized.")));
            }

            return;
        }
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
            TEXT("PostLogin rejected: BaseSlot was not assigned before login completion. PlayerName=%s"),
            *MatchPlayerState->GetPlayerName()
        );

        if (IsValid(GameSession))
        {
            GameSession->KickPlayer(NewPlayer, FText::FromString(TEXT("BaseSlot initialization failed.")));
        }

        return;
    }

    UDMSkillComponent* SkillComponent = MatchPlayerState->GetSkillComponent();

    if (!ensureMsgf(
            IsValid(SkillComponent),
            TEXT("PostLogin rejected: SkillComponent is missing. PlayerName=%s"),
            *MatchPlayerState->GetPlayerName()
        ))
    {
        if (IsValid(GameSession))
        {
            GameSession->KickPlayer(NewPlayer, FText::FromString(TEXT("Skill system initialization failed.")));
        }

        return;
    }

    if (!SkillComponent->InitializeSkillSystem(SkillSetData.Get()))
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT("PostLogin rejected: Skill system initialization failed. PlayerName=%s"),
            *MatchPlayerState->GetPlayerName()
        );

        if (IsValid(GameSession))
        {
            GameSession->KickPlayer(NewPlayer, FText::FromString(TEXT("Skill system initialization failed.")));
        }

        return;
    }

    if (ShouldEnforceSteamMatchRoster(GetWorld()))
    {
        UGameInstance* GameInstance = GetGameInstance();

        UDMOnlineSessionSubsystem* Session =
            IsValid(GameInstance) ? GameInstance->GetSubsystem<UDMOnlineSessionSubsystem>() : nullptr;

        if (!IsValid(Session) || !Session->MarkMatchPlayerArrived(MatchPlayerState->GetUniqueId()))
        {
            UE_LOG(LogDMMatchGameMode, Error, TEXT("PostLogin failed: could not record approved arrival."));

            if (IsValid(GameSession))
            {
                GameSession->KickPlayer(NewPlayer, FText::FromString(TEXT("Match admission failed.")));
            }

            return;
        }
    }

    UE_LOG(
        LogDMMatchGameMode,
        Log,
        TEXT("Match player prepared for spawn. PlayerName=%s, BaseSlot=%d, Connected=%d/%d"),
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

void ADMMatchGameMode::Logout(AController* Exiting)
{
    /*
     * Super::Logout 이후에는 PlayerState가 제거될 수 있으므로
     * 필요한 정보를 먼저 저장한다.
     */
    ADMMatchPlayerState* ExitingPlayerState =
        Exiting != nullptr ? Exiting->GetPlayerState<ADMMatchPlayerState>() : nullptr;

    const bool bHasMatchPlayerState = IsValid(ExitingPlayerState);

    if (bHasMatchPlayerState)
    {
        if (IsValid(RespawnCoordinator))
        {
            RespawnCoordinator->ClearRespawnTimer(ExitingPlayerState);
        }
    }

    const bool bWasAdmittedParticipant = bHasMatchPlayerState && ExitingPlayerState->HasAssignedBaseSlot();

    const bool bIsListenServerHost = Exiting != nullptr && Exiting->IsLocalController();

    const FString ExitingPlayerName = bHasMatchPlayerState && !ExitingPlayerState->GetPlayerName().IsEmpty()
        ? ExitingPlayerState->GetPlayerName()
        : GetNameSafe(Exiting);

    const int32 ExitingBaseSlot = bHasMatchPlayerState ? ExitingPlayerState->GetBaseSlot() : INDEX_NONE;

    ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    const EDMMatchPhase PhaseBeforeLogout =
        IsValid(MatchGameState) ? MatchGameState->GetCurrentPhase() : EDMMatchPhase::Waiting;

    const EDMMatchEndReason ReasonBeforeLogout =
        IsValid(MatchGameState) ? MatchGameState->GetMatchEndReason() : EDMMatchEndReason::None;

    /*
     * PlayerDisconnect로 Match를 중단해야 하는 조건:
     *
     * 1. Listen Server Host가 아님
     * 2. BaseSlot을 할당받은 실제 참가자임
     * 3. MatchGameState가 유효함
     * 4. 아직 결과가 확정되지 않음
     * 5. 현재 Phase에서 Aborted 전환이 허용됨
     */
    const bool bShouldAbortForDisconnect = !bIsListenServerHost && bWasAdmittedParticipant && IsValid(MatchGameState)
        && ReasonBeforeLogout == EDMMatchEndReason::None
        && IsPhaseTransitionAllowed(PhaseBeforeLogout, EDMMatchPhase::Aborted);

    if (bShouldAbortForDisconnect)
    {
        /*
         * Super::Logout 전에 Pawn/PlayerState가 살아 있을 때
         * 운반 중 Relic을 먼저 Drop한다.
         */
        if (IsValid(RelicManager) && RelicManager->IsPlayerCarryingRelic(ExitingPlayerState))
        {
            ADMPlayerCharacter* ExitingCharacter =
                Cast<ADMPlayerCharacter>(Exiting != nullptr ? Exiting->GetPawn() : nullptr);

            if (IsValid(ExitingCharacter))
            {
                FString DropFailureReason;

                if (!RelicManager->TryDropCarriedRelic(
                        ExitingPlayerState, ExitingCharacter, EDMRelicDropReason::Disconnect, DropFailureReason
                    ))
                {
                    UE_LOG(
                        LogDMMatchGameMode,
                        Warning,
                        TEXT("Disconnect Relic Drop failed. PlayerName=%s, Reason=%s"),
                        *ExitingPlayerName,
                        *DropFailureReason
                    );
                }
            }
            else
            {
                UE_LOG(
                    LogDMMatchGameMode,
                    Warning,
                    TEXT("Disconnect Relic Drop skipped: PlayerCharacter is unavailable. PlayerName=%s"),
                    *ExitingPlayerName
                );
            }
        }

        /*
         * 이 값은 Logout 정리 전의 서버 생명주기 상태다.
         * 최종 공개 결과는 MatchEndReason이 담당한다.
         */
        ExitingPlayerState->SetLifeState(EDMPlayerLifeState::Disconnected);

        UE_LOG(
            LogDMMatchGameMode,
            Log,
            TEXT("Remote participant logout detected. PlayerName=%s, BaseSlot=%d, Phase=%s"),
            *ExitingPlayerName,
            ExitingBaseSlot,
            *UEnum::GetValueAsString(PhaseBeforeLogout)
        );
    }
    else if (bIsListenServerHost)
    {
        UE_LOG(
            LogDMMatchGameMode,
            Log,
            TEXT("Listen Server host logout detected. Remote clients recover through UDMGameInstance. PlayerName=%s"),
            *ExitingPlayerName
        );
    }
    else if (!bWasAdmittedParticipant)
    {
        UE_LOG(
            LogDMMatchGameMode,
            Log,
            TEXT("Non-admitted Controller logout ignored. Controller=%s"),
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
            TEXT("Participant logout did not alter the locked Match result. PlayerName=%s, Phase=%s, Reason=%s"),
            *ExitingPlayerName,
            *UEnum::GetValueAsString(PhaseBeforeLogout),
            *UEnum::GetValueAsString(ReasonBeforeLogout)
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
        const bool bAbortSucceeded = TryAbortMatch(EDMMatchEndReason::PlayerDisconnect);

        if (!bAbortSucceeded)
        {
            UE_LOG(
                LogDMMatchGameMode,
                Error,
                TEXT("Remote participant logout was detected, but Match abort failed. PlayerName=%s, BaseSlot=%d"),
                *ExitingPlayerName,
                ExitingBaseSlot
            );

            return;
        }

        int32 ConnectedAfterLogout = 0;

        if (UWorld* World = GetWorld())
        {
            for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
            {
                APlayerController* PlayerController = It->Get();

                /*
                 * Logout 처리 중인 Controller는
                 * Iterator에 아직 남아 있을 수 있으므로 제외한다.
                 */
                if (!IsValid(PlayerController) || PlayerController == Exiting)
                {
                    continue;
                }

                const ADMMatchPlayerState* PlayerState = PlayerController->GetPlayerState<ADMMatchPlayerState>();

                /*
                 * BaseSlot을 배정받은 실제 Match 참가자만
                 * 현재 연결 참가자로 계산한다.
                 */
                if (IsValid(PlayerState) && PlayerState->HasAssignedBaseSlot())
                {
                    ++ConnectedAfterLogout;
                }
            }
        }

        UE_LOG(
            LogDMMatchGameMode,
            Log,
            TEXT("Match aborted after remote participant logout. PlayerName=%s, BaseSlot=%d, ConnectedAfterLogout=%d"),
            *ExitingPlayerName,
            ExitingBaseSlot,
            ConnectedAfterLogout
        );
    }

    ResultAcknowledgedControllers.Remove(Cast<APlayerController>(Exiting));

    // 나간 Controller는 이번 프레임까지 목록에 남으므로 다음 Tick에 다시 확인한다.
    if (bResultTravelPending)
    {
        GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::TryCommitPendingResultTravel);
    }
}

#pragma endregion

#pragma region Initial Player Spawn

bool ADMMatchGameMode::UpdatePlayerStartSpot(AController* Player, const FString& Portal, FString& OutErrorMessage)
{
    if (!IsValid(Player))
    {
        OutErrorMessage = TEXT("Player controller is invalid.");

        return false;
    }

    // 초기화에 실패한 매치는 Spawn하지 않으므로 시작 위치를 정하지 않는다.
    if (bMatchInitializationFailed)
    {
        return true;
    }

    ADMMatchPlayerState* MatchPlayerState = Player->GetPlayerState<ADMMatchPlayerState>();

    if (!ensureMsgf(IsValid(MatchPlayerState), TEXT("UpdatePlayerStartSpot failed: MatchPlayerState is unavailable.")))
    {
        OutErrorMessage = TEXT("MatchPlayerState is unavailable.");

        return false;
    }

    MatchPlayerState->SetLifeState(EDMPlayerLifeState::Respawning);

    /*
     * 반드시 부모 UpdatePlayerStartSpot보다 먼저 실행한다.
     *
     * 부모 함수 내부의 FindPlayerStart →
     * ChoosePlayerStart 흐름에서 BaseSlot이 필요하다.
     */
    if (!TryAssignBaseSlot(MatchPlayerState))
    {
        OutErrorMessage = TEXT("No valid BaseSlot is available.");

        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT("UpdatePlayerStartSpot failed: BaseSlot assignment failed. PlayerName=%s"),
            *MatchPlayerState->GetPlayerName()
        );

        return false;
    }

    UE_LOG(
        LogDMMatchGameMode,
        Log,
        TEXT("BaseSlot assigned before PlayerStart selection. PlayerName=%s, BaseSlot=%d"),
        *MatchPlayerState->GetPlayerName(),
        MatchPlayerState->GetBaseSlot()
    );

    const bool bStartSpotUpdated = Super::UpdatePlayerStartSpot(Player, Portal, OutErrorMessage);

    if (!bStartSpotUpdated)
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT("Engine PlayerStart initialization failed. PlayerName=%s, BaseSlot=%d, Reason=%s"),
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

void ADMMatchGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    // 초기화에 실패한 매치에서는 Pawn을 만들지 않는다.
    if (!IsValid(NewPlayer) || bMatchInitializationFailed)
    {
        return;
    }

    ADMMatchPlayerState* MatchPlayerState = NewPlayer->GetPlayerState<ADMMatchPlayerState>();

    if (MatchPlayerState == nullptr || !MatchPlayerState->HasAssignedBaseSlot())
    {
        UE_LOG(
            LogDMMatchGameMode, Error, TEXT("Initial spawn rejected: valid MatchPlayerState or BaseSlot is missing.")
        );
        AbortMatchForInternalFailure(TEXT("Initial player spawn prerequisites are missing."));
        return;
    }

    /*
     * 엔진 기본 흐름:
     * FindPlayerStart → ChoosePlayerStart
     * → Pawn Spawn → Possess
     */
    Super::HandleStartingNewPlayer_Implementation(NewPlayer);

    ADMPlayerCharacter* PlayerCharacter = NewPlayer->GetPawn<ADMPlayerCharacter>();

    const bool bSpawnSucceeded = IsValid(PlayerCharacter) && PlayerCharacter->GetController() == NewPlayer;

    if (!bSpawnSucceeded)
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT("Initial spawn validation failed. PlayerName=%s, BaseSlot=%d"),
            *MatchPlayerState->GetPlayerName(),
            MatchPlayerState->GetBaseSlot()
        );
        AbortMatchForInternalFailure(TEXT("Initial Pawn spawn or Possess validation failed."));
        return;
    }

    MatchPlayerState->SetLifeState(EDMPlayerLifeState::Alive);

    UE_LOG(
        LogDMMatchGameMode,
        Log,
        TEXT("Initial spawn validated. PlayerName=%s, BaseSlot=%d, LifeState=Alive"),
        *MatchPlayerState->GetPlayerName(),
        MatchPlayerState->GetBaseSlot()
    );

    TryFinalizeStartingRoster();
}

AActor* ADMMatchGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
    if (!IsValid(Player))
    {
        return nullptr;
    }

    const ADMMatchPlayerState* MatchPlayerState = Player->GetPlayerState<ADMMatchPlayerState>();

    if (MatchPlayerState == nullptr || !MatchPlayerState->HasAssignedBaseSlot())
    {
        UE_LOG(LogDMMatchGameMode, Error, TEXT("ChoosePlayerStart failed: BaseSlot is not assigned."));

        return nullptr;
    }

    return FindPlayerStartForBaseSlot(MatchPlayerState->GetBaseSlot());
}

#pragma endregion

#pragma region Starting Roster

bool ADMMatchGameMode::IsStartingRosterLocked() const
{
    if (StartingPlayerCountSnapshot != 0)
    {
        return true;
    }

    const ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    return IsValid(MatchGameState) && MatchGameState->GetCurrentPhase() != EDMMatchPhase::Waiting;
}

bool ADMMatchGameMode::TryAssignBaseSlot(ADMMatchPlayerState* MatchPlayerState)
{
    if (!IsValid(MatchPlayerState))
    {
        return false;
    }

    if (MatchPlayerState->HasAssignedBaseSlot())
    {
        return true;
    }

    const int32 AvailableBaseSlot = FindFirstAvailableBaseSlot();

    if (AvailableBaseSlot == INDEX_NONE)
    {
        return false;
    }

    MatchPlayerState->SetBaseSlot(AvailableBaseSlot);

    return MatchPlayerState->GetBaseSlot() == AvailableBaseSlot;
}

int32 ADMMatchGameMode::FindFirstAvailableBaseSlot() const
{
    if (!IsValid(MatchRuleData))
    {
        return INDEX_NONE;
    }

    const ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    if (MatchGameState == nullptr)
    {
        return INDEX_NONE;
    }

    for (int32 CandidateSlot = 0; CandidateSlot < MatchRuleData->MaxPlayerCount; ++CandidateSlot)
    {
        bool bSlotAlreadyUsed = false;

        for (APlayerState* PlayerState : MatchGameState->PlayerArray)
        {
            const ADMMatchPlayerState* MatchPlayerState = Cast<ADMMatchPlayerState>(PlayerState);

            if (MatchPlayerState != nullptr && MatchPlayerState->GetBaseSlot() == CandidateSlot)
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

APlayerStart* ADMMatchGameMode::FindPlayerStartForBaseSlot(const int32 BaseSlot) const
{
    if (!DMMatch::IsValidBaseSlot(BaseSlot))
    {
        return nullptr;
    }

    UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return nullptr;
    }

    const FName RequiredPlayerStartTag(*FString::Printf(TEXT("DM_Base_%d"), BaseSlot));

    APlayerStart* FoundPlayerStart = nullptr;

    for (TActorIterator<APlayerStart> Iterator(World); Iterator; ++Iterator)
    {
        APlayerStart* PlayerStart = *Iterator;

        if (PlayerStart->PlayerStartTag != RequiredPlayerStartTag)
        {
            continue;
        }

        if (!ensureMsgf(
                FoundPlayerStart == nullptr,
                TEXT("Duplicate PlayerStartTag found. Tag=%s"),
                *RequiredPlayerStartTag.ToString()
            ))
        {
            return nullptr;
        }

        FoundPlayerStart = PlayerStart;
    }

    ensureMsgf(
        FoundPlayerStart != nullptr,
        TEXT("Required PlayerStart was not found. Tag=%s"),
        *RequiredPlayerStartTag.ToString()
    );

    return FoundPlayerStart;
}

bool ADMMatchGameMode::IsStartingRosterComplete(FString& OutFailureReason) const
{
    OutFailureReason.Reset();

    if (ExpectedPlayerCount <= 0)
    {
        OutFailureReason = TEXT("ExpectedPlayerCount is invalid.");

        return false;
    }

    const ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    if (MatchGameState == nullptr)
    {
        OutFailureReason = TEXT("MatchGameState is unavailable.");

        return false;
    }

    if (MatchGameState->PlayerArray.Num() != ExpectedPlayerCount)
    {
        OutFailureReason = FString::Printf(
            TEXT("Waiting for players. Current=%d, Expected=%d"), MatchGameState->PlayerArray.Num(), ExpectedPlayerCount
        );

        return false;
    }

    TSet<int32> AssignedBaseSlots;

    for (APlayerState* PlayerState : MatchGameState->PlayerArray)
    {
        const ADMMatchPlayerState* MatchPlayerState = Cast<ADMMatchPlayerState>(PlayerState);

        if (MatchPlayerState == nullptr)
        {
            OutFailureReason = TEXT("Invalid MatchPlayerState.");

            return false;
        }

        const int32 BaseSlot = MatchPlayerState->GetBaseSlot();

        if (!DMMatch::IsValidBaseSlot(BaseSlot))
        {
            OutFailureReason = TEXT("A player has no valid BaseSlot.");

            return false;
        }

        if (AssignedBaseSlots.Contains(BaseSlot))
        {
            OutFailureReason = TEXT("Duplicate BaseSlot detected.");

            return false;
        }

        AssignedBaseSlots.Add(BaseSlot);

        if (MatchPlayerState->GetLifeState() != EDMPlayerLifeState::Alive)
        {
            OutFailureReason =
                FString::Printf(TEXT("Player is not Alive. PlayerName=%s"), *MatchPlayerState->GetPlayerName());

            return false;
        }

        const ADMPlayerCharacter* PlayerCharacter = MatchPlayerState->GetPawn<ADMPlayerCharacter>();

        if (!IsValid(PlayerCharacter) || PlayerCharacter->GetController() == nullptr
            || PlayerCharacter->GetPlayerState<ADMMatchPlayerState>() != MatchPlayerState)
        {
            OutFailureReason = FString::Printf(
                TEXT("Pawn or Possess validation failed. PlayerName=%s"), *MatchPlayerState->GetPlayerName()
            );

            return false;
        }
    }

    return true;
}

#pragma endregion

#pragma region Starting Roster Finalization

void ADMMatchGameMode::HandleStartingRosterTimeout()
{
    const ADMMatchGameState* State = GetGameState<ADMMatchGameState>();
    if (!HasAuthority() || StartingPlayerCountSnapshot != 0)
    {
        return;
    }
    if (!IsValid(State))
    {
        AbortMatchForInternalFailure(TEXT("Starting roster timeout: MatchGameState is unavailable."));
        return;
    }
    if (State->GetCurrentPhase() != EDMMatchPhase::Waiting)
    {
        return;
    }

    FString Reason;
    IsStartingRosterComplete(Reason);
    AbortMatchForInternalFailure(FString::Printf(
        TEXT("Starting roster timed out after %.1fs. Reason=%s"), StartingRosterTimeoutSeconds, *Reason));
}

void ADMMatchGameMode::FailStartingRosterFinalization(const FString& Diagnostic)
{
    // 부분 초기화된 서버 시스템을 역순으로 정리한다.
    ShutdownRandomEventSystem();
    ShutdownQuestSystem();
    if (IsValid(RelicManager))
    {
        RelicManager->Destroy();
        RelicManager = nullptr;
    }
    if (IsValid(NPCPopulationDirector))
    {
        NPCPopulationDirector->ShutdownPopulation();
        NPCPopulationDirector = nullptr;
    }
    AbortMatchForInternalFailure(Diagnostic);
}

void ADMMatchGameMode::TryFinalizeStartingRoster()
{
    if (!HasAuthority() || StartingPlayerCountSnapshot != 0 || bMatchInitializationFailed)
    {
        return;
    }

    const ADMMatchGameState* CurrentState = GetGameState<ADMMatchGameState>();
    if (!IsValid(CurrentState))
    {
        FailStartingRosterFinalization(TEXT("Starting roster finalization failed: GameState is missing."));
        return;
    }
    if (CurrentState->GetCurrentPhase() != EDMMatchPhase::Waiting
        || CurrentState->GetMatchEndReason() != EDMMatchEndReason::None)
    {
        return;
    }

    FString Reason;
    if (!IsStartingRosterComplete(Reason))
    {
        UE_LOG(LogDMMatchGameMode, Verbose, TEXT("Starting roster is not complete: %s"), *Reason);
        return;
    }

    FString RespawnReason;
    if (!IsValid(RespawnCoordinator)
        || !RespawnCoordinator->ValidateRespawnPointsForStartingRoster(RespawnReason))
    {
        FailStartingRosterFinalization(FString::Printf(TEXT("RespawnPoint validation failed: %s"), *RespawnReason));
        return;
    }

    ADMMatchGameState* State = GetGameState<ADMMatchGameState>();
    if (!IsValid(State) || !IsValid(MatchRuleData))
    {
        FailStartingRosterFinalization(TEXT("GameState or MatchRuleData became invalid."));
        return;
    }

    StartingPlayerCountSnapshot = State->PlayerArray.Num();
    ResolvedNPCMultiplierSnapshot = MatchRuleData->DefaultNPCMultiplier;
    ResolvedNPCCountSnapshot = StartingPlayerCountSnapshot * ResolvedNPCMultiplierSnapshot;
    bEnableEliminationVictorySnapshot = MatchRuleData->bEnableEliminationVictory;

    if (!InitializeNPCPopulation())
    {
        FailStartingRosterFinalization(TEXT("NPC Population initialization failed."));
        return;
    }
    if (!InitializeRelicSystem())
    {
        FailStartingRosterFinalization(TEXT("Relic system initialization failed."));
        return;
    }
    if (!InitializeQuestSystem())
    {
        FailStartingRosterFinalization(TEXT("Quest system initialization failed."));
        return;
    }
    if (!InitializeRandomEventSystem())
    {
        FailStartingRosterFinalization(TEXT("RandomEvent system initialization failed."));
        return;
    }

    State->SetStartingPlayerCount(StartingPlayerCountSnapshot);
    if (State->GetStartingPlayerCount() != StartingPlayerCountSnapshot)
    {
        FailStartingRosterFinalization(FString::Printf(
            TEXT("StartingPlayerCount projection failed. Snapshot=%d, Actual=%d"),
            StartingPlayerCountSnapshot, State->GetStartingPlayerCount()));
        return;
    }

    UE_LOG(LogDMMatchGameMode, Log,
        TEXT("Starting roster frozen. Players=%d, NPCMultiplier=%d, NPCCount=%d, TotalRelics=%d, RequiredRelics=%d, EliminationVictory=%s"),
        StartingPlayerCountSnapshot, ResolvedNPCMultiplierSnapshot, ResolvedNPCCountSnapshot,
        TotalRelicCountSnapshot, RequiredRelicCountSnapshot,
        bEnableEliminationVictorySnapshot ? TEXT("true") : TEXT("false"));

    GetWorldTimerManager().ClearTimer(StartingRosterTimeoutHandle);
    StartPreMatchTutorialGate();
}

#pragma endregion

#pragma region Skill Lifecycle Helpers

void ADMMatchGameMode::CleanupAllSkillRuntimeForTerminalMatch()
{
    ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    if (!IsValid(MatchGameState))
    {
        return;
    }

    for (ADMMatchPlayerState* MatchPlayerState : DMGameplay::GetMatchPlayerStates(MatchGameState))
    {
        UDMSkillComponent* SkillComponent = MatchPlayerState->GetSkillComponent();

        if (!IsValid(SkillComponent))
        {
            continue;
        }

        SkillComponent->CleanupTransientSkillRuntimeForMatchEnd();
    }
}

#pragma endregion

#pragma region Relic Initialization

bool ADMMatchGameMode::ConfigureBaseZoneActivity(const TArray<ADMMatchPlayerState*>& MatchPlayers)
{
    TSet<int32> ParticipatingBaseSlots;

    for (const ADMMatchPlayerState* PlayerState : MatchPlayers)
    {
        if (!ensureMsgf(
                IsValid(PlayerState) && PlayerState->HasAssignedBaseSlot(),
                TEXT("ConfigureBaseZoneActivity failed: invalid PlayerState or BaseSlot.")
            ))
        {
            return false;
        }

        const int32 BaseSlot = PlayerState->GetBaseSlot();

        if (!ensureMsgf(
                DMMatch::IsValidBaseSlot(BaseSlot),
                TEXT("ConfigureBaseZoneActivity failed: invalid BaseSlot=%d"),
                BaseSlot
            ))
        {
            return false;
        }

        if (!ensureMsgf(
                !ParticipatingBaseSlots.Contains(BaseSlot),
                TEXT("ConfigureBaseZoneActivity failed: duplicate participating BaseSlot=%d"),
                BaseSlot
            ))
        {
            return false;
        }

        ParticipatingBaseSlots.Add(BaseSlot);
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return false;
    }

    TSet<int32> FoundBaseSlots;

    for (TActorIterator<ADMBaseZone> It(World); It; ++It)
    {
        ADMBaseZone* BaseZone = *It;

        if (!IsValid(BaseZone))
        {
            continue;
        }

        const int32 BaseSlot = BaseZone->GetBaseSlot();

        if (!ensureMsgf(DMMatch::IsValidBaseSlot(BaseSlot), TEXT("Invalid BaseZone BaseSlot=%d"), BaseSlot))
        {
            return false;
        }

        if (!ensureMsgf(!FoundBaseSlots.Contains(BaseSlot), TEXT("Duplicate BaseZone. BaseSlot=%d"), BaseSlot))
        {
            return false;
        }

        FoundBaseSlots.Add(BaseSlot);

        const bool bShouldBeActive = ParticipatingBaseSlots.Contains(BaseSlot);

        BaseZone->SetActivityState(bShouldBeActive ? EDMBaseActivityState::Active : EDMBaseActivityState::Inactive);

        UE_LOG(
            LogDMMatchGameMode,
            Log,
            TEXT("BaseZone configured. BaseSlot=%d, State=%s"),
            BaseSlot,
            bShouldBeActive ? TEXT("Active") : TEXT("Inactive")
        );
    }

    for (const int32 ParticipatingSlot : ParticipatingBaseSlots)
    {
        if (!ensureMsgf(
                FoundBaseSlots.Contains(ParticipatingSlot),
                TEXT("Required BaseZone missing. BaseSlot=%d"),
                ParticipatingSlot
            ))
        {
            return false;
        }
    }

    return true;
}

bool ADMMatchGameMode::InitializeRelicSystem()
{
    if (!ensureMsgf(
            DMMatch::IsValidPlayerCount(StartingPlayerCountSnapshot),
            TEXT("InitializeRelicSystem failed: invalid StartingPlayerCountSnapshot=%d"),
            StartingPlayerCountSnapshot
        ))
    {
        return false;
    }

    if (!ensureMsgf(IsValid(RelicRuleData), TEXT("InitializeRelicSystem failed: RelicRuleData is unavailable.")))
    {
        return false;
    }

    if (!ensureMsgf(RelicClass, TEXT("InitializeRelicSystem failed: RelicClass is unavailable.")))
    {
        return false;
    }

    if (IsValid(RelicManager))
    {
        UE_LOG(LogDMMatchGameMode, Warning, TEXT("InitializeRelicSystem rejected: RelicManager already exists."));

        return false;
    }

    int32 ResolvedTotalRelicCount = INDEX_NONE;
    int32 ResolvedRequiredRelicCount = INDEX_NONE;

    if (!ensureMsgf(
            RelicRuleData->TryGetRelicCountRule(
                StartingPlayerCountSnapshot, ResolvedTotalRelicCount, ResolvedRequiredRelicCount
            ),
            TEXT("InitializeRelicSystem failed: Relic count rule could not be resolved. Players=%d"),
            StartingPlayerCountSnapshot
        ))
    {
        return false;
    }

    ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    if (!ensureMsgf(IsValid(MatchGameState), TEXT("InitializeRelicSystem failed: MatchGameState is unavailable.")))
    {
        return false;
    }

    TArray<ADMMatchPlayerState*> MatchPlayers;
    MatchPlayers.Reserve(StartingPlayerCountSnapshot);

    for (APlayerState* PlayerState : MatchGameState->PlayerArray)
    {
        ADMMatchPlayerState* MatchPlayerState = Cast<ADMMatchPlayerState>(PlayerState);

        if (!ensureMsgf(IsValid(MatchPlayerState), TEXT("InitializeRelicSystem failed: invalid MatchPlayerState.")))
        {
            return false;
        }

        MatchPlayers.Add(MatchPlayerState);
    }

    if (!ensureMsgf(
            MatchPlayers.Num() == StartingPlayerCountSnapshot,
            TEXT("InitializeRelicSystem failed: player count mismatch. Snapshot=%d, PlayerStates=%d"),
            StartingPlayerCountSnapshot,
            MatchPlayers.Num()
        ))
    {
        return false;
    }

    if (!ConfigureBaseZoneActivity(MatchPlayers))
    {
        UE_LOG(LogDMMatchGameMode, Error, TEXT("InitializeRelicSystem failed: BaseZone configuration failed."));

        return false;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return false;
    }

    FActorSpawnParameters SpawnParameters;

    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    RelicManager =
        World->SpawnActor<ADMRelicManager>(ADMRelicManager::StaticClass(), FTransform::Identity, SpawnParameters);

    if (!IsValid(RelicManager))
    {
        UE_LOG(LogDMMatchGameMode, Error, TEXT("InitializeRelicSystem failed: RelicManager spawn failed."));

        return false;
    }

    if (!RelicManager->InitializeMatch(
            StartingPlayerCountSnapshot, ResolvedTotalRelicCount, RelicRuleData, RelicClass, MatchPlayers
        ))
    {
        UE_LOG(LogDMMatchGameMode, Error, TEXT("InitializeRelicSystem failed: RelicManager initialization failed."));

        RelicManager->Destroy();
        RelicManager = nullptr;

        return false;
    }

    TotalRelicCountSnapshot = ResolvedTotalRelicCount;

    RequiredRelicCountSnapshot = ResolvedRequiredRelicCount;

    /*
     * 클라이언트 UI에서도 승리에 필요한 유물 개수는
     * 공개 정보이므로 GameState Read Model에 전달한다.
     */
    MatchGameState->SetRequiredRelicCount(RequiredRelicCountSnapshot);

    if (!ensureMsgf(
            MatchGameState->GetRequiredRelicCount() == RequiredRelicCountSnapshot,
            TEXT("InitializeRelicSystem failed: RequiredRelicCount projection failed. Snapshot=%d, GameState=%d"),
            RequiredRelicCountSnapshot,
            MatchGameState->GetRequiredRelicCount()
        ))
    {
        RelicManager->Destroy();
        RelicManager = nullptr;

        TotalRelicCountSnapshot = 0;
        RequiredRelicCountSnapshot = 0;

        return false;
    }

    /*
     * RelicManager는 Deposit Commit까지만 담당하고,
     * 그 이후 Match 결과 판정은 GameMode가 담당한다.
     */
    RelicManager->OnRelicDepositCommitted().BindUObject(this, &ADMMatchGameMode::HandleRelicDepositCommitted);

    RelicManager->OnRelicStolen().BindUObject(this, &ADMMatchGameMode::HandleRelicStolen);

    UE_LOG(
        LogDMMatchGameMode,
        Log,
        TEXT("Relic system initialized. Players=%d, TotalRelics=%d, RequiredRelics=%d"),
        StartingPlayerCountSnapshot,
        TotalRelicCountSnapshot,
        RequiredRelicCountSnapshot
    );

    return true;
}

#pragma endregion

#pragma region NPC Population Initialization

bool ADMMatchGameMode::InitializeNPCPopulation()
{
    if (!ensureMsgf(
            ResolvedNPCCountSnapshot > 0,
            TEXT("InitializeNPCPopulation failed: invalid ResolvedNPCCountSnapshot=%d"),
            ResolvedNPCCountSnapshot
        ))
    {
        return false;
    }

    if (!ensureMsgf(NPCClass, TEXT("InitializeNPCPopulation failed: NPCClass is unavailable.")))
    {
        return false;
    }

    if (IsValid(NPCPopulationDirector))
    {
        UE_LOG(LogDMMatchGameMode, Warning, TEXT("InitializeNPCPopulation rejected: director already exists."));

        return false;
    }

    NPCPopulationDirector = NewObject<UDMNPCPopulationDirector>(this);

    if (!IsValid(NPCPopulationDirector))
    {
        UE_LOG(LogDMMatchGameMode, Error, TEXT("InitializeNPCPopulation failed: director creation failed."));

        return false;
    }

    if (!NPCPopulationDirector->InitializePopulation(ResolvedNPCCountSnapshot, NPCClass))
    {
        UE_LOG(LogDMMatchGameMode, Error, TEXT("InitializeNPCPopulation failed: population initialization failed."));

        NPCPopulationDirector->ShutdownPopulation();

        NPCPopulationDirector = nullptr;

        return false;
    }

    const int32 RegisteredNPCCount = NPCPopulationDirector->GetRegisteredNPCCount();

    UE_LOG(
        LogDMMatchGameMode,
        Log,
        TEXT("NPC population initialized. Snapshot=%d, Registered=%d"),
        ResolvedNPCCountSnapshot,
        RegisteredNPCCount
    );

    return RegisteredNPCCount == ResolvedNPCCountSnapshot;
}

#pragma endregion

#pragma region Quest Initialization

bool ADMMatchGameMode::InitializeQuestSystem()
{
    if (!ensureMsgf(
            IsValid(MatchRuleData) && IsValid(QuestDefinitionTable),
            TEXT("InitializeQuestSystem failed: Quest configuration unavailable.")
        ))
    {
        return false;
    }

    if (IsValid(QuestDirector))
    {
        UE_LOG(LogDMMatchGameMode, Warning, TEXT("InitializeQuestSystem rejected: QuestDirector already exists."));

        return false;
    }

    QuestDirector = NewObject<UDMQuestDirector>(this);

    if (!IsValid(QuestDirector))
    {
        UE_LOG(LogDMMatchGameMode, Error, TEXT("InitializeQuestSystem failed: QuestDirector creation failed."));

        return false;
    }

    if (!QuestDirector->Initialize(
            this,
            MatchRuleData,
            QuestDefinitionTable,
            FDMQuestPublicStatePublisher::CreateUObject(this, &ThisClass::PublishQuestPublicState)
        ))
    {
        UE_LOG(LogDMMatchGameMode, Error, TEXT("InitializeQuestSystem failed: QuestDirector initialization failed."));

        QuestDirector->Shutdown();

        QuestDirector = nullptr;

        return false;
    }

    UE_LOG(LogDMMatchGameMode, Log, TEXT("Quest system initialized."));

    return true;
}

void ADMMatchGameMode::ShutdownQuestSystem()
{
    if (!IsValid(QuestDirector))
    {
        /*
         * Director가 없어도 Public Projection은
         * stale 상태가 남지 않도록 clear한다.
         */
        PublishQuestPublicState(FDMQuestRuntimeState());

        return;
    }

    QuestDirector->Shutdown();

    QuestDirector = nullptr;
}

bool ADMMatchGameMode::PublishQuestPublicState(const FDMQuestRuntimeState& NewQuestPublicState)
{
    ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    if (!IsValid(MatchGameState))
    {
        return false;
    }

    MatchGameState->SetQuestPublicState(NewQuestPublicState);

    const FDMQuestRuntimeState AppliedState = MatchGameState->GetQuestPublicState();

    return AppliedState.RunId == NewQuestPublicState.RunId && AppliedState.QuestTag == NewQuestPublicState.QuestTag
        && FMath::IsNearlyEqual(AppliedState.StartServerTime, NewQuestPublicState.StartServerTime)
        && FMath::IsNearlyEqual(AppliedState.EndServerTime, NewQuestPublicState.EndServerTime);
}

#pragma endregion

#pragma region Random Event Initialization

bool ADMMatchGameMode::InitializeRandomEventSystem()
{
    if (!ensureMsgf(
            IsValid(MatchRuleData) && IsValid(RandomEventSetData),
            TEXT("InitializeRandomEventSystem failed: configuration unavailable.")
        ))
    {
        return false;
    }

    if (IsValid(RandomEventDirector))
    {
        UE_LOG(LogDMMatchGameMode, Warning, TEXT("InitializeRandomEventSystem rejected: director already exists."));

        return false;
    }

    RandomEventDirector = NewObject<UDMRandomEventDirector>(this);

    if (!IsValid(RandomEventDirector))
    {
        UE_LOG(LogDMMatchGameMode, Error, TEXT("InitializeRandomEventSystem failed: director creation failed."));

        return false;
    }

    if (!RandomEventDirector->Initialize(
            this,
            MatchRuleData,
            RandomEventSetData,
            FDMRandomEventPublicStatePublisher::CreateUObject(this, &ThisClass::PublishRandomEventPublicState)
        ))
    {
        UE_LOG(LogDMMatchGameMode, Error, TEXT("InitializeRandomEventSystem failed: director initialization failed."));

        RandomEventDirector->Shutdown();

        RandomEventDirector = nullptr;

        return false;
    }

    UE_LOG(LogDMMatchGameMode, Log, TEXT("Random Event system initialized."));

    return true;
}

void ADMMatchGameMode::ShutdownRandomEventSystem()
{
    if (!IsValid(RandomEventDirector))
    {
        PublishRandomEventPublicState(FDMRandomEventRuntimeState());

        return;
    }

    RandomEventDirector->Shutdown();

    RandomEventDirector = nullptr;
}

bool ADMMatchGameMode::PublishRandomEventPublicState(const FDMRandomEventRuntimeState& NewRandomEventPublicState)
{
    ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    if (!IsValid(MatchGameState))
    {
        return false;
    }

    MatchGameState->SetRandomEventPublicState(NewRandomEventPublicState);

    const FDMRandomEventRuntimeState AppliedState = MatchGameState->GetRandomEventPublicState();

    return AppliedState.EventTag == NewRandomEventPublicState.EventTag
        && FMath::IsNearlyEqual(AppliedState.WarningStartServerTime, NewRandomEventPublicState.WarningStartServerTime)
        && FMath::IsNearlyEqual(AppliedState.StartServerTime, NewRandomEventPublicState.StartServerTime)
        && FMath::IsNearlyEqual(AppliedState.EndServerTime, NewRandomEventPublicState.EndServerTime);
}

#pragma endregion

#pragma region NPC Reaction Bridge

void ADMMatchGameMode::RequestNPCWrongTargetReaction(
    const FVector& AttackOrigin, const TArray<ADMNPCCharacter*>& HitNPCTargets
)
{
    if (AttackOrigin.ContainsNaN() || HitNPCTargets.Num() <= 0)
    {
        return;
    }

    if (!IsValid(NPCPopulationDirector) || !IsValid(MatchRuleData))
    {
        UE_LOG(
            LogDMMatchGameMode,
            Warning,
            TEXT("NPC wrong-target reaction request rejected: Director or MatchRuleData is unavailable.")
        );

        return;
    }

    NPCPopulationDirector->HandleWrongTargetAttack(
        AttackOrigin,
        HitNPCTargets,
        MatchRuleData->NPCWrongTargetReactionRadius,
        MatchRuleData->MaxNearbyNPCReactionResponders,
        MatchRuleData->NPCFleeDistance
    );
}

void ADMMatchGameMode::RequestNPCPanic()
{
    if (!IsValid(NPCPopulationDirector))
    {
        UE_LOG(LogDMMatchGameMode, Warning, TEXT("NPC Panic request rejected: NPCPopulationDirector is unavailable."));

        return;
    }

    NPCPopulationDirector->HandlePanicEvent();
}

#pragma endregion

#pragma region Player Death

bool ADMMatchGameMode::TryCommitPlayerDeath(ADMPlayerCharacter* VictimCharacter, ADMPlayerCharacter* KillerCharacter)
{
    ADMMatchPlayerState* VictimPlayerState = nullptr;

    if (!CanCommitPlayerDeath(VictimCharacter, VictimPlayerState))
    {
        return false;
    }

    // 사망 위치는 관전 시작 지점으로 사용한다.
    const FVector DeathLocation = VictimCharacter->GetActorLocation();

    /*
     * 처리 순서가 결과에 영향을 준다.
     *
     * 1. LifeState를 Dead로 확정
     * 2. 상호작용을 정리한 뒤 소지 유물 Drop
     * 3. 전투·행동을 정리한 뒤 이동 비활성화 (Cleanup이 MovementMode를 복원할 수 있다)
     * 4. 래그돌 표현
     * 5. Match 결과 평가
     * 6. 경기가 계속되면 Respawn·관전 시작
     */
    CommitDeathState(VictimPlayerState, KillerCharacter);

    CleanupDeathInteractionAndRelic(VictimCharacter, VictimPlayerState);

    CleanupDeathCombatAndMovement(VictimCharacter);

    StartDeathPresentation(VictimCharacter);

    UE_LOG(
        LogDMMatchGameMode,
        Log,
        TEXT("Player Death committed. Victim=%s, Killer=%s, BaseSlot=%d"),
        *GetNameSafe(VictimCharacter),
        *GetNameSafe(KillerCharacter),
        VictimPlayerState->GetBaseSlot()
    );

    // Death 사건에서도 중앙 Match Result를 평가한다(RelicVictory → EliminationVictory).
    EvaluateMatchResult();

    // 매치가 종료되지 않았을 때만 주변 NPC가 실제 사망 위치에 반응한다.
    // FakeDeath는 TryCommitPlayerDeath() 경로를 사용하지 않는다.
    const ADMMatchGameState* UpdatedMatchGameState = GetGameState<ADMMatchGameState>();
    if (IsValid(UpdatedMatchGameState) && IsValid(NPCPopulationDirector) && IsValid(MatchRuleData)
        && (UpdatedMatchGameState->GetCurrentPhase() == EDMMatchPhase::Playing
            || UpdatedMatchGameState->GetCurrentPhase() == EDMMatchPhase::Overtime))
    {
        NPCPopulationDirector->HandleNearbyPlayerDeath(
            DeathLocation,
            MatchRuleData->NPCDeathMockReactionRadius,
            MatchRuleData->MaxNearbyNPCDeathMockResponders,
            MatchRuleData->NPCDeathMockDuration
        );
    }

    StartPostDeathLifecycle(VictimCharacter, VictimPlayerState, KillerCharacter, DeathLocation);

    return true;
}

bool ADMMatchGameMode::CanCommitPlayerDeath(
    ADMPlayerCharacter* VictimCharacter, ADMMatchPlayerState*& OutVictimPlayerState
) const
{
    OutVictimPlayerState = nullptr;

    if (!IsValid(VictimCharacter))
    {
        UE_LOG(LogDMMatchGameMode, Verbose, TEXT("Death Commit rejected: VictimCharacter is invalid."));

        return false;
    }

    /*
     * 공격 Notify가 Match 종료 직후 도착하는
     * 경우까지 서버에서 최종 차단한다.
     */
    const ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    if (!IsValid(MatchGameState))
    {
        return false;
    }

    const EDMMatchPhase CurrentPhase = MatchGameState->GetCurrentPhase();

    if (CurrentPhase != EDMMatchPhase::Playing && CurrentPhase != EDMMatchPhase::Overtime)
    {
        UE_LOG(
            LogDMMatchGameMode,
            Verbose,
            TEXT("Death Commit rejected: invalid Match Phase. Victim=%s, Phase=%s"),
            *GetNameSafe(VictimCharacter),
            *UEnum::GetValueAsString(CurrentPhase)
        );

        return false;
    }

    ADMMatchPlayerState* VictimPlayerState = VictimCharacter->GetPlayerState<ADMMatchPlayerState>();

    if (!IsValid(VictimPlayerState))
    {
        UE_LOG(
            LogDMMatchGameMode,
            Verbose,
            TEXT("Death Commit rejected: Victim PlayerState is invalid. Victim=%s"),
            *GetNameSafe(VictimCharacter)
        );

        return false;
    }

    /*
     * 최종 중복 Death Guard.
     *
     * 같은 서버 프레임에 여러 공격이
     * 들어와도 첫 Commit만 성공한다.
     */
    if (VictimPlayerState->GetLifeState() != EDMPlayerLifeState::Alive)
    {
        UE_LOG(
            LogDMMatchGameMode,
            Verbose,
            TEXT("Death Commit rejected: Victim is no longer Alive. Victim=%s, LifeState=%s"),
            *GetNameSafe(VictimCharacter),
            *UEnum::GetValueAsString(VictimPlayerState->GetLifeState())
        );

        return false;
    }

    OutVictimPlayerState = VictimPlayerState;

    return true;
}

void ADMMatchGameMode::CommitDeathState(ADMMatchPlayerState* VictimPlayerState, ADMPlayerCharacter* KillerCharacter)
{
    ADMMatchPlayerState* KillerPlayerState =
        IsValid(KillerCharacter) ? KillerCharacter->GetPlayerState<ADMMatchPlayerState>() : nullptr;

    if (IsValid(KillerPlayerState) && KillerPlayerState != VictimPlayerState)
    {
        KillerPlayerState->IncrementKillCount();
    }

    // 여기부터 이 Player의 authoritative LifeState는 Dead다.
    VictimPlayerState->SetLifeState(EDMPlayerLifeState::Dead);

    VictimPlayerState->ForceNetUpdate();
}

void ADMMatchGameMode::CleanupDeathInteractionAndRelic(
    ADMPlayerCharacter* VictimCharacter, ADMMatchPlayerState* VictimPlayerState
)
{
    /*
     * Interaction Cleanup.
     *
     * 반드시 Drop 없는 Death 전용 Cleanup을
     * 사용한다.
     */
    if (UDMInteractionComponent* InteractionComponent =
            VictimCharacter->FindComponentByClass<UDMInteractionComponent>())
    {
        InteractionComponent->ForceCleanupForDeath();
    }

    /*
     * Interaction Session / Reservation을 먼저 정리한 뒤
     * 실제 Carried Relic Drop은
     * RelicManager에서 정확히 한 번 Commit한다.
     */
    if (IsValid(RelicManager) && RelicManager->IsPlayerCarryingRelic(VictimPlayerState))
    {
        FString DropFailureReason;

        const bool bRelicDropped = RelicManager->TryDropCarriedRelic(
            VictimPlayerState, VictimCharacter, EDMRelicDropReason::Death, DropFailureReason
        );

        if (bRelicDropped)
        {
            UE_LOG(
                LogDMMatchGameMode, Log, TEXT("Death Relic Drop committed. Victim=%s"), *GetNameSafe(VictimCharacter)
            );
        }
        else
        {
            UE_LOG(
                LogDMMatchGameMode,
                Error,
                TEXT("Death Relic Drop failed. Victim=%s, Reason=%s"),
                *GetNameSafe(VictimCharacter),
                *DropFailureReason
            );
        }
    }
    else if (!IsValid(RelicManager))
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT("Death Relic Drop check failed: RelicManager is unavailable. Victim=%s"),
            *GetNameSafe(VictimCharacter)
        );
    }
}

void ADMMatchGameMode::CleanupDeathCombatAndMovement(ADMPlayerCharacter* VictimCharacter)
{
    /*
     * Attack / Defense / Combat Timer /
     * Montage 정리.
     */
    if (UDMCombatComponent* CombatComponent = VictimCharacter->FindComponentByClass<UDMCombatComponent>())
    {
        CombatComponent->HandleOwnerDeath();
    }

    /*
     * Interaction/Combat 외의
     * FakeDeath / SkillCast 등의 Action까지
     * Death 이후 남지 않도록 마지막으로
     * ActionState를 None으로 정리한다.
     */
    if (UDMCharacterStateComponent* StateComponent = VictimCharacter->GetCharacterStateComponent())
    {
        const EDMActionState CurrentAction = StateComponent->GetActionState();

        if (CurrentAction != EDMActionState::None)
        {
            StateComponent->EndAction(CurrentAction);
        }
    }

    /*
     * Interaction Cleanup 과정에서
     * 기존 MovementMode가 복원될 수 있으므로
     * Movement Disable은 모든 Cleanup 뒤에
     * 최종적으로 실행한다.
     */
    VictimCharacter->ClearSprintRequest();

    VictimCharacter->StopJumping();

    if (UCharacterMovementComponent* MovementComponent = VictimCharacter->GetCharacterMovement())
    {
        MovementComponent->StopMovementImmediately();

        MovementComponent->DisableMovement();
    }
}

void ADMMatchGameMode::StartDeathPresentation(ADMPlayerCharacter* VictimCharacter)
{
    /*
     * Gameplay Death는 이미 PlayerState에 Commit되었다.
     * JellyPhysicsComponent는 물리 표현만 담당한다.
     */
    UDMJellyPhysicsComponent* PhysicsComponent = VictimCharacter->GetJellyPhysicsComponent();

    if (PhysicsComponent != nullptr)
    {
        if (!PhysicsComponent->StartDeathRagdollPresentation())
        {
            UE_LOG(
                LogDMMatchGameMode,
                Warning,
                TEXT("Death Ragdoll presentation request failed. Victim=%s"),
                *GetNameSafe(VictimCharacter)
            );
        }
    }
    else
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT("Death Ragdoll presentation failed: JellyPhysicsComponent is null. Victim=%s"),
            *GetNameSafe(VictimCharacter)
        );
    }

    VictimCharacter->ForceNetUpdate();
}

void ADMMatchGameMode::StartPostDeathLifecycle(
    ADMPlayerCharacter* VictimCharacter,
    ADMMatchPlayerState* VictimPlayerState,
    ADMPlayerCharacter* KillerCharacter,
    const FVector& DeathLocation
)
{
    ADMMatchGameState* UpdatedMatchGameState = GetGameState<ADMMatchGameState>();

    const bool bMatchContinues = IsValid(UpdatedMatchGameState)
        && UpdatedMatchGameState->GetMatchEndReason() == EDMMatchEndReason::None
        && (UpdatedMatchGameState->GetCurrentPhase() == EDMMatchPhase::Playing
            || UpdatedMatchGameState->GetCurrentPhase() == EDMMatchPhase::Overtime);

    if (bMatchContinues)
    {
        /*
         * Local Spectator Sequence와 Server Respawn Timer가
         * 가능한 한 동일한 기준 시점을 공유하도록
         * Respawn Lifecycle 시작 직전에 ServerTime을 기록한다.
         */
        const double SpectatorStartServerTime = UpdatedMatchGameState->GetServerWorldTimeSeconds();

        const bool bRespawnLifecycleStarted =
            IsValid(RespawnCoordinator) && RespawnCoordinator->StartRespawnLifecycle(VictimPlayerState);

        if (!bRespawnLifecycleStarted)
        {
            // 30초 관전 타이머 자체를 시작하지 못한 경우에는 유효하지 않은
            // Respawn 작업을 반복 예약하지 않고 이미 존재하는 종료 경로로 전환한다.
            AbortMatchForInternalFailure(FString::Printf(
                TEXT("Death committed but Respawn Lifecycle failed to start. Victim=%s"),
                *GetNameSafe(VictimCharacter)
            ));
            return;
        }

        ADMMatchPlayerController* VictimController =
            Cast<ADMMatchPlayerController>(VictimCharacter->GetController());

        if (!IsValid(VictimController))
        {
            AbortMatchForInternalFailure(FString::Printf(
                TEXT("Respawn timer started but victim MatchPlayerController is unavailable. Victim=%s"),
                *GetNameSafe(VictimCharacter)
            ));
            return;
        }

        FDMSpectatorContext SpectatorContext;
        SpectatorContext.DeathLocation = DeathLocation;

        if (IsValid(KillerCharacter) && KillerCharacter != VictimCharacter)
        {
            SpectatorContext.KillerPawn = KillerCharacter;
        }

        SpectatorContext.SpectatorStartServerTime = SpectatorStartServerTime;
        VictimController->ClientBeginSpectatorSequence(SpectatorContext);
    }
}

#pragma endregion

#pragma region Match Flow

void ADMMatchGameMode::ClearPreMatchWaitingState()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(StartingRosterTimeoutHandle);
        World->GetTimerManager().ClearTimer(PreMatchTutorialTimeoutHandle);
    }
    bPreMatchTutorialGateActive = false;
    TutorialDisplayedControllers.Empty();
    TutorialCompletedControllers.Empty();
}

bool ADMMatchGameMode::IsValidPreMatchTutorialParticipant(ADMMatchPlayerController* Controller) const
{
    const ADMMatchGameState* State = GetGameState<ADMMatchGameState>();
    ADMMatchPlayerState* Participant = IsValid(Controller)
        ? Controller->GetPlayerState<ADMMatchPlayerState>() : nullptr;
    return IsValid(State) && IsValid(Participant) && Participant->HasAssignedBaseSlot()
        && State->PlayerArray.Contains(Participant) && IsValid(Controller->GetPawn());
}

void ADMMatchGameMode::HandlePreMatchTutorialTimeout()
{
    const ADMMatchGameState* State = GetGameState<ADMMatchGameState>();
    if (!HasAuthority() || !bPreMatchTutorialGateActive || !IsValid(State)
        || State->GetCurrentPhase() != EDMMatchPhase::Waiting)
    {
        return;
    }
    AbortMatchForInternalFailure(FString::Printf(
        TEXT("Tutorial timed out after %.1fs. Displayed=%d/%d, Completed=%d/%d"),
        PreMatchTutorialTimeoutSeconds, TutorialDisplayedControllers.Num(), StartingPlayerCountSnapshot,
        TutorialCompletedControllers.Num(), StartingPlayerCountSnapshot));
}

void ADMMatchGameMode::StartPreMatchTutorialGate()
{
    ADMMatchGameState* State = GetGameState<ADMMatchGameState>();
    if (!HasAuthority() || bPreMatchTutorialGateActive)
    {
        return;
    }
    if (!IsValid(State))
    {
        AbortMatchForInternalFailure(TEXT("Tutorial gate failed: MatchGameState is unavailable."));
        return;
    }
    if (State->GetCurrentPhase() != EDMMatchPhase::Waiting)
    {
        return;
    }
    if (!bEnablePreMatchTutorial)
    {
        StartCountdown();
        return;
    }
    if (StartingPlayerCountSnapshot <= 0
        || State->GetCurrentPlayerStateCount() != StartingPlayerCountSnapshot)
    {
        AbortMatchForInternalFailure(TEXT("Tutorial participants differ from the frozen starting roster."));
        return;
    }

    bPreMatchTutorialGateActive = true;
    TutorialDisplayedControllers.Empty();
    TutorialCompletedControllers.Empty();
    GetWorldTimerManager().SetTimer(PreMatchTutorialTimeoutHandle, this,
        &ThisClass::HandlePreMatchTutorialTimeout, PreMatchTutorialTimeoutSeconds, false);
    if (!GetWorldTimerManager().IsTimerActive(PreMatchTutorialTimeoutHandle))
    {
        AbortMatchForInternalFailure(TEXT("Tutorial timeout could not be scheduled."));
        return;
    }

    int32 NotifiedPlayers = 0;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        // Listen Server 호스트의 즉시 실패 보고로 이미 Aborted가 되었다면 전송을 중단한다.
        if (!bPreMatchTutorialGateActive || State->GetCurrentPhase() != EDMMatchPhase::Waiting)
        {
            return;
        }
        ADMMatchPlayerController* Controller = Cast<ADMMatchPlayerController>(It->Get());
        if (!IsValidPreMatchTutorialParticipant(Controller))
        {
            continue;
        }
        Controller->ClientShowPreMatchTutorial();
        ++NotifiedPlayers;
    }
    if (NotifiedPlayers != StartingPlayerCountSnapshot)
    {
        AbortMatchForInternalFailure(FString::Printf(
            TEXT("Tutorial notification mismatch. Notified=%d, Expected=%d"),
            NotifiedPlayers, StartingPlayerCountSnapshot));
        return;
    }
    UE_LOG(LogDMMatchGameMode, Log,
        TEXT("Tutorial gate opened. Notified=%d, Expected=%d, Timeout=%.1fs"),
        NotifiedPlayers, StartingPlayerCountSnapshot, PreMatchTutorialTimeoutSeconds);
}

void ADMMatchGameMode::HandlePreMatchTutorialDisplayReported(
    ADMMatchPlayerController* RequestingController, const bool bSucceeded)
{
    const ADMMatchGameState* State = GetGameState<ADMMatchGameState>();
    if (!HasAuthority() || !bPreMatchTutorialGateActive || !IsValid(State)
        || State->GetCurrentPhase() != EDMMatchPhase::Waiting
        || !IsValidPreMatchTutorialParticipant(RequestingController))
    {
        return;
    }
    if (!bSucceeded)
    {
        AbortMatchForInternalFailure(FString::Printf(
            TEXT("Tutorial widget failed to display. PlayerController=%s"), *GetNameSafe(RequestingController)));
        return;
    }
    TutorialDisplayedControllers.Add(TWeakObjectPtr<ADMMatchPlayerController>(RequestingController));
    UE_LOG(LogDMMatchGameMode, Log, TEXT("Tutorial displayed: %d/%d"),
        TutorialDisplayedControllers.Num(), StartingPlayerCountSnapshot);
}

void ADMMatchGameMode::HandlePreMatchTutorialCompleted(ADMMatchPlayerController* RequestingController)
{
    ADMMatchGameState* State = GetGameState<ADMMatchGameState>();
    if (!HasAuthority() || !bPreMatchTutorialGateActive || !IsValid(State)
        || State->GetCurrentPhase() != EDMMatchPhase::Waiting
        || !IsValidPreMatchTutorialParticipant(RequestingController))
    {
        return;
    }

    const TWeakObjectPtr<ADMMatchPlayerController> Key(RequestingController);
    if (!TutorialDisplayedControllers.Contains(Key))
    {
        UE_LOG(LogDMMatchGameMode, Warning,
            TEXT("Tutorial completion ignored: successful display acknowledgement is missing. Player=%s"),
            *GetNameSafe(RequestingController));
        return;
    }
    if (TutorialCompletedControllers.Contains(Key))
    {
        return;
    }
    TutorialCompletedControllers.Add(Key);
    UE_LOG(LogDMMatchGameMode, Log, TEXT("Tutorial completed: %d/%d"),
        TutorialCompletedControllers.Num(), StartingPlayerCountSnapshot);

    if (StartingPlayerCountSnapshot > 0
        && TutorialCompletedControllers.Num() == StartingPlayerCountSnapshot
        && TutorialDisplayedControllers.Num() == StartingPlayerCountSnapshot)
    {
        // 모든 참가자의 튜토리얼 완료를 검증한 뒤에만 카운트다운을 요청한다.
        // 실제 카운트다운 전환 성공 후에 대기 상태를 정리한다.
        StartCountdown();
    }
}

void ADMMatchGameMode::StartCountdown()
{
    ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    if (!ensureMsgf(IsValid(MatchGameState), TEXT("StartCountdown failed: MatchGameState is unavailable.")))
    {
        AbortMatchForInternalFailure(TEXT("Countdown failed: GameState is missing."));
        return;
    }

    if (!ensureMsgf(IsValid(MatchRuleData), TEXT("StartCountdown failed: MatchRuleData is unavailable.")))
    {
        AbortMatchForInternalFailure(TEXT("Countdown failed: MatchRuleData is missing."));
        return;
    }

    /*
     * 실제 Phase 변경은 TryTransitionPhase()가 다시 검사하지만,
     * StartCountdown 자체의 호출 조건을 명확히 하기 위해
     * 여기서도 Waiting 여부를 먼저 검사한다.
     */

    if (MatchGameState->GetCurrentPhase() != EDMMatchPhase::Waiting)
    {
        UE_LOG(LogDMMatchGameMode, Warning, TEXT("StartCountdown rejected: current phase is not Waiting."));

        return;
    }

    // 튜토리얼 사용 시 다른 호출 경로에서 카운트다운을 조기 시작하지 못하도록 한다.
    // 튜토리얼을 명시적으로 비활성화한 기존 경기 시작 경로는 그대로 허용한다.
    if (bEnablePreMatchTutorial
        && (!bPreMatchTutorialGateActive
            || StartingPlayerCountSnapshot <= 0
            || TutorialDisplayedControllers.Num() != StartingPlayerCountSnapshot
            || TutorialCompletedControllers.Num() != StartingPlayerCountSnapshot))
    {
        UE_LOG(LogDMMatchGameMode, Warning,
            TEXT("StartCountdown rejected: tutorial incomplete. Gate=%d, Displayed=%d/%d, Completed=%d/%d"),
            bPreMatchTutorialGateActive ? 1 : 0,
            TutorialDisplayedControllers.Num(), StartingPlayerCountSnapshot,
            TutorialCompletedControllers.Num(), StartingPlayerCountSnapshot);
        return;
    }

    if (GetWorldTimerManager().IsTimerActive(PhaseTimerHandle))
    {
        UE_LOG(LogDMMatchGameMode, Warning, TEXT("StartCountdown rejected: PhaseTimer is already active."));

        AbortMatchForInternalFailure(TEXT("Countdown failed: PhaseTimer was already active."));
        return;
    }

    if (!DMMatch::IsValidPlayerCount(StartingPlayerCountSnapshot))
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT("StartCountdown failed: StartingPlayerCountSnapshot is invalid. Snapshot=%d"),
            StartingPlayerCountSnapshot
        );

        AbortMatchForInternalFailure(TEXT("Countdown failed: StartingPlayerCountSnapshot is invalid."));
        return;
    }

    if (MatchGameState->GetStartingPlayerCount() != StartingPlayerCountSnapshot)
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT("StartCountdown failed: StartingPlayerCount projection mismatch. Snapshot=%d, GameState=%d"),
            StartingPlayerCountSnapshot,
            MatchGameState->GetStartingPlayerCount()
        );

        AbortMatchForInternalFailure(TEXT("Countdown failed: Starting player count projection mismatch."));
        return;
    }

    if (MatchGameState->GetCurrentPlayerStateCount() != StartingPlayerCountSnapshot)
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT("StartCountdown failed: connected player count mismatch. Connected=%d, Snapshot=%d"),
            MatchGameState->GetCurrentPlayerStateCount(),
            StartingPlayerCountSnapshot
        );

        AbortMatchForInternalFailure(TEXT("Countdown failed: Connected player count mismatch."));
        return;
    }

    const float CountdownDuration = MatchRuleData->CountdownDuration;

    if (!ensureMsgf(
            CountdownDuration > 0.0f,
            TEXT("StartCountdown failed: CountdownDuration must be positive. Duration=%.3f"),
            CountdownDuration
        ))
    {
        AbortMatchForInternalFailure(TEXT("Countdown failed: CountdownDuration must be positive."));
        return;
    }

    const double ServerNow = MatchGameState->GetServerWorldTimeSeconds();

    const double CountdownEndServerTime = ServerNow + CountdownDuration;

    /*
     * 현재 Phase를 종료할 서버 Timer를 먼저 등록한다.
     */

    GetWorldTimerManager().SetTimer(
        PhaseTimerHandle, this, &ADMMatchGameMode::HandleCountdownExpired, CountdownDuration, false
    );

    if (!GetWorldTimerManager().IsTimerActive(PhaseTimerHandle))
    {
        UE_LOG(LogDMMatchGameMode, Error, TEXT("StartCountdown failed: Countdown Timer was not activated."));

        AbortMatchForInternalFailure(TEXT("Countdown failed: Countdown timer failed to start."));
        return;
    }

    /*
     * Phase 변경은 항상 중앙 전환 함수(TryTransitionPhase)를 거친다.
     */
    if (!TryTransitionPhase(EDMMatchPhase::Countdown, CountdownEndServerTime))
    {
        /*
         * Phase 전환에 실패했다면
         * 먼저 등록한 Timer도 제거한다.
         */
        GetWorldTimerManager().ClearTimer(PhaseTimerHandle);
        AbortMatchForInternalFailure(TEXT("Countdown failed: phase transition rejected."));
        return;
    }

    // Phase 전환에 성공한 시점에만 튜토리얼 대기 Timer와 완료 집합을 정리한다.
    ClearPreMatchWaitingState();

    UE_LOG(
        LogDMMatchGameMode,
        Log,
        TEXT("Countdown started. Players=%d, Duration=%.3f, ServerNow=%.3f, EndServerTime=%.3f"),
        StartingPlayerCountSnapshot,
        CountdownDuration,
        ServerNow,
        CountdownEndServerTime
    );
}

void ADMMatchGameMode::HandleCountdownExpired()
{
    ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    if (!ensureMsgf(IsValid(MatchGameState), TEXT("HandleCountdownExpired failed: MatchGameState is unavailable.")))
    {
        return;
    }

    if (MatchGameState->GetCurrentPhase() != EDMMatchPhase::Countdown)
    {
        UE_LOG(
            LogDMMatchGameMode, Verbose, TEXT("Countdown expiration ignored: current phase is no longer Countdown.")
        );

        return;
    }

    UE_LOG(LogDMMatchGameMode, Log, TEXT("Countdown expired. Transitioning to Playing."));

    StartPlaying();
}

void ADMMatchGameMode::StartPlaying()
{
    ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    if (!ensureMsgf(IsValid(MatchGameState), TEXT("StartPlaying failed: MatchGameState is unavailable.")))
    {
        AbortMatchForInternalFailure(TEXT("StartPlaying failed: MatchGameState is unavailable."));
        return;
    }

    if (!ensureMsgf(IsValid(MatchRuleData), TEXT("StartPlaying failed: MatchRuleData is unavailable.")))
    {
        AbortMatchForInternalFailure(TEXT("StartPlaying failed: MatchRuleData is unavailable."));
        return;
    }

    if (MatchGameState->GetCurrentPhase() != EDMMatchPhase::Countdown)
    {
        UE_LOG(LogDMMatchGameMode, Warning, TEXT("StartPlaying rejected: current phase is not Countdown."));

        return;
    }

    const float MatchDuration = MatchRuleData->MatchDuration;

    if (!ensureMsgf(
            MatchDuration > 0.0f,
            TEXT("StartPlaying failed: MatchDuration must be positive. Duration=%.3f"),
            MatchDuration
        ))
    {
        AbortMatchForInternalFailure(TEXT("StartPlaying failed: MatchDuration is not positive."));
        return;
    }

    /*
     * Countdown Timer가 만료된 뒤 같은 Handle을
     * 정규 경기 Timer 용도로 재사용한다.
     */
    GetWorldTimerManager().ClearTimer(PhaseTimerHandle);

    const double ServerNow = MatchGameState->GetServerWorldTimeSeconds();

    const double RegularEndServerTime = ServerNow + MatchDuration;

    GetWorldTimerManager().SetTimer(
        PhaseTimerHandle, this, &ADMMatchGameMode::HandleRegularTimeExpired, MatchDuration, false
    );

    if (!GetWorldTimerManager().IsTimerActive(PhaseTimerHandle))
    {
        AbortMatchForInternalFailure(TEXT("StartPlaying failed: regular match timer was not activated."));
        return;
    }

    /*
     * Phase 변경은 항상 중앙 전환 함수(TryTransitionPhase)를 거친다.
     */
    if (!TryTransitionPhase(EDMMatchPhase::Playing, RegularEndServerTime))
    {
        /*
         * Phase 전환 실패 시 정규 경기 Timer도 제거한다.
         */
        GetWorldTimerManager().ClearTimer(PhaseTimerHandle);
        AbortMatchForInternalFailure(TEXT("StartPlaying failed: could not transition to Playing."));
        return;
    }

    // 정규 경기 시작 직후 실제 종료 시각을 기준으로 공개를 예약한다.
    CancelRelicReveal();

    if (MatchDuration >= RelicRevealLeadSeconds + RelicRevealDurationSeconds)
    {
        const double RevealStartServerTime = RegularEndServerTime - RelicRevealLeadSeconds;
        const double RemainingDelay = RevealStartServerTime - MatchGameState->GetServerWorldTimeSeconds();

        if (RemainingDelay > 0.0)
        {
            GetWorldTimerManager().SetTimer(
                RelicRevealStartTimerHandle,
                this,
                &ThisClass::BeginRelicReveal,
                static_cast<float>(RemainingDelay),
                false
            );
        }
    }
    else
    {
        UE_LOG(LogDMMatchGameMode, Log, TEXT("Relic reveal not scheduled: MatchDuration=%.1f"), MatchDuration);
    }

    // Quest·Event는 필수 경기 규칙이다. 일부만 가동된 매치를 계속 진행하지 않는다.
    if (!IsValid(QuestDirector) || !QuestDirector->StartPlaying())
    {
        AbortMatchForInternalFailure(TEXT("Regular match start failed: Quest schedule could not start."));
        return;
    }

    if (!IsValid(RandomEventDirector) || !RandomEventDirector->StartPlaying())
    {
        AbortMatchForInternalFailure(TEXT("Regular match start failed: Random Event schedule could not start."));
        return;
    }

    UE_LOG(
        LogDMMatchGameMode,
        Log,
        TEXT("Regular match timer started. Players=%d, Duration=%.3f, ServerNow=%.3f, EndServerTime=%.3f"),
        StartingPlayerCountSnapshot,
        MatchDuration,
        ServerNow,
        RegularEndServerTime
    );
}

void ADMMatchGameMode::BeginRelicReveal()
{
    ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();
    if (!IsValid(MatchGameState) || !IsValid(RelicManager)
        || MatchGameState->GetCurrentPhase() != EDMMatchPhase::Playing)
    {
        return;
    }

    TArray<FDMRelicRevealEntry> Entries;
    if (!RelicManager->BuildRelicRevealEntries(Entries))
    {
        UE_LOG(LogDMMatchGameMode, Warning, TEXT("Relic reveal skipped: incomplete positions."));
        return;
    }

    FDMRelicRevealState RevealState;
    RevealState.EndServerTime = MatchGameState->GetServerWorldTimeSeconds() + RelicRevealDurationSeconds;
    RevealState.Entries = MoveTemp(Entries);

    MatchGameState->SetRelicRevealState(RevealState);
    if (!MatchGameState->GetRelicRevealState().HasPayload())
    {
        return;
    }

    GetWorldTimerManager().SetTimer(
        RelicRevealEndTimerHandle, this, &ThisClass::EndRelicReveal, RelicRevealDurationSeconds, false
    );

    UE_LOG(
        LogDMMatchGameMode,
        Log,
        TEXT("Relic reveal started. Count=%d, EndServerTime=%.3f"),
        RevealState.Entries.Num(),
        RevealState.EndServerTime
    );
}

void ADMMatchGameMode::EndRelicReveal()
{
    GetWorldTimerManager().ClearTimer(RelicRevealStartTimerHandle);
    GetWorldTimerManager().ClearTimer(RelicRevealEndTimerHandle);

    ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();
    if (IsValid(MatchGameState))
    {
        MatchGameState->SetRelicRevealState(FDMRelicRevealState());
    }
}

void ADMMatchGameMode::CancelRelicReveal()
{
    // 조기 종료·연장전 진입·시작 초기화에서 동일한 정리 경로를 사용한다.
    EndRelicReveal();
}

void ADMMatchGameMode::HandleRegularTimeExpired()
{
    ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    if (!ensureMsgf(IsValid(MatchGameState), TEXT("Regular time expiry handling failed: GameState is invalid.")))
    {
        AbortMatchForInternalFailure(TEXT("정규 시간 종료: GameState 없음"));

        return;
    }

    // 이미 종료되었거나 다른 단계로 넘어갔다면 처리하지 않는다.
    if (MatchGameState->GetCurrentPhase() != EDMMatchPhase::Playing
        || MatchGameState->GetMatchEndReason() != EDMMatchEndReason::None)
    {
        return;
    }

    const FDMMatchPhaseState PhaseState = MatchGameState->GetPhaseState();

    const double RemainingTime = PhaseState.PhaseEndServerTime - MatchGameState->GetServerWorldTimeSeconds();

    // 서버 종료 시각보다 먼저 호출되었다면 남은 시간만 다시 예약한다.
    if (RemainingTime > 0.0)
    {
        GetWorldTimerManager().SetTimer(
            PhaseTimerHandle,
            this,
            &ThisClass::HandleRegularTimeExpired,
            FMath::Max(static_cast<float>(RemainingTime), KINDA_SMALL_NUMBER),
            false
        );

        return;
    }

    GetWorldTimerManager().ClearTimer(PhaseTimerHandle);

    // 기존 유물 승리·전멸 승리 조건을 마지막으로 확인한다.
    EvaluateMatchResult();

    if (MatchGameState->GetMatchEndReason() != EDMMatchEndReason::None
        || MatchGameState->GetCurrentPhase() != EDMMatchPhase::Playing)
    {
        return;
    }

    // 정규 시간 종료 시 등록 유물 수(RegisteredRelicCount) 단독 1위는 즉시 승리한다.
    // 최다 동률(0개 동률 포함)일 때만 연장전으로 진입한다.
    // TryResolveOvertimeWinner()는 Phase와 무관하게 등록 유물 수 단독 1위를 구하므로 재사용한다.
    int32 RegularTimeWinnerBaseSlot = INDEX_NONE;

    if (!TryResolveOvertimeWinner(RegularTimeWinnerBaseSlot))
    {
        AbortMatchForInternalFailure(TEXT("정규 시간 종료: 등록 유물 수 판정에 실패했습니다."));
        return;
    }

    if (RegularTimeWinnerBaseSlot != INDEX_NONE)
    {
        if (TryFinishMatch(EDMMatchEndReason::RegularTimeVictory, RegularTimeWinnerBaseSlot))
        {
            return;
        }

        // 같은 프레임에 다른 경로가 결과를 먼저 확정했다면 정상적인 경쟁이다.
        if (MatchGameState->GetMatchEndReason() != EDMMatchEndReason::None)
        {
            return;
        }

        AbortMatchForInternalFailure(
            FString::Printf(TEXT("정규 시간 승리 적용 실패. Winner=%d"), RegularTimeWinnerBaseSlot)
        );
        return;
    }

    if (!TryStartOvertimeIntro())
    {
        // 같은 프레임에 다른 경로가 결과를 확정했거나 Phase를 바꿨다면 정상적인 경쟁이다.
        if (MatchGameState->GetCurrentPhase() != EDMMatchPhase::Playing
            || MatchGameState->GetMatchEndReason() != EDMMatchEndReason::None)
        {
            return;
        }

        AbortMatchForInternalFailure(TEXT("정규 시간 종료 후 연장전 진입에 실패했습니다."));
    }
}

#pragma endregion

#pragma region Phase Transition

bool ADMMatchGameMode::TryTransitionPhase(const EDMMatchPhase TargetPhase, const double PhaseEndServerTime)
{
    ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    if (!IsValid(MatchGameState))
    {
        UE_LOG(LogDMMatchGameMode, Error, TEXT("Phase transition failed: MatchGameState is unavailable."));

        return false;
    }

    const EDMMatchPhase CurrentPhase = MatchGameState->GetCurrentPhase();

    if (CurrentPhase == TargetPhase)
    {
        UE_LOG(
            LogDMMatchGameMode,
            Warning,
            TEXT("Phase transition rejected: current and target phases are identical. Phase=%s"),
            *UEnum::GetValueAsString(CurrentPhase)
        );

        return false;
    }

    if (!IsPhaseTransitionAllowed(CurrentPhase, TargetPhase))
    {
        UE_LOG(
            LogDMMatchGameMode,
            Warning,
            TEXT("Phase transition rejected: transition is not allowed. From=%s, To=%s"),
            *UEnum::GetValueAsString(CurrentPhase),
            *UEnum::GetValueAsString(TargetPhase)
        );

        return false;
    }

    const bool bTimedPhase = TargetPhase == EDMMatchPhase::Countdown || TargetPhase == EDMMatchPhase::Playing
        || TargetPhase == EDMMatchPhase::OvertimeIntro || TargetPhase == EDMMatchPhase::Overtime;

    const double ServerNow = MatchGameState->GetServerWorldTimeSeconds();

    if (!ensureMsgf(
            !(bTimedPhase && PhaseEndServerTime <= ServerNow),
            TEXT(
                "Phase transition failed: timed phase requires a future deadline. Target=%s, ServerNow=%.3f, Deadline=%.3f"
            ),
            *UEnum::GetValueAsString(TargetPhase),
            ServerNow,
            PhaseEndServerTime
        ))
    {
        return false;
    }

    if (!ensureMsgf(
            !(!bTimedPhase && !FMath::IsNearlyZero(PhaseEndServerTime)),
            TEXT("Phase transition failed: untimed phase requires a zero deadline. Target=%s, Deadline=%.3f"),
            *UEnum::GetValueAsString(TargetPhase),
            PhaseEndServerTime
        ))
    {
        return false;
    }

    FDMMatchPhaseState NewPhaseState;
    NewPhaseState.CurrentPhase = TargetPhase;
    NewPhaseState.PhaseEndServerTime = PhaseEndServerTime;

    if (!MatchGameState->TrySetPhaseState(NewPhaseState))
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT("Phase transition failed: GameState rejected the requested state. Requested=%s"),
            *UEnum::GetValueAsString(TargetPhase)
        );

        return false;
    }

    if (CurrentPhase == EDMMatchPhase::Waiting && TargetPhase != EDMMatchPhase::Waiting)
    {
        ClearPreMatchWaitingState();
    }

    if (CurrentPhase == EDMMatchPhase::Playing && TargetPhase != EDMMatchPhase::Playing)
    {
        CancelRelicReveal();
    }

    // OvertimeIntro를 벗어나면 진영별 공개 Snapshot을 비운다.
    if (CurrentPhase == EDMMatchPhase::OvertimeIntro && TargetPhase != EDMMatchPhase::OvertimeIntro)
    {
        MatchGameState->SetOvertimeIntroState(FDMOvertimeIntroState());
    }

    UE_LOG(
        LogDMMatchGameMode,
        Log,
        TEXT("Match phase transitioned. From=%s, To=%s, EndServerTime=%.3f"),
        *UEnum::GetValueAsString(CurrentPhase),
        *UEnum::GetValueAsString(TargetPhase),
        PhaseEndServerTime
    );

    return true;
}

bool ADMMatchGameMode::IsPhaseTransitionAllowed(const EDMMatchPhase FromPhase, const EDMMatchPhase ToPhase)
{
    switch (FromPhase)
    {
    case EDMMatchPhase::Waiting:
        return ToPhase == EDMMatchPhase::Countdown || ToPhase == EDMMatchPhase::Aborted;

    case EDMMatchPhase::Countdown:
        return ToPhase == EDMMatchPhase::Playing || ToPhase == EDMMatchPhase::Aborted;

    case EDMMatchPhase::Playing:
        return ToPhase == EDMMatchPhase::OvertimeIntro || ToPhase == EDMMatchPhase::Finished
            || ToPhase == EDMMatchPhase::Aborted;

    case EDMMatchPhase::OvertimeIntro:
        return ToPhase == EDMMatchPhase::Overtime || ToPhase == EDMMatchPhase::Aborted;

    case EDMMatchPhase::Overtime:
        return ToPhase == EDMMatchPhase::Finished || ToPhase == EDMMatchPhase::Aborted;

    case EDMMatchPhase::Finished:
    case EDMMatchPhase::Aborted:
    default:
        return false;
    }
}

#pragma endregion

#pragma region Match Result

void ADMMatchGameMode::HandleRelicDepositCommitted()
{
    UE_LOG(LogDMMatchGameMode, Log, TEXT("Relic deposit committed. Evaluating Match result."));

    EvaluateMatchResult();
}

void ADMMatchGameMode::HandleRelicStolen(ADMMatchPlayerState* StealingPlayerState)
{
    if (!IsValid(StealingPlayerState))
    {
        UE_LOG(LogDMMatchGameMode, Warning, TEXT("Relic stolen notification skipped: StealingPlayerState is invalid."));

        return;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    int32 NotificationReceiverCount = 0;

    for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator();

         Iterator;

         ++Iterator)
    {
        ADMMatchPlayerController* MatchPlayerController = Cast<ADMMatchPlayerController>(Iterator->Get());

        if (!IsValid(MatchPlayerController))
        {
            continue;
        }

        ADMMatchPlayerState* ReceiverPlayerState = MatchPlayerController->GetPlayerState<ADMMatchPlayerState>();

        /*
         * 실제로 훔친 본인에게는
         * 자기 Steal 알림을 보내지 않는다.
         */
        if (ReceiverPlayerState == StealingPlayerState)
        {
            continue;
        }

        MatchPlayerController->ClientNotifyRelicStolen();

        ++NotificationReceiverCount;
    }

    UE_LOG(
        LogDMMatchGameMode,
        Log,
        TEXT("Relic stolen notification sent. Thief=%s, Receivers=%d"),
        *GetNameSafe(StealingPlayerState),
        NotificationReceiverCount
    );
}

bool ADMMatchGameMode::CheckRelicVictory(int32& OutWinningBaseSlot, int32& OutWinningRegisteredRelicCount) const
{
    OutWinningBaseSlot = INDEX_NONE;

    OutWinningRegisteredRelicCount = 0;

    if (!IsValid(RelicManager))
    {
        return false;
    }

    if (RequiredRelicCountSnapshot <= 0)
    {
        return false;
    }

    const ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    if (!IsValid(MatchGameState))
    {
        return false;
    }

    const FDMMatchParticipantSnapshotList Participants = BuildParticipantSnapshots(MatchGameState, true);

    return DMMatchResultRules::FindRelicVictoryWinner(
        Participants, RequiredRelicCountSnapshot, OutWinningBaseSlot, OutWinningRegisteredRelicCount
    );
}

bool ADMMatchGameMode::CheckEliminationVictory(
    int32& OutWinningBaseSlot, int32& OutConnectedParticipantCount, int32& OutAlivePlayerCount
) const
{
    OutWinningBaseSlot = INDEX_NONE;

    OutConnectedParticipantCount = 0;

    OutAlivePlayerCount = 0;

    /*
     * Match 시작 시 비활성화된 Rule이면
     * 전멸 승리를 평가하지 않는다.
     */
    if (!bEnableEliminationVictorySnapshot)
    {
        return false;
    }

    const ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    if (!IsValid(MatchGameState))
    {
        return false;
    }

    const FDMMatchParticipantSnapshotList Participants = BuildParticipantSnapshots(MatchGameState, false);

    return DMMatchResultRules::FindEliminationWinner(
        Participants, OutWinningBaseSlot, OutConnectedParticipantCount, OutAlivePlayerCount
    );
}

bool ADMMatchGameMode::EvaluateMatchResult()
{
    ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    if (!ensureMsgf(IsValid(MatchGameState), TEXT("EvaluateMatchResult failed: MatchGameState is unavailable.")))
    {
        return false;
    }

    /*
     * 이미 결과가 확정된 Match라면
     * 추가 승리 판정을 하지 않는다.
     */
    if (MatchGameState->GetMatchEndReason() != EDMMatchEndReason::None)
    {
        return false;
    }

    const EDMMatchPhase CurrentPhase = MatchGameState->GetCurrentPhase();

    /*
     * 실제 승리 판정은 Playing / Overtime에서만 허용한다.
     */
    if (CurrentPhase != EDMMatchPhase::Playing && CurrentPhase != EDMMatchPhase::Overtime)
    {
        return false;
    }

    /*
     * =====================================
     * 1순위: Relic Victory
     * =====================================
     *
     * 중앙 결과 우선순위:
     * Relic Victory
     * → Elimination Victory
     */
    int32 WinningBaseSlot = INDEX_NONE;

    int32 WinningRegisteredRelicCount = 0;

    if (CheckRelicVictory(WinningBaseSlot, WinningRegisteredRelicCount))
    {
        UE_LOG(
            LogDMMatchGameMode,
            Log,
            TEXT("Relic victory condition satisfied. WinnerBaseSlot=%d, RegisteredRelics=%d, RequiredRelics=%d"),
            WinningBaseSlot,
            WinningRegisteredRelicCount,
            RequiredRelicCountSnapshot
        );

        return TryFinishMatch(EDMMatchEndReason::RelicVictory, WinningBaseSlot);
    }

    /*
     * =====================================
     * 2순위: Elimination Victory
     * =====================================
     *
     * Relic Victory가 먼저 평가된 뒤에만
     * Elimination Victory를 검사한다.
     */
    int32 EliminationWinningBaseSlot = INDEX_NONE;

    int32 ConnectedParticipantCount = 0;

    int32 AlivePlayerCount = 0;

    if (CheckEliminationVictory(EliminationWinningBaseSlot, ConnectedParticipantCount, AlivePlayerCount))
    {
        UE_LOG(
            LogDMMatchGameMode,
            Log,
            TEXT("Elimination victory condition satisfied. WinnerBaseSlot=%d, ConnectedParticipants=%d, AlivePlayers=%d"
            ),
            EliminationWinningBaseSlot,
            ConnectedParticipantCount,
            AlivePlayerCount
        );

        return TryFinishMatch(EDMMatchEndReason::EliminationVictory, EliminationWinningBaseSlot);
    }

    return false;
}

bool ADMMatchGameMode::TryResolveOvertimeWinner(int32& OutWinningBaseSlot) const
{
    OutWinningBaseSlot = INDEX_NONE;

    const ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    if (!ensureMsgf(
            IsValid(MatchGameState) && IsValid(RelicManager),
            TEXT("Overtime result resolution failed: server authority or result source is invalid.")
        ))
    {
        return false;
    }

    const int32 StartingPlayerCount = MatchGameState->GetStartingPlayerCount();

    // 참가자 누락을 정상적인 동률로 처리하지 않는다.
    if (!DMMatch::IsValidPlayerCount(StartingPlayerCount) || MatchGameState->PlayerArray.Num() != StartingPlayerCount)
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT("Overtime result resolution failed: participant count mismatch. Starting=%d, Current=%d"),
            StartingPlayerCount,
            MatchGameState->PlayerArray.Num()
        );

        return false;
    }

    FDMMatchParticipantSnapshotList Participants;

    for (APlayerState* BasePlayerState : MatchGameState->PlayerArray)
    {
        Participants.Add(MakeParticipantSnapshot(Cast<ADMMatchPlayerState>(BasePlayerState), true));
    }

    const EDMOvertimeWinnerResolveResult ResolveResult =
        DMMatchResultRules::ResolveOvertimeWinner(Participants, OutWinningBaseSlot);

    if (ResolveResult == EDMOvertimeWinnerResolveResult::InvalidParticipant)
    {
        UE_LOG(LogDMMatchGameMode, Error, TEXT("Overtime result resolution failed: invalid participant found."));
    }

    return ResolveResult == EDMOvertimeWinnerResolveResult::Resolved;
}

TArray<FDMMatchResultEntry> ADMMatchGameMode::BuildFinalPlayerResults(const int32 WinningBaseSlot) const
{
    TArray<FDMMatchResultEntry> Results;

    const ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    if (!IsValid(MatchGameState))
    {
        return Results;
    }

    for (const ADMMatchPlayerState* MatchPlayerState : DMGameplay::GetMatchPlayerStates(MatchGameState))
    {
        if (!MatchPlayerState->HasAssignedBaseSlot())
        {
            continue;
        }

        FDMMatchResultEntry Entry;
        Entry.PlayerName = MatchPlayerState->GetPlayerName();
        Entry.BaseSlot = MatchPlayerState->GetBaseSlot();
        Entry.KillCount = MatchPlayerState->GetKillCount();

        // 결과표 '획득한 유물 수' 열: 경기 중 실제로 획득한 누적 유물 수
        Entry.AcquiredRelicCount = MatchPlayerState->GetAcquiredRelicCount();

        // 등수 계산 기준: 종료 순간 상자에 등록된 유물 수
        if (IsValid(RelicManager))
        {
            Entry.RelicCount = RelicManager->GetRegisteredRelicCount(Entry.BaseSlot);
        }

        Results.Add(MoveTemp(Entry));
    }

    // 결과표 등수는 종료 순간 등록 유물 수 기준이다.
    DMMatchResultRules::SortAndRankResults(Results, WinningBaseSlot);

    return Results;
}

FDMMatchParticipantSnapshot ADMMatchGameMode::MakeParticipantSnapshot(
    const ADMMatchPlayerState* MatchPlayerState, const bool bIncludeRegisteredRelicCount
) const
{
    FDMMatchParticipantSnapshot Snapshot;

    if (!IsValid(MatchPlayerState))
    {
        return Snapshot;
    }

    Snapshot.bHasAssignedBaseSlot = MatchPlayerState->HasAssignedBaseSlot();

    Snapshot.BaseSlot = MatchPlayerState->GetBaseSlot();

    Snapshot.LifeState = MatchPlayerState->GetLifeState();

    // 등록 유물 수의 SSOT는 RelicManager다. 조회만 하므로 판정 결과에 영향이 없다.
    if (bIncludeRegisteredRelicCount && Snapshot.bHasAssignedBaseSlot && IsValid(RelicManager))
    {
        Snapshot.RegisteredRelicCount = RelicManager->GetRegisteredRelicCount(Snapshot.BaseSlot);
    }

    return Snapshot;
}

FDMMatchParticipantSnapshotList ADMMatchGameMode::BuildParticipantSnapshots(
    const ADMMatchGameState* MatchGameState, const bool bIncludeRegisteredRelicCount
) const
{
    FDMMatchParticipantSnapshotList Participants;

    for (const ADMMatchPlayerState* MatchPlayerState : DMGameplay::GetMatchPlayerStates(MatchGameState))
    {
        Participants.Add(MakeParticipantSnapshot(MatchPlayerState, bIncludeRegisteredRelicCount));
    }

    return Participants;
}

bool ADMMatchGameMode::IsFinishReason(const EDMMatchEndReason EndReason)
{
    switch (EndReason)
    {
    case EDMMatchEndReason::RelicVictory:
    case EDMMatchEndReason::EliminationVictory:
    case EDMMatchEndReason::RegularTimeVictory:
    case EDMMatchEndReason::OvertimeVictory:
    case EDMMatchEndReason::AllDefeated:
    case EDMMatchEndReason::OvertimeDraw:
        return true;

    case EDMMatchEndReason::None:
    case EDMMatchEndReason::PlayerDisconnect:
    case EDMMatchEndReason::HostLeft:
    default:
        return false;
    }
}

bool ADMMatchGameMode::IsFinishReasonAllowedForPhase(const EDMMatchPhase Phase, const EDMMatchEndReason EndReason)
{
    switch (EndReason)
    {
    case EDMMatchEndReason::RelicVictory:
    case EDMMatchEndReason::EliminationVictory:
    case EDMMatchEndReason::AllDefeated:
        return Phase == EDMMatchPhase::Playing || Phase == EDMMatchPhase::Overtime;

    case EDMMatchEndReason::RegularTimeVictory:
        return Phase == EDMMatchPhase::Playing;

    case EDMMatchEndReason::OvertimeVictory:
    case EDMMatchEndReason::OvertimeDraw:
        return Phase == EDMMatchPhase::Overtime;

    case EDMMatchEndReason::None:
    case EDMMatchEndReason::PlayerDisconnect:
    case EDMMatchEndReason::HostLeft:
    default:
        return false;
    }
}

bool ADMMatchGameMode::IsAbortReason(const EDMMatchEndReason EndReason)
{
    return EndReason == EDMMatchEndReason::PlayerDisconnect || EndReason == EDMMatchEndReason::HostLeft
        || EndReason == EDMMatchEndReason::InternalError;
}

bool ADMMatchGameMode::TryFinishMatch(const EDMMatchEndReason EndReason, const int32 WinningBaseSlot)
{
    if (!ensureMsgf(
            IsFinishReason(EndReason),
            TEXT("Finish Match rejected: invalid finish reason. Reason=%s"),
            *UEnum::GetValueAsString(EndReason)
        ))
    {
        return false;
    }

    const bool bRequiresWinnerBase = EndReason == EDMMatchEndReason::RelicVictory
        || EndReason == EDMMatchEndReason::EliminationVictory || EndReason == EDMMatchEndReason::RegularTimeVictory
        || EndReason == EDMMatchEndReason::OvertimeVictory;

    if (!ensureMsgf(
            !(bRequiresWinnerBase && !DMMatch::IsValidBaseSlot(WinningBaseSlot)),
            TEXT("Finish Match rejected: winner BaseSlot is required. Reason=%s, WinningBaseSlot=%d"),
            *UEnum::GetValueAsString(EndReason),
            WinningBaseSlot
        ))
    {
        return false;
    }

    // 동률 종료에는 승리 진영이 존재하면 안 된다.
    if (!ensureMsgf(
            !(EndReason == EDMMatchEndReason::OvertimeDraw && WinningBaseSlot != INDEX_NONE),
            TEXT("Draw finish rejected: winning BaseSlot must be INDEX_NONE.")
        ))
    {
        return false;
    }

    ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    if (!ensureMsgf(IsValid(MatchGameState), TEXT("Finish Match failed: MatchGameState is unavailable.")))
    {
        return false;
    }

    const EDMMatchPhase CurrentPhase = MatchGameState->GetCurrentPhase();

    if (!IsFinishReasonAllowedForPhase(CurrentPhase, EndReason))
    {
        UE_LOG(
            LogDMMatchGameMode,
            Warning,
            TEXT("Finish Match rejected: finish reason is not valid for the current phase. Phase=%s, Reason=%s"),
            *UEnum::GetValueAsString(CurrentPhase),
            *UEnum::GetValueAsString(EndReason)
        );

        return false;
    }

    if (MatchGameState->GetMatchEndReason() != EDMMatchEndReason::None)
    {
        UE_LOG(
            LogDMMatchGameMode,
            Verbose,
            TEXT("Finish Match rejected: match result is already locked. CurrentReason=%s"),
            *UEnum::GetValueAsString(MatchGameState->GetMatchEndReason())
        );

        return false;
    }

    if (!IsPhaseTransitionAllowed(CurrentPhase, EDMMatchPhase::Finished))
    {
        UE_LOG(
            LogDMMatchGameMode,
            Warning,
            TEXT("Finish Match rejected: current phase cannot transition to Finished. CurrentPhase=%s"),
            *UEnum::GetValueAsString(CurrentPhase)
        );

        return false;
    }

    FDMMatchResult FinalResult;

    FinalResult.EndReason = EndReason;
    FinalResult.WinningBaseSlot = WinningBaseSlot;
    FinalResult.ResultServerTime = MatchGameState->GetServerWorldTimeSeconds();
    FinalResult.ResultId = FGuid::NewGuid();

    // 무승부와 전원 탈락에도 동일한 서버 기준 통계 스냅샷을 저장한다.
    // 승리자가 없으면 INDEX_NONE을 전달하고 기존 공동 순위 규칙을 사용한다.
    FinalResult.PlayerResults = BuildFinalPlayerResults(WinningBaseSlot);

    if (FinalResult.PlayerResults.IsEmpty())
    {
        UE_LOG(LogDMMatchGameMode, Warning, TEXT("Match finished without result entries. Reason=%s"),
            *UEnum::GetValueAsString(EndReason));
    }

    if (!MatchGameState->TrySetMatchResult(FinalResult))
    {
        UE_LOG(LogDMMatchGameMode, Error, TEXT("Finish Match failed: MatchResult was not applied."));

        return false;
    }

    GetWorldTimerManager().ClearTimer(PhaseTimerHandle);

    if (IsValid(RespawnCoordinator))
    {
        RespawnCoordinator->ClearAllRespawnTimers();
    }

    ShutdownRandomEventSystem();

    ShutdownQuestSystem();

    CleanupAllSkillRuntimeForTerminalMatch();

    if (!TryTransitionPhase(EDMMatchPhase::Finished, 0.0))
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT("Finish Match invariant failure: reason was applied but terminal phase transition failed.")
        );

        return false;
    }

    UE_LOG(
        LogDMMatchGameMode,
        Log,
        TEXT("Match finished. Reason=%s, WinnerBaseSlot=%d, ResultServerTime=%.3f"),
        *UEnum::GetValueAsString(EndReason),
        FinalResult.WinningBaseSlot,
        FinalResult.ResultServerTime
    );

    StartResultAutoReturnTimer(MatchGameState->GetMatchResult().ResultServerTime);

    return true;
}

void ADMMatchGameMode::HandleRematchReadyRequest(ADMMatchPlayerController* RequestingController, const bool bNewReady)
{
    if (bResultTravelCommitted || bResultTravelPending || !IsValid(RequestingController)
        || RequestingController->GetWorld() != GetWorld())
    {
        return;
    }

    // 재경기 준비 요청은 원격 참가자만 보낼 수 있다.
    // 호스트는 재시작 버튼 클릭으로 참가 의사를 확정한다.
    if (GetNetMode() != NM_ListenServer || RequestingController->IsLocalController())
    {
        return;
    }

    const ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    if (!IsValid(MatchGameState)
        || (MatchGameState->GetCurrentPhase() != EDMMatchPhase::Finished
            && MatchGameState->GetCurrentPhase() != EDMMatchPhase::Aborted))
    {
        return;
    }

    const FDMMatchResult Result = MatchGameState->GetMatchResult();

    if (Result.EndReason == EDMMatchEndReason::None
        || MatchGameState->GetServerWorldTimeSeconds() >= Result.ResultServerTime + 30.0)
    {
        return;
    }

    ADMMatchPlayerState* MatchPlayerState = RequestingController->GetPlayerState<ADMMatchPlayerState>();

    if (IsValid(MatchPlayerState) && MatchPlayerState->HasAssignedBaseSlot())
    {
        MatchPlayerState->SetRematchReady(bNewReady);
    }
}

void ADMMatchGameMode::HandleReturnToLobbyRequest(ADMMatchPlayerController* RequestingController)
{
    if (bResultTravelCommitted || bResultTravelPending || !IsValid(RequestingController)
        || RequestingController->GetWorld() != GetWorld())
    {
        return;
    }

    // 전체 참가자의 수동 로비 복귀는 호스트만 요청할 수 있다.
    if (GetNetMode() != NM_ListenServer || !RequestingController->IsLocalController())
    {
        return;
    }

    const ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    const ADMMatchPlayerState* RequestingPlayerState = RequestingController->GetPlayerState<ADMMatchPlayerState>();

    if (!IsValid(MatchGameState) || !IsValid(RequestingPlayerState) || !RequestingPlayerState->HasAssignedBaseSlot()
        || MatchGameState->GetMatchEndReason() == EDMMatchEndReason::None)
    {
        return;
    }

    const EDMMatchPhase Phase = MatchGameState->GetCurrentPhase();

    if (Phase != EDMMatchPhase::Finished && Phase != EDMMatchPhase::Aborted)
    {
        return;
    }

    // 자동 복귀와 동일한 서버 이동 경로를 사용한다.
    HandleResultAutoReturnExpired();
}

void ADMMatchGameMode::HandleRestartMatchRequest(ADMMatchPlayerController* RequestingController)
{
    if (bResultTravelCommitted || bResultTravelPending || !IsValid(RequestingController)
        || RequestingController->GetWorld() != GetWorld())
    {
        return;
    }

    // Listen Server의 로컬 호스트만 재시작할 수 있다.
    if (GetNetMode() != NM_ListenServer || !RequestingController->IsLocalController())
    {
        return;
    }

    const ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    if (!IsValid(MatchGameState) || MatchGameState->GetMatchEndReason() == EDMMatchEndReason::None)
    {
        return;
    }

    const EDMMatchPhase Phase = MatchGameState->GetCurrentPhase();

    if (Phase != EDMMatchPhase::Finished && Phase != EDMMatchPhase::Aborted)
    {
        return;
    }

    // 참가자가 빠졌다면 로비에서 인원을 다시 구성해야 한다.
    if (!DMMatch::IsValidPlayerCount(StartingPlayerCountSnapshot)
        || MatchGameState->PlayerArray.Num() != StartingPlayerCountSnapshot
        || GetNumPlayers() != StartingPlayerCountSnapshot)
    {
        return;
    }

    // 자동 복귀 시각을 넘긴 재시작 요청은 허용하지 않는다.
    const FDMMatchResult Result = MatchGameState->GetMatchResult();

    if (MatchGameState->GetServerWorldTimeSeconds() >= Result.ResultServerTime + 30.0)
    {
        return;
    }

    const ADMMatchPlayerState* HostPlayerState = RequestingController->GetPlayerState<ADMMatchPlayerState>();

    if (!IsValid(HostPlayerState) || !HostPlayerState->HasAssignedBaseSlot())
    {
        return;
    }

    bool bFoundHost = false;
    int32 ReadyClientCount = 0;

    for (APlayerState* PlayerState : MatchGameState->PlayerArray)
    {
        const ADMMatchPlayerState* MatchPlayerState = Cast<ADMMatchPlayerState>(PlayerState);

        if (!IsValid(MatchPlayerState) || !MatchPlayerState->HasAssignedBaseSlot())
        {
            return;
        }

        // 호스트는 준비 값 검사에서 제외한다.
        if (MatchPlayerState == HostPlayerState)
        {
            bFoundHost = true;
            continue;
        }

        if (!MatchPlayerState->IsRematchReady())
        {
            return;
        }

        ++ReadyClientCount;
    }

    // 전체 참가자에서 호스트 한 명을 제외한 인원이 준비해야 한다.
    if (!bFoundHost || ReadyClientCount != StartingPlayerCountSnapshot - 1)
    {
        return;
    }

    if (CurrentMatchMapPackageName.IsEmpty())
    {
        RecoverFromResultTravelFailure(TEXT("재경기에 사용할 매치 맵 경로가 비어 있습니다."));

        return;
    }

    const FString TravelURL =
        FString::Printf(TEXT("%s?listen?ExpectedPlayers=%d"), *CurrentMatchMapPackageName, StartingPlayerCountSnapshot);

    CommitServerTravel(TravelURL);
}

bool ADMMatchGameMode::TryCommitHostLeaveToMainMenu(ADMMatchPlayerController* RequestingController)
{
    if (bResultTravelCommitted || !IsValid(RequestingController) || RequestingController->GetWorld() != GetWorld()
        || !RequestingController->IsLocalController())
    {
        return false;
    }

    const ENetMode NetMode = GetNetMode();

    if (NetMode != NM_ListenServer && NetMode != NM_Standalone)
    {
        return false;
    }

    // 로비 복귀·재경기·호스트 퇴장은 같은 이동 잠금을 사용한다.
    bResultTravelCommitted = true;

    GetWorldTimerManager().ClearTimer(ResultAckTimeoutHandle);

    bResultTravelPending = false;

    GetWorldTimerManager().ClearTimer(ResultAutoReturnTimerHandle);

    return true;
}

bool ADMMatchGameMode::TryAbortMatch(const EDMMatchEndReason EndReason)
{
    if (!ensureMsgf(
            IsAbortReason(EndReason),
            TEXT("Abort Match rejected: invalid abort reason. Reason=%s"),
            *UEnum::GetValueAsString(EndReason)
        ))
    {
        return false;
    }

    ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    if (!IsValid(MatchGameState))
    {
        UE_LOG(LogDMMatchGameMode, Error, TEXT("Abort Match failed: MatchGameState is unavailable."));

        return false;
    }

    if (MatchGameState->GetMatchEndReason() != EDMMatchEndReason::None)
    {
        UE_LOG(
            LogDMMatchGameMode,
            Verbose,
            TEXT("Abort Match rejected: match result is already locked. CurrentReason=%s"),
            *UEnum::GetValueAsString(MatchGameState->GetMatchEndReason())
        );

        return false;
    }

    if (!IsPhaseTransitionAllowed(MatchGameState->GetCurrentPhase(), EDMMatchPhase::Aborted))
    {
        UE_LOG(
            LogDMMatchGameMode,
            Warning,
            TEXT("Abort Match rejected: current phase cannot transition to Aborted. CurrentPhase=%s"),
            *UEnum::GetValueAsString(MatchGameState->GetCurrentPhase())
        );

        return false;
    }

    FDMMatchResult AbortResult;

    AbortResult.EndReason = EndReason;

    AbortResult.WinningBaseSlot = INDEX_NONE;

    AbortResult.ResultServerTime = MatchGameState->GetServerWorldTimeSeconds();

    AbortResult.ResultId = FGuid::NewGuid();

    if (!MatchGameState->TrySetMatchResult(AbortResult))
    {
        UE_LOG(LogDMMatchGameMode, Error, TEXT("Abort Match failed: MatchResult was not applied."));

        return false;
    }

    GetWorldTimerManager().ClearTimer(PhaseTimerHandle);

    if (IsValid(RespawnCoordinator))
    {
        RespawnCoordinator->ClearAllRespawnTimers();
    }

    ShutdownRandomEventSystem();

    ShutdownQuestSystem();

    CleanupAllSkillRuntimeForTerminalMatch();

    if (!TryTransitionPhase(EDMMatchPhase::Aborted, 0.0))
    {
        UE_LOG(
            LogDMMatchGameMode,
            Error,
            TEXT("Abort Match invariant failure: reason was applied but terminal phase transition failed.")
        );

        return false;
    }

    UE_LOG(LogDMMatchGameMode, Log, TEXT("Match aborted. Reason=%s"), *UEnum::GetValueAsString(EndReason));

    StartResultAutoReturnTimer(MatchGameState->GetMatchResult().ResultServerTime);

    return true;
}

void ADMMatchGameMode::AbortMatchForInternalFailure(const FString& Diagnostic)
{
    UE_LOG(LogDMMatchGameMode, Error, TEXT("Internal match flow error. %s"), *Diagnostic);

    GetWorldTimerManager().ClearTimer(PhaseTimerHandle);

    const ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    if (IsValid(MatchGameState) && MatchGameState->GetMatchEndReason() != EDMMatchEndReason::None)
    {
        const EDMMatchPhase Phase = MatchGameState->GetCurrentPhase();

        // 이미 확정된 결과는 덮어쓰지 않는다.
        if (Phase == EDMMatchPhase::Finished || Phase == EDMMatchPhase::Aborted)
        {
            return;
        }

        // 결과는 적용됐지만 종료 Phase 전환이 실패한 상태는 결과 화면을 만들 수 없다.
        RecoverFromResultTravelFailure(Diagnostic);
        return;
    }

    if (TryAbortMatch(EDMMatchEndReason::InternalError))
    {
        return; // 중단 결과 화면 + 30초 자동 복귀
    }

    RecoverFromResultTravelFailure(Diagnostic);
}

void ADMMatchGameMode::StartResultAutoReturnTimer(const double ResultServerTime)
{
    if (bResultTravelCommitted || bResultTravelPending)
    {
        return;
    }

    ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    if (!IsValid(MatchGameState))
    {
        return;
    }

    const double ElapsedServerTime = MatchGameState->GetServerWorldTimeSeconds() - ResultServerTime;

    const float RemainingSeconds = static_cast<float>(FMath::Max(0.0, 30.0 - ElapsedServerTime));

    if (RemainingSeconds <= 0.0f)
    {
        GetWorldTimerManager().ClearTimer(ResultAutoReturnTimerHandle);

        HandleResultAutoReturnExpired();
        return;
    }

    GetWorldTimerManager().SetTimer(
        ResultAutoReturnTimerHandle, this, &ThisClass::HandleResultAutoReturnExpired, RemainingSeconds, false
    );
}

void ADMMatchGameMode::StopResultTravelForRecovery()
{
    // 실패 이후에는 기본 맵 복구가 이동을 담당한다.
    bResultTravelCommitted = true;

    GetWorldTimerManager().ClearTimer(ResultAutoReturnTimerHandle);

    GetWorldTimerManager().ClearTimer(ResultAckTimeoutHandle);
    bResultTravelPending = false;
}

void ADMMatchGameMode::RecoverFromResultTravelFailure(const FString& Diagnostic)
{
    StopResultTravelForRecovery();

    UE_LOG(LogDMMatchGameMode, Error, TEXT("Result travel failed. %s"), *Diagnostic);

    UWorld* World = GetWorld();

    UDMGameInstance* GameInstance = IsValid(World) ? Cast<UDMGameInstance>(World->GetGameInstance()) : nullptr;

    if (!IsValid(GameInstance))
    {
        UE_LOG(LogDMMatchGameMode, Error, TEXT("Recovery failed: check the project GameInstance setting."));

        return;
    }

    GameInstance->StartTravelFailureRecovery(World, Diagnostic);
}

void ADMMatchGameMode::HandleResultAutoReturnExpired()
{
    if (bResultTravelCommitted || bResultTravelPending)
    {
        return;
    }

    if (LobbyMap.IsNull())
    {
        RecoverFromResultTravelFailure(TEXT("LobbyMap이 설정되지 않았습니다."));

        return;
    }

    const FString LobbyPackageName = LobbyMap.ToSoftObjectPath().GetLongPackageName();

    if (!FPackageName::IsValidLongPackageName(LobbyPackageName) || !FPackageName::DoesPackageExist(LobbyPackageName))
    {
        RecoverFromResultTravelFailure(FString::Printf(TEXT("로비 맵을 찾을 수 없습니다. 경로=%s"), *LobbyPackageName));

        return;
    }

    FString TravelURL = FString::Printf(TEXT("%s?listen"), *LobbyPackageName);

    if (GetNetMode() == NM_ListenServer)
    {
        UDMOnlineSessionSubsystem* Session =
            GetGameInstance() ? GetGameInstance()->GetSubsystem<UDMOnlineSessionSubsystem>() : nullptr;

        const ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

        if (!IsValid(Session) || !IsValid(MatchGameState))
        {
            RecoverFromResultTravelFailure(TEXT("로비 복귀 명단을 준비할 수 없습니다."));
            return;
        }

        TArray<FUniqueNetIdRepl> ReturningIds;

        for (const APlayerState* PlayerState : MatchGameState->PlayerArray)
        {
            if (!IsValid(PlayerState) || !PlayerState->GetUniqueId().IsValid())
            {
                RecoverFromResultTravelFailure(TEXT("로비 복귀 참가자 ID가 올바르지 않습니다."));
                return;
            }

            ReturningIds.Add(PlayerState->GetUniqueId());
        }

        if (ReturningIds.Num() != GetNumPlayers() || !Session->SetApprovedMatchRoster(ReturningIds))
        {
            RecoverFromResultTravelFailure(TEXT("로비 복귀 참가자 명단이 일치하지 않습니다."));
            return;
        }

        TravelURL += FString::Printf(TEXT("?ReturningPlayers=%d"), ReturningIds.Num());
    }

    // Steam 방을 유지한 채 기존 로비로 돌아간다.
    CommitServerTravel(TravelURL);
}

bool ADMMatchGameMode::CommitServerTravel(const FString& TravelURL)
{
    if (bResultTravelCommitted || bResultTravelPending)
    {
        return false;
    }

    if (TravelURL.IsEmpty())
    {
        RecoverFromResultTravelFailure(TEXT("서버 이동 경로가 비어 있습니다."));
        return false;
    }

    if (AreAllRemoteResultsAcknowledged())
    {
        return ExecuteServerTravel(TravelURL);
    }

    // 결과를 아직 받지 못한 참가자가 있으면 이동을 보류한다.
    bResultTravelPending = true;
    PendingResultTravelURL = TravelURL;

    GetWorldTimerManager().ClearTimer(ResultAutoReturnTimerHandle);
    GetWorldTimerManager().SetTimer(
        ResultAckTimeoutHandle, this, &ThisClass::HandleResultAckTimeout, ResultAckTimeoutSeconds, false
    );

    UE_LOG(LogDMMatchGameMode, Log, TEXT("Result travel waiting for client acknowledgements. URL=%s"), *TravelURL);
    return true;
}

void ADMMatchGameMode::HandleMatchResultAcknowledged(
    ADMMatchPlayerController* RequestingController, const FGuid& ResultId
)
{
    if (!IsValid(RequestingController) || RequestingController->GetWorld() != GetWorld()
        || RequestingController->IsLocalController())
    {
        return;
    }

    const ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    // 다른 경기의 값이나 잘못된 값은 무시한다.
    if (!IsValid(MatchGameState) || !ResultId.IsValid() || MatchGameState->GetMatchResult().ResultId != ResultId)
    {
        return;
    }

    ResultAcknowledgedControllers.Add(RequestingController);

    if (bResultTravelPending)
    {
        TryCommitPendingResultTravel();
    }
}

bool ADMMatchGameMode::AreAllRemoteResultsAcknowledged() const
{
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* Controller = It->Get();

        if (!IsValid(Controller) || Controller->IsLocalController() || Controller->IsActorBeingDestroyed()
            || Controller->GetNetConnection() == nullptr)
        {
            continue;
        }

        if (!ResultAcknowledgedControllers.Contains(Controller))
        {
            return false;
        }
    }

    return true;
}

void ADMMatchGameMode::TryCommitPendingResultTravel()
{
    if (!bResultTravelPending || bResultTravelCommitted || !AreAllRemoteResultsAcknowledged())
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(ResultAckTimeoutHandle);
    bResultTravelPending = false;

    const FString TravelURL = PendingResultTravelURL;
    PendingResultTravelURL.Reset();

    ExecuteServerTravel(TravelURL);
}

void ADMMatchGameMode::HandleResultAckTimeout()
{
    if (!bResultTravelPending)
    {
        return;
    }

    UE_LOG(
        LogDMMatchGameMode,
        Warning,
        TEXT("Result acknowledgement timed out. Clients that did not acknowledge may be missing the last result.")
    );

    bResultTravelPending = false;

    const FString TravelURL = PendingResultTravelURL;
    PendingResultTravelURL.Reset();

    ExecuteServerTravel(TravelURL);
}

bool ADMMatchGameMode::ExecuteServerTravel(const FString& TravelURL)
{
    UWorld* World = GetWorld();

    if (bResultTravelCommitted || !IsValid(World))
    {
        return false;
    }

    bResultTravelCommitted = true;

    const bool bTravelStarted = World->ServerTravel(TravelURL, false, false);

    if (!bTravelStarted)
    {
        // 원래 이동을 다시 허용하지 않고 기본 맵 복구로 전환한다.
        RecoverFromResultTravelFailure(FString::Printf(TEXT("서버 이동 요청이 거부되었습니다. 경로=%s"), *TravelURL));

        return false;
    }

    GetWorldTimerManager().ClearTimer(ResultAutoReturnTimerHandle);

    return true;
}

#pragma endregion

#pragma region Overtime

bool ADMMatchGameMode::BuildOvertimeIntroState(FDMOvertimeIntroState& OutState) const
{
    OutState = FDMOvertimeIntroState();

    const ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    if (!IsValid(RelicManager) || !IsValid(MatchGameState))
    {
        return false;
    }

    const int32 StartingPlayerCount = MatchGameState->GetStartingPlayerCount();

    if (!DMMatch::IsValidPlayerCount(StartingPlayerCount))
    {
        return false;
    }

    TArray<ADMRelicChest*> ActiveChests;
    RelicManager->GetActiveRelicChests(ActiveChests);

    // 참여 진영의 상자가 하나라도 누락되면 부분 공개하지 않는다.
    if (ActiveChests.Num() != StartingPlayerCount)
    {
        return false;
    }

    TSet<int32> SeenBaseSlots;
    OutState.BaseCounts.Reserve(StartingPlayerCount);

    for (const ADMRelicChest* Chest : ActiveChests)
    {
        if (!IsValid(Chest))
        {
            OutState = FDMOvertimeIntroState();
            return false;
        }

        const int32 BaseSlot = Chest->GetBaseSlot();

        if (!DMMatch::IsValidBaseSlot(BaseSlot) || SeenBaseSlots.Contains(BaseSlot))
        {
            OutState = FDMOvertimeIntroState();
            return false;
        }

        SeenBaseSlots.Add(BaseSlot);

        const int32 RegisteredCount = RelicManager->GetRegisteredRelicCount(BaseSlot);

        if (RegisteredCount < 0 || RegisteredCount > RelicManager->GetTotalRelicCount())
        {
            OutState = FDMOvertimeIntroState();
            return false;
        }

        FDMOvertimeBaseCount Entry;
        Entry.BaseSlot = BaseSlot;
        Entry.RegisteredRelicCount = RegisteredCount;

        OutState.BaseCounts.Add(Entry);
    }

    // 서버의 Registry 순서가 UI의 진영 순서를 결정하지 않게 한다.
    OutState.BaseCounts.Sort(
        [](const FDMOvertimeBaseCount& A, const FDMOvertimeBaseCount& B)
        {
            return A.BaseSlot < B.BaseSlot;
        }
    );

    return OutState.BaseCounts.Num() == StartingPlayerCount;
}

bool ADMMatchGameMode::TryStartOvertimeIntro()
{
    ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    if (!ensureMsgf(IsValid(MatchGameState), TEXT("Start OvertimeIntro failed: MatchGameState is unavailable.")))
    {
        return false;
    }

    if (!ensureMsgf(IsValid(MatchRuleData), TEXT("Start OvertimeIntro failed: MatchRuleData is unavailable.")))
    {
        return false;
    }

    if (MatchGameState->GetCurrentPhase() != EDMMatchPhase::Playing)
    {
        UE_LOG(
            LogDMMatchGameMode,
            Warning,
            TEXT("Start OvertimeIntro rejected: current phase is not Playing. CurrentPhase=%s"),
            *UEnum::GetValueAsString(MatchGameState->GetCurrentPhase())
        );

        return false;
    }

    if (MatchGameState->GetMatchEndReason() != EDMMatchEndReason::None)
    {
        UE_LOG(
            LogDMMatchGameMode,
            Warning,
            TEXT("Start OvertimeIntro rejected: match result is already locked. Reason=%s"),
            *UEnum::GetValueAsString(MatchGameState->GetMatchEndReason())
        );

        return false;
    }

    /*
     * 정규 경기 시간이 실제로 끝난 이후에만
     * 연장전 진입을 허용한다.
     */
    const double RemainingRegularTime = MatchGameState->GetRemainingRegularTime();

    if (RemainingRegularTime > KINDA_SMALL_NUMBER)
    {
        UE_LOG(
            LogDMMatchGameMode,
            Warning,
            TEXT("Start OvertimeIntro rejected: regular time has not expired. Remaining=%.3f"),
            RemainingRegularTime
        );

        return false;
    }

    const float OvertimeIntroDuration = MatchRuleData->OvertimeIntroDuration;

    if (!ensureMsgf(
            OvertimeIntroDuration > 0.0f,
            TEXT("Start OvertimeIntro failed: OvertimeIntroDuration must be positive. Duration=%.3f"),
            OvertimeIntroDuration
        ))
    {
        return false;
    }

    // 연장전 진입 전에 진영별 유물 수를 한 번 확정한다.
    FDMOvertimeIntroState IntroSnapshot;

    if (!BuildOvertimeIntroState(IntroSnapshot))
    {
        UE_LOG(LogDMMatchGameMode, Error, TEXT("OvertimeIntro failed: registered relic snapshot is incomplete."));

        return false;
    }

    GetWorldTimerManager().ClearTimer(PhaseTimerHandle);

    const double ServerNow = MatchGameState->GetServerWorldTimeSeconds();

    const double OvertimeIntroEndServerTime = ServerNow + OvertimeIntroDuration;

    GetWorldTimerManager().SetTimer(
        PhaseTimerHandle, this, &ADMMatchGameMode::HandleOvertimeIntroExpired, OvertimeIntroDuration, false
    );

    if (!GetWorldTimerManager().IsTimerActive(PhaseTimerHandle))
    {
        UE_LOG(LogDMMatchGameMode, Error, TEXT("Start OvertimeIntro failed: PhaseTimer was not activated."));

        return false;
    }

    if (!TryTransitionPhase(EDMMatchPhase::OvertimeIntro, OvertimeIntroEndServerTime))
    {
        GetWorldTimerManager().ClearTimer(PhaseTimerHandle);

        return false;
    }

    // Phase가 확정된 후 같은 종료 시각을 가진 Snapshot을 공개한다.
    IntroSnapshot.IntroEndServerTime = OvertimeIntroEndServerTime;

    MatchGameState->SetOvertimeIntroState(IntroSnapshot);

    if (!MatchGameState->GetOvertimeIntroState().HasPayload())
    {
        UE_LOG(LogDMMatchGameMode, Error, TEXT("OvertimeIntro state publication failed."));
    }

    /*
     * OvertimeIntro부터 Quest는 존재하지 않는다.
     *
     * Active / FailedPending Quest도 여기서 종료하여
     * Overtime으로 가져가지 않는다.
     */
    ShutdownRandomEventSystem();

    ShutdownQuestSystem();

    UE_LOG(
        LogDMMatchGameMode,
        Log,
        TEXT("OvertimeIntro started. Duration=%.3f, ServerNow=%.3f, EndServerTime=%.3f"),
        OvertimeIntroDuration,
        ServerNow,
        OvertimeIntroEndServerTime
    );

    return true;
}

void ADMMatchGameMode::HandleOvertimeIntroExpired()
{
    ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    if (!ensureMsgf(IsValid(MatchGameState), TEXT("HandleOvertimeIntroExpired failed: MatchGameState is unavailable.")))
    {
        return;
    }

    if (MatchGameState->GetCurrentPhase() != EDMMatchPhase::OvertimeIntro)
    {
        UE_LOG(
            LogDMMatchGameMode,
            Verbose,
            TEXT("OvertimeIntro expiration ignored: current phase is no longer OvertimeIntro.")
        );

        return;
    }

    UE_LOG(LogDMMatchGameMode, Log, TEXT("OvertimeIntro expired. Transitioning to Overtime."));

    if (!StartOvertime())
    {
        AbortMatchForInternalFailure(TEXT("연장전 시작에 실패했습니다."));
    }
}

bool ADMMatchGameMode::StartOvertime()
{
    ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    if (!ensureMsgf(IsValid(MatchGameState), TEXT("StartOvertime failed: MatchGameState is unavailable.")))
    {
        return false;
    }

    if (!ensureMsgf(IsValid(MatchRuleData), TEXT("StartOvertime failed: MatchRuleData is unavailable.")))
    {
        return false;
    }

    if (MatchGameState->GetCurrentPhase() != EDMMatchPhase::OvertimeIntro)
    {
        UE_LOG(LogDMMatchGameMode, Warning, TEXT("StartOvertime rejected: current phase is not OvertimeIntro."));

        return false;
    }

    const float OvertimeDuration = MatchRuleData->OvertimeDuration;

    if (!ensureMsgf(
            OvertimeDuration > 0.0f,
            TEXT("StartOvertime failed: OvertimeDuration must be positive. Duration=%.3f"),
            OvertimeDuration
        ))
    {
        return false;
    }

    GetWorldTimerManager().ClearTimer(PhaseTimerHandle);

    const double ServerNow = MatchGameState->GetServerWorldTimeSeconds();

    const double OvertimeEndServerTime = ServerNow + OvertimeDuration;

    GetWorldTimerManager().SetTimer(
        PhaseTimerHandle, this, &ADMMatchGameMode::HandleOvertimeExpired, OvertimeDuration, false
    );

    if (!GetWorldTimerManager().IsTimerActive(PhaseTimerHandle))
    {
        UE_LOG(LogDMMatchGameMode, Error, TEXT("StartOvertime failed: PhaseTimer was not activated."));

        return false;
    }

    if (!TryTransitionPhase(EDMMatchPhase::Overtime, OvertimeEndServerTime))
    {
        GetWorldTimerManager().ClearTimer(PhaseTimerHandle);

        return false;
    }

    UE_LOG(
        LogDMMatchGameMode,
        Log,
        TEXT("Overtime started. Duration=%.3f, ServerNow=%.3f, EndServerTime=%.3f"),
        OvertimeDuration,
        ServerNow,
        OvertimeEndServerTime
    );

    return true;
}

void ADMMatchGameMode::HandleOvertimeExpired()
{
    ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    if (!ensureMsgf(IsValid(MatchGameState), TEXT("Overtime expiry handling failed: GameState is invalid.")))
    {
        return;
    }

    // 진행 중 승리가 확정된 경우에는 시간 만료 결과를 덮어쓰지 않는다.
    if (MatchGameState->GetCurrentPhase() != EDMMatchPhase::Overtime
        || MatchGameState->GetMatchEndReason() != EDMMatchEndReason::None)
    {
        return;
    }

    const FDMMatchPhaseState PhaseState = MatchGameState->GetPhaseState();

    const double RemainingTime = PhaseState.PhaseEndServerTime - MatchGameState->GetServerWorldTimeSeconds();

    if (RemainingTime > 0.0)
    {
        GetWorldTimerManager().SetTimer(
            PhaseTimerHandle,
            this,
            &ThisClass::HandleOvertimeExpired,
            FMath::Max(static_cast<float>(RemainingTime), KINDA_SMALL_NUMBER),
            false
        );

        return;
    }

    GetWorldTimerManager().ClearTimer(PhaseTimerHandle);

    ResolveOvertimeEnd();
}

void ADMMatchGameMode::ResolveOvertimeEnd()
{
    const ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    int32 WinningBaseSlot = INDEX_NONE;

    // 판정 실패(참가자 불일치, RelicManager 없음 등)는 동률이 아니다.
    if (!TryResolveOvertimeWinner(WinningBaseSlot))
    {
        AbortMatchForInternalFailure(TEXT("연장전 승자 판정에 실패했습니다."));
        return;
    }

    const EDMMatchEndReason EndReason =
        WinningBaseSlot != INDEX_NONE ? EDMMatchEndReason::OvertimeVictory : EDMMatchEndReason::OvertimeDraw;

    if (TryFinishMatch(EndReason, WinningBaseSlot))
    {
        return;
    }

    // 다른 경로가 먼저 결과를 확정했다면 정상이다.
    if (IsValid(MatchGameState) && MatchGameState->GetMatchEndReason() != EDMMatchEndReason::None
        && MatchGameState->GetCurrentPhase() == EDMMatchPhase::Finished)
    {
        return;
    }

    AbortMatchForInternalFailure(FString::Printf(
        TEXT("연장전 종료 결과 적용 실패. Reason=%s, Winner=%d"), *UEnum::GetValueAsString(EndReason), WinningBaseSlot
    ));
}

#if !UE_BUILD_SHIPPING
void ADMMatchGameMode::DebugResolveOvertimeEnd()
{
    const ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    if (!IsValid(MatchGameState) || MatchGameState->GetCurrentPhase() != EDMMatchPhase::Overtime
        || MatchGameState->GetMatchEndReason() != EDMMatchEndReason::None)
    {
        UE_LOG(LogDMMatchGameMode, Warning, TEXT("DebugResolveOvertimeEnd rejected: only available during Overtime."));
        return;
    }

    GetWorldTimerManager().ClearTimer(PhaseTimerHandle);
    ResolveOvertimeEnd();
}
#endif

#pragma endregion
