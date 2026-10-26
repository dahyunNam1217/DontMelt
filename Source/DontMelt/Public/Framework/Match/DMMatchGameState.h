#pragma once

#include "CoreMinimal.h"
#include "Core/DMMatchTypes.h"
#include "GameFramework/GameStateBase.h"
#include "Events/DMRandomEventTypes.h"
#include "Quests/DMQuestTypes.h"

#include "DMMatchGameState.generated.h"

class ADMMatchGameMode;
class ADMMatchGameState;

DECLARE_MULTICAST_DELEGATE_TwoParams(FDMOnMatchPhaseStateChanged, ADMMatchGameState*, FDMMatchPhaseState);

DECLARE_MULTICAST_DELEGATE_TwoParams(FDMOnStartingPlayerCountChanged, ADMMatchGameState*, int32);

DECLARE_MULTICAST_DELEGATE_TwoParams(FDMOnRequiredRelicCountChanged, ADMMatchGameState*, int32);

DECLARE_MULTICAST_DELEGATE_TwoParams(FDMOnMatchResultChanged, ADMMatchGameState*, FDMMatchResult);

DECLARE_MULTICAST_DELEGATE_TwoParams(FDMOnQuestPublicStateChanged, ADMMatchGameState*, FDMQuestRuntimeState);

DECLARE_MULTICAST_DELEGATE_TwoParams(
    FDMOnRandomEventPublicStateChanged, ADMMatchGameState*, FDMRandomEventRuntimeState
);

/** 유물 위치 공개용 Snapshot 변경 알림. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FDMOnRelicRevealChanged, ADMMatchGameState*, FDMRelicRevealState);

/** 연장전 진입 시 공개되는 진영별 유물 개수 변경 알림. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FDMOnOvertimeIntroStateChanged, ADMMatchGameState*, FDMOvertimeIntroState);

/**
 * 모든 참가자에게 공개되는 Match 런타임 상태를 소유한다.
 *
 * MatchGameMode가 서버에서 상태를 결정하고,
 * MatchGameState가 그 결과를 전체 클라이언트에 복제한다.
 */
UCLASS()
class DONTMELT_API ADMMatchGameState : public AGameStateBase
{
    GENERATED_BODY()

