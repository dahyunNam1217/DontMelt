#pragma once

#include "CoreMinimal.h"
#include "Events/DMRandomEventTypes.h"
#include "TimerManager.h"
#include "UObject/Object.h"
#include "DMRandomEventDirector.generated.h"

class ADMMatchGameMode;
class ADMMatchGameState;
class ADMMatchPlayerState;

class UDMMatchRuleDataAsset;
class UDMRandomEventSetDataAsset;

/**
 * Director가 확정한 공개 상태를 소유 GameMode가 GameState에 반영한다.
 * 반영에 실패하면 false를 돌려주며, Director는 이를 보고 시작을 롤백한다.
 */
DECLARE_DELEGATE_RetVal_OneParam(bool, FDMRandomEventPublicStatePublisher, const FDMRandomEventRuntimeState&);

/**
 * Match 단위 Random Event 실행의 서버 권위 원본.
 *
 * ADMMatchGameMode가 소유하며 Client에는 존재하지 않는다.
 *
 * 책임:
 * - Event 발생 시도 Schedule
 * - Event Definition 선택
 * - Warning / Active lifecycle
 * - Event 간 최소 간격
 * - 동일 Event 연속 발생 방지
 * - Quest와의 상호배제
 *
 * Event별 실제 Gameplay Effect는
 * 해당 Event handler에서 수행한다.
 */
UCLASS()
class DONTMELT_API UDMRandomEventDirector : public UObject
{
    GENERATED_BODY()

public:
#pragma region Lifecycle

    bool Initialize(
        ADMMatchGameMode* InOwnerGameMode,
        const UDMMatchRuleDataAsset* InMatchRuleData,
        const UDMRandomEventSetDataAsset* InEventSetData,
        FDMRandomEventPublicStatePublisher InPublicStatePublisher
    );

    /**
     * Playing 진입 후
     * Random Event 발생 시도 Timer를 등록한다.
     */
    bool StartPlaying();

    /**
     * Match 종료 또는 Overtime 진입 시
     * 모든 Event runtime을 정리한다.
     */
    void Shutdown();

#pragma endregion

#pragma region Query

    bool HasScheduledOrActiveEvent() const
    {
        return ActiveEventState.IsScheduledOrActive();
    }

    const FDMRandomEventRuntimeState& GetActiveEventState() const
    {
        return ActiveEventState;
    }

#pragma endregion

private:
#pragma region Initialization

    bool CacheEventDefinitions(const UDMRandomEventSetDataAsset* EventSetData);

    bool ValidateEventDefinition(const FDMRandomEventDefinition& Definition) const;

    bool ValidateSchedule() const;

#pragma endregion

#pragma region Scheduling

    void HandleEventAttempt(int32 AttemptIndex);

    bool TryScheduleEvent(int32 AttemptIndex);

    int32 SelectNextEventDefinitionIndex() const;

    bool AreAllPlayersAlive() const;

    void ClearEventAttemptTimers();

#pragma endregion

#pragma region Active Event

    void HandleWarningFinished();

    void HandleEventFinished();

    void CancelScheduledEvent(const TCHAR* Reason);

    void ClearActiveEventTimers();

#pragma endregion

#pragma region Input Reverse

    /**
     * 현재 Alive Player들에게
     * 이동 입력 반전 Status를 적용한다.
     *
     * Status 원본은 각 CharacterStateComponent다.
     */
    bool ApplyInputReverseEffect(float EffectDuration);

    /**
     * 현재 Player Character에 남아 있는
     * 이동 입력 반전 Status를 제거한다.
     */
    void RemoveInputReverseEffect();

#pragma endregion

#pragma region Helpers

    ADMMatchGameState* GetMatchGameState() const;

    /** 공개 상태를 소유 GameMode가 연결한 경로로 반영한다. */
    bool PublishPublicState(const FDMRandomEventRuntimeState& NewPublicState) const;

#pragma endregion

#pragma region Configuration Cache

    TArray<FDMRandomEventDefinition> CachedEventDefinitions;

    int32 RandomEventMinCount = 0;

    int32 RandomEventMaxCount = 0;

    TArray<float> EventAttemptTimes;

    float MinimumEventStartInterval = 0.0f;

    float MatchDuration = 0.0f;

#pragma endregion

#pragma region Runtime State

    TWeakObjectPtr<ADMMatchGameMode> OwnerGameMode;

    FDMRandomEventPublicStatePublisher PublicStatePublisher;

    FDMRandomEventRuntimeState ActiveEventState;

    int32 ActiveDefinitionIndex = INDEX_NONE;

    int32 ResolvedEventAttemptCount = 0;

    /**
     * 마지막으로 실제 Active Window까지 진입한 Event.
     *
     * Warning 중 Cancel된 Event는 여기에 기록하지 않는다.
     */
    FGameplayTag PreviousEventTag;

    /**
     * 마지막 실제 Event 시작의 논리적 ServerTime.
     */
    double LastStartedEventServerTime = 0.0;

    TArray<FTimerHandle> EventAttemptTimerHandles;

    FTimerHandle WarningTimerHandle;

    FTimerHandle EventEndTimerHandle;

#pragma endregion
};