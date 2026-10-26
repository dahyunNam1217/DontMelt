#pragma once

#include "CoreMinimal.h"
#include "Core/DMMatchTypes.h"
#include "Framework/Match/DMMatchResultRules.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"
#include "DMMatchGameMode.generated.h"

class ADMMatchGameState;
class ADMMatchPlayerState;
class ADMRelic;
class ADMRelicManager;
class APlayerStart;
class UDMMatchRuleDataAsset;
class UDMRelicRuleDataAsset;
class UDMSkillSetDataAsset;
class ADMBaseZone;
class ADMNPCCharacter;
class ADMPlayerCharacter;
class ADMMatchPlayerController;
class ADMRespawnPoint;
class UDMRespawnCoordinator;
class UDMNPCPopulationDirector;
class UDMQuestDirector;
class UDMRandomEventDirector;
class UDMRandomEventSetDataAsset;
class UDataTable;

struct FDMQuestRuntimeState;
struct FDMRandomEventRuntimeState;

/**
 * Match 서버의 권위 규칙 클래스.
 *
 * 참가자 입장, BaseSlot 할당, 초기 Spawn 검증,
 * Starting Roster 확정과 Match Phase 진행을 담당한다.
 */
UCLASS()
class DONTMELT_API ADMMatchGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
#if !UE_BUILD_SHIPPING
    // 실제 연장전 판정 규칙(승리/동률)을 그대로 실행한다.
    void DebugResolveOvertimeEnd();
#endif

#pragma region Lifecycle

    ADMMatchGameMode();

    /**
     * Lobby가 Travel URL로 전달한 ExpectedPlayers를 읽고
     * Match 시작에 필요한 서버 규칙 데이터를 검증한다.
     */
    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

    /**
     * InitGame 검증에 실패한 매치라면 진행하지 않고 InternalError로 중단한다.
     *
     * 원격 참가자는 PreLogin에서 거부되므로 이 시점에는 호스트만 남아 있다.
     */
    virtual void StartPlay() override;

    /** 대기 Timer 및 구독 종료. */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /**
     * Match 입장 가능 여부를 서버에서 검증한다.
     */
    virtual void PreLogin(
        const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage
    ) override;

    /**
     * PlayerController와 PlayerState 생성 후
     * 참가자에게 BaseSlot을 할당한다.
     */
    virtual void PostLogin(APlayerController* NewPlayer) override;

    /**
     * 원격 참가자의 실제 연결 종료라면
     * Match를 PlayerDisconnect 사유로 중단한다.
     */
    virtual void Logout(AController* Exiting) override;

#pragma endregion

#pragma region Initial Player Spawn

    /**
     * BaseSlot이 확정된 참가자의 초기 Pawn Spawn을 실행하고
     * 실제 Pawn과 Possess 결과를 검증한다.
     */
    virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

    /**
     * PlayerState의 BaseSlot에 대응하는 PlayerStart를 반환한다.
     */
    virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

#pragma endregion

#pragma region Relic Access

    /**
     * 서버 소유 RelicManager.
     *
     * GameMode 자체가 Server-only이므로
     * Client에서 접근하는 API가 아니다.
     */
    ADMRelicManager* GetRelicManager() const
    {
        return RelicManager;
    }

#pragma endregion

#pragma region NPC Reaction Bridge

    /**
     * 서버 Combat에서 확정된 NPC 오인 공격 사건을
     * NPC Population 시스템에 전달한다.
     *
     * AttackOrigin:
     * NPC가 도주해야 할 위협의 원점.
     *
     * HitNPCTargets:
     * 실제 이번 공격에 직접 맞은 NPC.
     */
    void RequestNPCWrongTargetReaction(const FVector& AttackOrigin, const TArray<ADMNPCCharacter*>& HitNPCTargets);

    /**
     * Global Panic 이벤트를
     * NPC Population 시스템으로 전달한다.
     *
     * 실제 Event 발생 조건/주기는
     * UDMRandomEventDirector가 소유한다.
     */
    void RequestNPCPanic();