    friend class ADMMatchGameMode;

public:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintPure, Category = "DM|Match|State")
    FDMMatchPhaseState GetPhaseState() const
    {
        return PhaseState;
    }

    UFUNCTION(BlueprintPure, Category = "DM|Match|State")
    EDMMatchPhase GetCurrentPhase() const
    {
        return PhaseState.CurrentPhase;
    }

    UFUNCTION(BlueprintPure, Category = "DM|Match|Players")
    int32 GetStartingPlayerCount() const
    {
        return StartingPlayerCount;
    }

    UFUNCTION(BlueprintPure, Category = "DM|Match|Relic")
    int32 GetRequiredRelicCount() const
    {
        return RequiredRelicCount;
    }

    UFUNCTION(BlueprintPure, Category = "DM|Match|Players")
    int32 GetCurrentPlayerStateCount() const;

    UFUNCTION(BlueprintPure, Category = "DM|Match|Result")
    FDMMatchResult GetMatchResult() const
    {
        return MatchResult;
    }

    UFUNCTION(BlueprintPure, Category = "DM|Match|Result")
    EDMMatchEndReason GetMatchEndReason() const
    {
        return MatchResult.EndReason;
    }

    UFUNCTION(BlueprintPure, Category = "DM|Match|Result")
    int32 GetWinningBaseSlot() const
    {
        return MatchResult.WinningBaseSlot;
    }

    UFUNCTION(BlueprintPure, Category = "DM|Match|Quest")
    FDMQuestRuntimeState GetQuestPublicState() const
    {
        return QuestPublicState;
    }

    UFUNCTION(BlueprintPure, Category = "DM|Match|Quest")
    bool HasCurrentQuest() const
    {
        return QuestPublicState.IsActive();
    }

    UFUNCTION(BlueprintPure, Category = "DM|Match|Random Event")
    FDMRandomEventRuntimeState GetRandomEventPublicState() const
    {
        return RandomEventPublicState;
    }

    UFUNCTION(BlueprintPure, Category = "DM|Match|Random Event")
    bool HasCurrentRandomEvent() const
    {
        return RandomEventPublicState.IsScheduledOrActive();
    }

    UFUNCTION(BlueprintPure, Category = "DM|Match|Quest|Time")
    double GetRemainingQuestTime() const;

    UFUNCTION(BlueprintPure, Category = "DM|Match|Random Event|Time")
    double GetRemainingRandomEventWarningTime() const;

    UFUNCTION(BlueprintPure, Category = "DM|Match|Random Event|Time")
    double GetRemainingRandomEventTime() const;

    UFUNCTION(BlueprintPure, Category = "DM|Match|Time")
    double GetRemainingPhaseTime() const;

    UFUNCTION(BlueprintPure, Category = "DM|Match|Time")
    double GetRemainingRegularTime() const;

    UFUNCTION(BlueprintPure, Category = "DM|Match|Time")
    double GetRemainingOvertimeTime() const;

    /** 공개 허용 시간 중에만 위치가 채워지는 Snapshot을 반환한다. */
    FDMRelicRevealState GetRelicRevealState() const
    {
        return RelicRevealState;
    }

    FDMOnRelicRevealChanged& OnRelicRevealChanged()
    {
        return RelicRevealChangedDelegate;
    }

    /** 현재 연장전 Intro의 서버 확정 공개 정보. */
    FDMOvertimeIntroState GetOvertimeIntroState() const
    {
        return OvertimeIntroState;
    }

    /** 연장전 공개 정보가 바뀌었을 때 구독한다. */
    FDMOnOvertimeIntroStateChanged& OnOvertimeIntroStateChanged()
    {
        return OvertimeIntroStateChangedDelegate;
    }

    FDMOnMatchPhaseStateChanged& OnPhaseStateChanged()
    {
        return PhaseStateChangedDelegate;
    }

    FDMOnStartingPlayerCountChanged& OnStartingPlayerCountChanged()
    {
        return StartingPlayerCountChangedDelegate;
    }

    FDMOnRequiredRelicCountChanged& OnRequiredRelicCountChanged()
    {
        return RequiredRelicCountChangedDelegate;
    }

    FDMOnMatchResultChanged& OnMatchResultChanged()
    {
        return MatchResultChangedDelegate;
    }

    FDMOnQuestPublicStateChanged& OnQuestPublicStateChanged()
    {
        return QuestPublicStateChangedDelegate;
    }

    FDMOnRandomEventPublicStateChanged& OnRandomEventPublicStateChanged()
    {
        return RandomEventPublicStateChangedDelegate;
    }

private:
#pragma region Server Mutators

    /** 서버에서 Phase를 확정한다. 이미 같은 값이면 변경 없이 true를 반환한다. */
    bool TrySetPhaseState(const FDMMatchPhaseState& NewPhaseState);

    void SetStartingPlayerCount(int32 NewStartingPlayerCount);

    void SetRequiredRelicCount(int32 NewRequiredRelicCount);

    /** 서버에서 최종 결과를 한 번만 확정한다. 거절되면 false를 반환한다. */
    bool TrySetMatchResult(const FDMMatchResult& NewMatchResult);

    void SetQuestPublicState(const FDMQuestRuntimeState& NewQuestPublicState);

    void SetRandomEventPublicState(const FDMRandomEventRuntimeState& NewRandomEventPublicState);

    /** GameMode에서만 호출하는 서버 전용 공개 상태 변경 API. */
    void SetRelicRevealState(const FDMRelicRevealState& NewState);

    /** 서버가 연장전 Intro의 공개값을 설정하거나 초기화한다. */
    void SetOvertimeIntroState(const FDMOvertimeIntroState& NewState);

#pragma endregion

#pragma region Replication Callbacks

    UFUNCTION()
    void OnRep_PhaseState();

    UFUNCTION()
    void OnRep_StartingPlayerCount();

    UFUNCTION()
    void OnRep_RequiredRelicCount();

    UFUNCTION()
    void OnRep_MatchResult();

    UFUNCTION()
    void OnRep_QuestPublicState();

    UFUNCTION()
    void OnRep_RandomEventPublicState();

    UFUNCTION()
    void OnRep_RelicRevealState();

    UFUNCTION()
    void OnRep_OvertimeIntroState();

