#pragma once

#include "CoreMinimal.h"
#include "Core/DMMatchTypes.h"
#include "Quests/DMQuestTypes.h"
#include "TimerManager.h"
#include "UObject/Object.h"
#include "DMQuestDirector.generated.h"

class ADMMatchGameMode;
class ADMMatchGameState;
class ADMMatchPlayerState;
class UDMMatchRuleDataAsset;
class UDMQuestProgressComponent;
class UDataTable;

/**
 * Director가 확정한 공개 상태를 소유 GameMode가 GameState에 반영한다.
 * 반영에 실패하면 false를 돌려주며, Director는 이를 보고 시작을 롤백한다.
 */
DECLARE_DELEGATE_RetVal_OneParam(bool, FDMQuestPublicStatePublisher, const FDMQuestRuntimeState&);

/**
 * Match 단위 Global Quest 실행의 서버 권위 원본.
 *
 * ADMMatchGameMode가 Outer이자 소유자이며
 * Client에는 생성/복제되지 않는다.
 *
 * 책임:
 * - Quest 발생 시도 스케줄
 * - Quest Definition 선택
 * - Global RunId
 * - 참가자 Snapshot
 * - Deadline
 * - 참가자 LifeState 대응
 * - Global Run 종료 판정
 *
 * 개인 Progress 계산은
 * UDMQuestProgressComponent가 담당한다.
 */
UCLASS()
class DONTMELT_API UDMQuestDirector : public UObject
{
    GENERATED_BODY()

public:
#pragma region Lifecycle

    bool Initialize(
        ADMMatchGameMode* InOwnerGameMode,
        const UDMMatchRuleDataAsset* InMatchRuleData,
        UDataTable* InQuestDefinitionTable,
        FDMQuestPublicStatePublisher InPublicStatePublisher
    );

    /**
     * Match가 Playing에 진입한 뒤
     * Quest 발생 Timer를 등록한다.
     */
    bool StartPlaying();

    /**
     * Match 종료 / Aborted / Overtime 진입 시
     * 모든 Quest runtime을 정리한다.
     */
    void Shutdown();

#pragma endregion

#pragma region Query

    bool HasActiveQuest() const
    {
        return ActiveQuestState.IsActive();
    }

    const FDMQuestRuntimeState& GetActiveQuestState() const
    {
        return ActiveQuestState;
    }

#pragma endregion

private:
#pragma region Initialization

    bool CacheQuestDefinitions();

    bool ValidateQuestDefinition(const FDMQuestDefinitionRow& Definition) const;

    bool BindPlayerDelegates();

    void UnbindPlayerDelegates();

    bool BindQuestActionSource(ADMMatchPlayerState* MatchPlayerState);

    void UnbindQuestActionSource(ADMMatchPlayerState* MatchPlayerState);

#pragma endregion

#pragma region Scheduling

    void HandleQuestAttempt(int32 AttemptIndex);

    bool TryStartQuest(int32 AttemptIndex);

    bool CollectAllAliveParticipants(TArray<ADMMatchPlayerState*>& OutParticipants) const;

    void ClearQuestAttemptTimers();

#pragma endregion

#pragma region Active Run

    void HandleQuestActionEvent(const FDMQuestActionEvent& ActionEvent, ADMMatchPlayerState* MatchPlayerState);

    void HandleQuestDeadlineExpired();

    void HandlePlayerLifeStateChanged(ADMMatchPlayerState* MatchPlayerState, EDMPlayerLifeState NewLifeState);

    void HandleParticipantQuestProgressChanged(
        UDMQuestProgressComponent* QuestProgressComponent, FDMQuestPlayerProgress NewQuestProgress
    );

    void TryFinishActiveQuestRun();

    /**
     * 제한시간 내 완료자의 CompletionServerTime을 기준으로
     * 최종 Rank를 확정한다.
     */
    void FinalizeCompletionRanks();

    // 시간 초과 참가자의 사이렌 공개 상태를 켠다. 사운드는 Character 설정을 사용한다.
    void StartFailureWarningForParticipant(ADMMatchPlayerState* MatchPlayerState);

    void StopFailureWarningForParticipant(ADMMatchPlayerState* MatchPlayerState);

    void StopAllFailureWarnings();

    bool AreAllActiveParticipantsResolved() const;

    void FinishActiveQuestRun();

    void RollbackActiveQuestStart();

#pragma endregion

#pragma region Helpers

    ADMMatchGameState* GetMatchGameState() const;

    /** 공개 상태를 소유 GameMode가 연결한 경로로 반영한다. */
    bool PublishPublicState(const FDMQuestRuntimeState& NewPublicState) const;

    const FDMQuestDefinitionRow* GetActiveQuestDefinition() const;

    bool IsActiveParticipant(const ADMMatchPlayerState* MatchPlayerState) const;

    static bool IsTerminalParticipantState(EDMQuestParticipantState State);

#pragma endregion

#pragma region Configuration Cache

    UPROPERTY()
    TObjectPtr<UDataTable> QuestDefinitionTable = nullptr;

    TArray<FDMQuestDefinitionRow> CachedQuestDefinitions;

    int32 QuestMinCount = 0;

    int32 QuestMaxCount = 0;

    float MatchDuration = 0.0f;

    TArray<float> QuestAttemptTimes;

#pragma endregion

#pragma region Runtime State

    TWeakObjectPtr<ADMMatchGameMode> OwnerGameMode;

    FDMQuestPublicStatePublisher PublicStatePublisher;

    FDMQuestRuntimeState ActiveQuestState;

    int32 ActiveDefinitionIndex = INDEX_NONE;

    TArray<TWeakObjectPtr<ADMMatchPlayerState>> ActiveParticipants;

    /**
     * 이번 경기에서 등록할 Quest 발생 시도 수.
     *
     * 2~3 중 서버가 Match 시작 시 결정한다.
     */
    int32 ResolvedQuestAttemptCount = 0;

    /**
     * 실제 성공적으로 시작한 Quest 수.
     */
    int32 StartedQuestCount = 0;

    /**
     * Global Quest Run의 monotonically increasing ID.
     */
    int32 NextRunId = 1;

    TArray<FTimerHandle> QuestAttemptTimerHandles;

    FTimerHandle QuestDeadlineTimerHandle;

    /**
     * Rank Commit 중 QuestProgress delegate가
     * 동기 재진입하는 것을 막는 실행 guard.
     *
     * Gameplay state가 아니므로 복제하지 않는다.
     */
    bool bIsFinalizingCompletionRanks = false;

#pragma endregion
};