#pragma endregion

#pragma region Player Death

    /**
     * 서버 권한으로 Player Death를 Commit한다.
     *
     * VictimCharacter:
     * 실제 사망 판정 대상.
     *
     * KillerCharacter:
     * 이번 Death를 발생시킨 공격자.
     * Spectator Context 생성에 사용하며,
     * Killer가 유효하지 않더라도 Death 자체를 되돌리지는 않는다.
     *
     * LifeState Alive -> Dead의 유일한 Death 진입점.
     */
    bool TryCommitPlayerDeath(ADMPlayerCharacter* VictimCharacter, ADMPlayerCharacter* KillerCharacter);
#pragma endregion

#pragma region Match Control

    /**
     * 정상적인 게임 결과로 Match를 종료한다.
     */
    bool TryFinishMatch(EDMMatchEndReason EndReason, int32 WinningBaseSlot = INDEX_NONE);

    /**
     * 정상적인 결과를 만들 수 없는 상황에서 Match를 중단한다.
     */
    bool TryAbortMatch(EDMMatchEndReason EndReason);

    void HandleRematchReadyRequest(ADMMatchPlayerController* RequestingController, bool bNewReady);

    void HandleReturnToLobbyRequest(ADMMatchPlayerController* RequestingController);

    void HandleRestartMatchRequest(ADMMatchPlayerController* RequestingController);

    // 각 클라이언트의 튜토리얼 완료를 서버에서 검사한다.
    void HandlePreMatchTutorialCompleted(ADMMatchPlayerController* RequestingController);

    /** 로컬 위젯 표시 결과를 기존 서버 GameMode에서 확인한다. */
    void HandlePreMatchTutorialDisplayReported(ADMMatchPlayerController* RequestingController, bool bSucceeded);

    // 호스트가 방을 떠나기 전에 다른 결과 이동을 차단한다.
    bool TryCommitHostLeaveToMainMenu(ADMMatchPlayerController* RequestingController);

    // 오류 복구가 시작되면 결과 화면의 다른 이동을 차단한다.
    void StopResultTravelForRecovery();

    /**
     * 정규 시간 종료 후 OvertimeIntro Phase 진입을 시도한다.
     */
    bool TryStartOvertimeIntro();

#pragma endregion

protected:
#pragma region Player Start Validation

    /**
     * 엔진이 PlayerStart를 최종 선택하기 전에
     * 참가자의 BaseSlot이 유효한지 검증한다.
     */
    virtual bool UpdatePlayerStartSpot(AController* Player, const FString& Portal, FString& OutErrorMessage) override;

#pragma endregion

private:
#pragma region Starting Roster

    /**
     * Starting Roster 구성이 이미 확정되어
     * 더 이상 변경할 수 없는지 반환한다.
     */
    bool IsStartingRosterLocked() const;

    /**
     * PlayerState에 아직 사용되지 않은 BaseSlot을 할당한다.
     */
    bool TryAssignBaseSlot(ADMMatchPlayerState* MatchPlayerState);

    /**
     * 현재 사용되지 않은 첫 BaseSlot을 반환한다.
     */
    int32 FindFirstAvailableBaseSlot() const;

    /**
     * BaseSlot에 정확히 대응하는 PlayerStart를 찾는다.
     *
     * 누락 또는 중복 Tag가 확인되면 nullptr을 반환한다.
     */
    APlayerStart* FindPlayerStartForBaseSlot(int32 BaseSlot) const;

    /**
     * 예상 인원 전원이 정상적으로 입장하고
     * 초기 Spawn까지 완료되었는지 검증한다.
     */
    bool IsStartingRosterComplete(FString& OutFailureReason) const;

#pragma endregion