#pragma endregion

#pragma region Internal Notifications

    void HandlePhaseStateChanged();

    void HandleStartingPlayerCountChanged();

    void HandleRequiredRelicCountChanged();

    void HandleMatchResultChanged();

    void HandleQuestPublicStateChanged();

    void HandleRandomEventPublicStateChanged();

    void HandleRelicRevealChanged();

    void HandleOvertimeIntroStateChanged();

#pragma endregion

#pragma region Replicated State

    UPROPERTY(
        VisibleInstanceOnly,
        ReplicatedUsing = OnRep_PhaseState,
        BlueprintReadOnly,
        Category = "DM|Match|State",
        meta = (AllowPrivateAccess = "true")
    )
    FDMMatchPhaseState PhaseState;

    UPROPERTY(
        VisibleInstanceOnly,
        ReplicatedUsing = OnRep_StartingPlayerCount,
        BlueprintReadOnly,
        Category = "DM|Match|Players",
        meta = (AllowPrivateAccess = "true")
    )
    int32 StartingPlayerCount = 0;

    UPROPERTY(
        VisibleInstanceOnly,
        ReplicatedUsing = OnRep_RequiredRelicCount,
        BlueprintReadOnly,
        Category = "DM|Match|Relic",
        meta = (AllowPrivateAccess = "true")
    )
    int32 RequiredRelicCount = 0;

    UPROPERTY(
        VisibleInstanceOnly,
        ReplicatedUsing = OnRep_MatchResult,
        BlueprintReadOnly,
        Category = "DM|Match|Result",
        meta = (AllowPrivateAccess = "true")
    )
    FDMMatchResult MatchResult;

    UPROPERTY(
        VisibleInstanceOnly,
        ReplicatedUsing = OnRep_QuestPublicState,
        BlueprintReadOnly,
        Category = "DM|Match|Quest",
        meta = (AllowPrivateAccess = "true")
    )
    FDMQuestRuntimeState QuestPublicState;

    UPROPERTY(
        VisibleInstanceOnly,
        ReplicatedUsing = OnRep_RandomEventPublicState,
        BlueprintReadOnly,
        Category = "DM|Match|Random Event",
        meta = (AllowPrivateAccess = "true")
    )
    FDMRandomEventRuntimeState RandomEventPublicState;

    /** 공개 종료 시 데이터 배열까지 비워지는 임시 서버 Snapshot. */
    UPROPERTY(
        ReplicatedUsing = OnRep_RelicRevealState,
        VisibleInstanceOnly,
        BlueprintReadOnly,
        Category = "DM|Match|Reveal",
        meta = (AllowPrivateAccess = "true")
    )
    FDMRelicRevealState RelicRevealState;

    /** OvertimeIntro 동안만 유지되는 등록 유물 개수 Snapshot. */
    UPROPERTY(
        ReplicatedUsing = OnRep_OvertimeIntroState,
        VisibleInstanceOnly,
        BlueprintReadOnly,
        Category = "DM|Match|Overtime",
        meta = (AllowPrivateAccess = "true")
    )
    FDMOvertimeIntroState OvertimeIntroState;

#pragma endregion

#pragma region Delegates

    FDMOnMatchPhaseStateChanged PhaseStateChangedDelegate;

    FDMOnStartingPlayerCountChanged StartingPlayerCountChangedDelegate;

    FDMOnRequiredRelicCountChanged RequiredRelicCountChangedDelegate;

    FDMOnMatchResultChanged MatchResultChangedDelegate;

    FDMOnQuestPublicStateChanged QuestPublicStateChangedDelegate;

    FDMOnRandomEventPublicStateChanged RandomEventPublicStateChangedDelegate;

    FDMOnRelicRevealChanged RelicRevealChangedDelegate;

    FDMOnOvertimeIntroStateChanged OvertimeIntroStateChangedDelegate;

#pragma endregion
};