#pragma region Player Death Steps

    /** 사망 확정이 가능한 상태인지 검사하고 Victim PlayerState를 반환한다. */
    bool CanCommitPlayerDeath(ADMPlayerCharacter* VictimCharacter, ADMMatchPlayerState*& OutVictimPlayerState) const;

    /** Kill 통계를 반영하고 LifeState를 Dead로 확정한다. */
    void CommitDeathState(ADMMatchPlayerState* VictimPlayerState, ADMPlayerCharacter* KillerCharacter);

    /** 진행 중인 상호작용을 정리한 뒤 소지 유물을 Drop한다. */
    void CleanupDeathInteractionAndRelic(ADMPlayerCharacter* VictimCharacter, ADMMatchPlayerState* VictimPlayerState);

    /** 전투·행동 상태를 정리하고 마지막으로 이동을 비활성화한다. */
    void CleanupDeathCombatAndMovement(ADMPlayerCharacter* VictimCharacter);

    /** 사망 래그돌 표현을 시작한다. */
    void StartDeathPresentation(ADMPlayerCharacter* VictimCharacter);

    /** 경기가 계속되면 Respawn Lifecycle과 관전 Sequence를 시작한다. */
    void StartPostDeathLifecycle(
        ADMPlayerCharacter* VictimCharacter,
        ADMMatchPlayerState* VictimPlayerState,
        ADMPlayerCharacter* KillerCharacter,
        const FVector& DeathLocation
    );

#pragma endregion

#pragma region Match Initialization

    /** InitGame에 필요한 에셋·클래스 참조와 SkillSet 구성을 검사한다. */
    bool ValidateRequiredAssets(FString& OutErrorMessage) const;

    /** MatchRuleData의 규칙값을 검사한다. */
    bool ValidateMatchRuleData(FString& OutErrorMessage) const;

    /** Lobby가 Travel URL로 전달한 ExpectedPlayers를 읽어 저장한다. */
    bool ParseExpectedPlayerCount(const FString& Options, FString& OutErrorMessage);

    /** Steam 승인 명단이 ExpectedPlayers와 일치하는지 검사하고 입장 기록을 초기화한다. */
    bool PrepareApprovedMatchRoster(FString& OutErrorMessage);

#pragma endregion

#pragma region Starting Roster Finalization

    /**
     * Starting Roster 검증이 완료되면
     * 시작 인원 Snapshot을 확정한다.
     */
    void TryFinalizeStartingRoster();

#pragma endregion

#pragma region Skill Lifecycle Helpers

    /**
     * Match Finished / Aborted 시
     * 모든 참가자의 transient Skill Runtime만 정리한다.
     */
    void CleanupAllSkillRuntimeForTerminalMatch();

#pragma endregion

#pragma region Relic Initialization

    /**
     * Starting Roster가 고정된 직후
     * Relic 규칙 Snapshot을 확정하고
     * 서버 RelicManager를 초기화한다.
     */
    bool InitializeRelicSystem();

    /**
     * Starting Roster의 BaseSlot을 기준으로
     * 실제 참가 Base만 Active로 설정한다.
     */
    bool ConfigureBaseZoneActivity(const TArray<ADMMatchPlayerState*>& MatchPlayers);

#pragma endregion

#pragma region NPC Population Initialization

    /**
     * Starting Roster가 고정된 뒤
     * 이번 Match의 NPC Population을 서버에서 생성한다.
     */
    bool InitializeNPCPopulation();

#pragma endregion

#pragma region Quest Initialization

    /**
     * Starting Roster가 확정된 뒤
     * 서버 전용 QuestDirector를 생성하고
     * Quest Definition / Participant delegate를 준비한다.
     */
    bool InitializeQuestSystem();

    /**
     * Match가 Quest를 허용하지 않는 상태로
     * 전환될 때 Quest runtime을 정리한다.
     */
    void ShutdownQuestSystem();

    /**
     * QuestDirector가 확정한 Global Quest 상태를
     * GameState Public Projection에 기록한다.
     *
     * QuestDirector Initialize 때 Publisher Delegate로 전달한다.
     */
    bool PublishQuestPublicState(const FDMQuestRuntimeState& NewQuestPublicState);

#pragma endregion

#pragma region Random Event Initialization

    /**
     * Starting Roster 확정 후
     * 서버 전용 RandomEventDirector를 준비한다.
     */
    bool InitializeRandomEventSystem();

    /**
     * Random Event의 Timer와 Public State를 정리한다.
     */
    void ShutdownRandomEventSystem();

    /**
     * RandomEventDirector의 권위 상태를
     * GameState Public Projection에 기록한다.
     *
     * RandomEventDirector Initialize 때 Publisher Delegate로 전달한다.
     */
    bool PublishRandomEventPublicState(const FDMRandomEventRuntimeState& NewRandomEventPublicState);

#pragma endregion

#pragma region Match Flow

    /**
     * Starting Roster 확정 후 Countdown Phase를 시작한다.
     */
    void StartCountdown();

    // Starting Roster 확정 후 모든 참가자의 튜토리얼을 시작한다.
    void StartPreMatchTutorialGate();
    void HandleStartingRosterTimeout();
    void HandlePreMatchTutorialTimeout();
    void FailStartingRosterFinalization(const FString& Diagnostic);
    void ClearPreMatchWaitingState();
    bool IsValidPreMatchTutorialParticipant(ADMMatchPlayerController* Controller) const;

    // 튜토리얼이 켜져 있으면 전원이 완료한 뒤에만 Countdown을 허용한다.
    UPROPERTY(EditDefaultsOnly, Category = "DM|Match|Tutorial")
    bool bEnablePreMatchTutorial = true;

    bool bPreMatchTutorialGateActive = false;
    TSet<TWeakObjectPtr<ADMMatchPlayerController>> TutorialCompletedControllers;
    TSet<TWeakObjectPtr<ADMMatchPlayerController>> TutorialDisplayedControllers;
    FTimerHandle StartingRosterTimeoutHandle;
    FTimerHandle PreMatchTutorialTimeoutHandle;

    // 참가자 입장과 초기 Spawn이 끝날 때까지 허용하는 최대 시간.
    UPROPERTY(EditDefaultsOnly, Category = "DM|Match|Startup", meta = (ClampMin = "5.0", UIMin = "5.0"))
    float StartingRosterTimeoutSeconds = 120.0f;

    // 페이지 읽기·스킵을 포함해 모든 참가자가 완료할 때까지 허용하는 최대 시간.
    UPROPERTY(EditDefaultsOnly, Category = "DM|Match|Tutorial", meta = (ClampMin = "10.0", UIMin = "10.0"))
    float PreMatchTutorialTimeoutSeconds = 300.0f;

    /**
     * Countdown 서버 Timer 만료 시 Playing 진입을 시도한다.
     */
    void HandleCountdownExpired();

    /**
     * Playing Phase에 진입하고
     * 정규 경기 종료 Timer를 시작한다.
     */
    void StartPlaying();

    /**
     * 정규 경기 시간 만료 시 기존 승리 조건을 확인한다.
     * 승자가 확정되지 않았다면 연장전 진입을 시도한다.
     */
    void HandleRegularTimeExpired();

    /** 종료 60초 전 공개 시작 및 3초 종료 처리. */
    void BeginRelicReveal();
    void EndRelicReveal();
    void CancelRelicReveal();

#pragma endregion

#pragma region Phase Transition

    /**
     * 모든 Match Phase 변경이 통과하는 중앙 전환 함수.
     */
    bool TryTransitionPhase(EDMMatchPhase TargetPhase, double PhaseEndServerTime);

    /**
     * 현재 Phase에서 Target Phase로의 전환이
     * 허용되는지 검증한다.
     */
    static bool IsPhaseTransitionAllowed(EDMMatchPhase FromPhase, EDMMatchPhase ToPhase);

#pragma endregion

#pragma region Match Result

    /**
     * RelicManager에서 Deposit Commit 완료 통지를 받는다.
     */
    void HandleRelicDepositCommitted();

    /**
     * Relic Steal Commit 완료 시
     * 모든 참가자의 개인 UI에
     * 익명 알림을 전달한다.
     */
    void HandleRelicStolen(ADMMatchPlayerState* StealingPlayerState);

    /**
     * 상태 변경 사건 하나가 Commit된 뒤
     * Match 결과를 중앙에서 한 번 평가한다.
     *
     * RelicVictory → EliminationVictory 순서로 검사한다.
     */
    bool EvaluateMatchResult();

    /**
     * 현재 Relic 상태를 기준으로
     * 승리 조건을 만족한 Base가 있는지 검사한다.
     *
     * 실제 Count의 SSOT는 RelicManager다.
     */
    bool CheckRelicVictory(int32& OutWinningBaseSlot, int32& OutWinningRegisteredRelicCount) const;

    /**
     * 현재 Player LifeState를 기준으로
     * Elimination Victory를 검사한다.
     *
     * Alive가 정확히 한 명일 때만 성공한다.
     */
    bool CheckEliminationVictory(
        int32& OutWinningBaseSlot, int32& OutConnectedParticipantCount, int32& OutAlivePlayerCount
    ) const;

    /**
     * 연장전 종료 시 등록 유물 단독 최다 참가자를 찾는다.
     *
     * 사망·리스폰 대기 중인 참가자도 포함한다.
     * 반환값은 결과 판정에 필요한 데이터가 유효했는지를 의미한다.
     * 정상 판정 후 동률이면 OutWinningBaseSlot은 INDEX_NONE이다.
     */
    bool TryResolveOvertimeWinner(int32& OutWinningBaseSlot) const;

    TArray<FDMMatchResultEntry> BuildFinalPlayerResults(int32 WinningBaseSlot) const;

    /** 승패 판정 입력용으로 PlayerState 값을 복사한다. 유효하지 않으면 미배정 참가자로 만든다. */
    FDMMatchParticipantSnapshot
    MakeParticipantSnapshot(const ADMMatchPlayerState* MatchPlayerState, bool bIncludeRegisteredRelicCount) const;

    /** 유효한 매치 PlayerState를 PlayerArray 순서대로 복사한다. */
    FDMMatchParticipantSnapshotList
    BuildParticipantSnapshots(const ADMMatchGameState* MatchGameState, bool bIncludeRegisteredRelicCount) const;

    /**
     * Finished에 사용할 수 있는 정상 종료 사유인지 검증한다.
     */
    static bool IsFinishReason(EDMMatchEndReason EndReason);

    /**
     * 현재 Match Phase에서 해당 정상 종료 사유를
     * 사용할 수 있는지 검증한다.
     */
    static bool IsFinishReasonAllowedForPhase(EDMMatchPhase Phase, EDMMatchEndReason EndReason);

    /**
     * Aborted에 사용할 수 있는 중단 사유인지 검증한다.
     */
    static bool IsAbortReason(EDMMatchEndReason EndReason);

    void StartResultAutoReturnTimer(double ResultServerTime);

    void HandleResultAutoReturnExpired();

    bool CommitServerTravel(const FString& TravelURL);

    // 결과 이동 실패를 로컬 오류 안내와 기본 맵 복구로 전달한다.
    void RecoverFromResultTravelFailure(const FString& Diagnostic);

#pragma endregion

#pragma region Overtime

    /**
     * OvertimeIntro Timer 만료 시 호출된다.
     */
    void HandleOvertimeIntroExpired();

    /**
     * Overtime Phase에 진입하고
     * Overtime 종료 Timer를 시작한다.
     */
    bool StartOvertime();

    /**
     * 연장전 종료 시 등록 유물 단독 최다 참가자를 승자로 확정한다.
     * 최다 동률이면 승자 없이 종료한다.
     * 사망·리스폰 대기 중인 참가자도 판정에 포함한다.
     */
    void HandleOvertimeExpired();

    /**
     * 활성 진영의 현재 등록 유물 수를 수집한다.
     * 일부 진영의 상자 연결이 유효하지 않으면 실패한다.
     */
    bool BuildOvertimeIntroState(FDMOvertimeIntroState& OutState) const;

#pragma endregion

#pragma region Configuration

    /**
     * Global Quest 정의 원본.
     *
     * BP_DMMatchGameMode에서
     * DT_QuestDefinitions를 연결한다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DM|Quest|Rules", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UDataTable> QuestDefinitionTable = nullptr;

    /**
     * Match에서 사용하는 Random Event 정의 원본.
     */
    UPROPERTY(
        EditDefaultsOnly, BlueprintReadOnly, Category = "DM|Random Event|Rules", meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<UDMRandomEventSetDataAsset> RandomEventSetData = nullptr;

    /**
     * Match 전체 Player가 공유하는
     * 공용 Skill Definition Set.
     *
     * Runtime cooldown은 이 Asset이 아니라
     * 각 PlayerState의 SkillComponent가 소유한다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DM|Skill|Rules", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UDMSkillSetDataAsset> SkillSetData = nullptr;

    /**
     * Match에서 사용하는 규칙 원본.
     *
     * BP_DMMatchGameMode에서 DA_DMMatchRules를 연결한다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DM|Match|Rules", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UDMMatchRuleDataAsset> MatchRuleData = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DM|Match|Travel", meta = (AllowPrivateAccess = "true"))
    TSoftObjectPtr<UWorld> LobbyMap;

    /**
     * Relic 수량과 상호작용 수치를 정의하는
     * Relic Rule Data Asset.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DM|Relic|Rules", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UDMRelicRuleDataAsset> RelicRuleData = nullptr;

    /**
     * 서버가 생성할 실제 Relic Actor Class.
     *
     * BP_DMRelic을 연결한다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DM|Relic", meta = (AllowPrivateAccess = "true"))
    TSubclassOf<ADMRelic> RelicClass;

    /**
     * 서버가 생성할 실제 NPC Character Class.
     *
     * BP_DMMatchGameMode에서
     * 플레이어와 동일 외형을 사용하는
     * BP_DMNPCCharacter를 연결한다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DM|NPC", meta = (AllowPrivateAccess = "true"))
    TSubclassOf<ADMNPCCharacter> NPCClass;

#pragma endregion

#pragma region Runtime State

    /**
     * Lobby가 Travel URL Option으로 전달한 예상 참가 인원.
     */
    int32 ExpectedPlayerCount = 0;

    /**
     * InitGame 검증 실패 여부.
     *
     * 엔진은 InitGame의 ErrorMessage로 맵 로드를 중단하지 않으므로
     * 이 값으로 입장 거부와 매치 중단을 직접 처리한다.
     */
    bool bMatchInitializationFailed = false;

    /**
     * 실제 입장 및 Spawn 검증 후 확정된 시작 인원.
     *
     * 0은 아직 Starting Roster가 확정되지 않았음을 의미한다.
     */
    int32 StartingPlayerCountSnapshot = 0;

    /**
     * 이번 Match 시작 시 고정된 NPC 배율.
     *
     * 경기 도중 플레이어가 연결 종료해도
     * 변경하지 않는다.
     */
    int32 ResolvedNPCMultiplierSnapshot = 0;

    /**
     * 이번 Match 시작 시 고정된 NPC 총수.
     *
     * StartingPlayerCountSnapshot
     * * ResolvedNPCMultiplierSnapshot
     * 으로 한 번 계산하며 경기 중 다시 계산하지 않는다.
     */
    int32 ResolvedNPCCountSnapshot = 0;

    /**
     * Match 시작 시 고정된
     * Elimination Victory 활성화 여부.
     */
    bool bEnableEliminationVictorySnapshot = false;

    /**
     * 이번 Match에서 고정된 전체 Relic 수.
     */
    int32 TotalRelicCountSnapshot = 0;

    /**
     * 이번 Match에서 승리에 필요한 등록 Relic 수.
     *
     * 시작 인원을 기준으로 한 번 결정한 뒤
     * 경기 도중 변경하지 않는다.
     */
    int32 RequiredRelicCountSnapshot = 0;

    /**
     * 이번 Match의 NPC Population 서버 관리자.
     *
     * GameMode를 Outer로 가지며
     * Client에는 존재하지 않는 서버 전용 Runtime Object다.
     */
    UPROPERTY()
    TObjectPtr<UDMNPCPopulationDirector> NPCPopulationDirector = nullptr;

    /**
     * 이번 Match의 서버 전용 Global Quest 관리자.
     *
     * GameMode를 Outer로 사용하며
     * Client에는 존재하지 않는다.
     */
    UPROPERTY()
    TObjectPtr<UDMQuestDirector> QuestDirector = nullptr;

    /**
     * 이번 Match의 서버 전용 Random Event 관리자.
     */
    UPROPERTY()
    TObjectPtr<UDMRandomEventDirector> RandomEventDirector = nullptr;

    /**
     * 이번 Match의 서버 전용 RelicManager.
     */
    UPROPERTY()
    TObjectPtr<ADMRelicManager> RelicManager = nullptr;

    FTimerHandle ResultAutoReturnTimerHandle;

    bool bResultTravelCommitted = false;

    FString CurrentMatchMapPackageName;

    /**
     * 현재 Phase 종료를 담당하는 서버 Timer Handle.
     *
     * Countdown, Playing, OvertimeIntro, Overtime에서
     * 동일 Handle을 순차적으로 재사용한다.
     */
    FTimerHandle PhaseTimerHandle;

    /** 정규 시간 중 공개 시작·종료 타이머. PhaseTimer와 별개다. */
    FTimerHandle RelicRevealStartTimerHandle;
    FTimerHandle RelicRevealEndTimerHandle;

    /** 사망 후 리스폰 타이머와 부활 처리를 담당한다. InitGame에서 생성한다. */
    UPROPERTY(Transient)
    TObjectPtr<UDMRespawnCoordinator> RespawnCoordinator = nullptr;

    /** Respawn 실패 시 재시도 간격(초). RespawnCoordinator에 전달한다. */
    UPROPERTY(EditDefaultsOnly, Category = "DM|Match|Respawn", meta = (ClampMin = "0.1"))
    float RespawnRetryDelaySeconds = 1.0f;

#pragma endregion

public:
    void HandleMatchResultAcknowledged(ADMMatchPlayerController* RequestingController, const FGuid& ResultId);

private:
    bool AreAllRemoteResultsAcknowledged() const;
    void TryCommitPendingResultTravel();
    void HandleResultAckTimeout();
    // 실제 ServerTravel 호출만 담당한다.
    bool ExecuteServerTravel(const FString& TravelURL);

    TSet<TWeakObjectPtr<APlayerController>> ResultAcknowledgedControllers;
    FString PendingResultTravelURL;
    bool bResultTravelPending = false;
    FTimerHandle ResultAckTimeoutHandle;

    // 응답하지 않는 Client 때문에 영구 대기하지 않기 위한 상한이다.
    // 이 값은 수신을 보장하는 수단이 아니다.
    UPROPERTY(EditDefaultsOnly, Category = "DM|Match|Result", meta = (ClampMin = "1.0"))
    float ResultAckTimeoutSeconds = 5.0f;

    // 내부 실패 시 중단 결과 → 실패하면 기본 맵 복구
    void AbortMatchForInternalFailure(const FString& Diagnostic);

    // 연장전 종료 판정. HandleOvertimeExpired와 테스트 명령이 함께 사용한다.
    void ResolveOvertimeEnd();
};