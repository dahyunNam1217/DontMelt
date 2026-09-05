// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/DMMatchTypes.h"
#include "GameFramework/GameStateBase.h"
#include "DMMatchGameState.generated.h"

/**
 * 모든 매치 참가자에게 공개 가능한 상태만 저장할 GameState.
 *
 * 승리 판정이나 서버 전용 원본 데이터는 소유하지 않는다.
 */


class ADMMatchGameState;
class ADMMatchGameMode;

/**
 * 공개 Match Phase 상태가 변경됐을 때 호출된다.
 *
 * 첫 번째 인자: 상태가 변경된 MatchGameState
 * 두 번째 인자: 새로운 PhaseState
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(
    FDMOnMatchPhaseStateChanged,
    ADMMatchGameState*,
    FDMMatchPhaseState
);

/**
 * Match 시작 인원이 확정됐을 때 호출된다.
 *
 * 첫 번째 인자: MatchGameState
 * 두 번째 인자: 확정된 시작 인원
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(
    FDMOnStartingPlayerCountChanged,
    ADMMatchGameState*,
    int32
);

/**
 * Match 종료 원인이 확정됐을 때 호출된다.
 *
 * 첫 번째 인자: MatchGameState
 * 두 번째 인자: 새로운 종료 원인
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(
    FDMOnMatchEndReasonChanged,
    ADMMatchGameState*,
    EDMMatchEndReason
);

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
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    /** 현재 Match Phase 상태를 반환한다. */
    UFUNCTION(BlueprintPure, Category = "DM|Match|State")
    FDMMatchPhaseState GetPhaseState() const
    {
        return PhaseState;
    }

    /** 현재 Match Phase만 반환한다. */
    UFUNCTION(BlueprintPure, Category = "DM|Match|State")
    EDMMatchPhase GetCurrentPhase() const
    {
        return PhaseState.CurrentPhase;
    }

    /** Match 시작 시점에 고정된 참가자 수를 반환한다. */
    UFUNCTION(BlueprintPure, Category = "DM|Match|Players")
    int32 GetStartingPlayerCount() const
    {
        return StartingPlayerCount;
    }

    /** 현재 Match 종료 원인을 반환한다. */
    UFUNCTION(BlueprintPure, Category = "DM|Match|Result")
    EDMMatchEndReason GetMatchEndReason() const
    {
        return MatchEndReason;
    }

    /**
     * 현재 연결된 Match PlayerState 수를 반환한다.
     *
     * 별도 캐시를 소유하지 않고 PlayerArray에서 계산한다.
     */
    UFUNCTION(BlueprintPure, Category = "DM|Match|Players")
    int32 GetCurrentPlayerStateCount() const;

    /**
     * 현재 Phase의 남은 시간을 서버 시간 기준으로 계산한다.
     *
     * 종료 시각이 없는 Phase에서는 0.0을 반환한다.
     */
    UFUNCTION(BlueprintPure, Category = "DM|Match|Time")
    double GetRemainingPhaseTime() const;

    /** Playing Phase일 때만 정규 경기 남은 시간을 반환한다. */
    UFUNCTION(BlueprintPure, Category = "DM|Match|Time")
    double GetRemainingRegularTime() const;

    /** Overtime Phase일 때만 연장전 남은 시간을 반환한다. */
    UFUNCTION(BlueprintPure, Category = "DM|Match|Time")
    double GetRemainingOvertimeTime() const;

    

    FDMOnMatchPhaseStateChanged& OnPhaseStateChanged()
    {
        return PhaseStateChangedDelegate;
    }

    FDMOnStartingPlayerCountChanged&
        OnStartingPlayerCountChanged()
    {
        return StartingPlayerCountChangedDelegate;
    }

    FDMOnMatchEndReasonChanged& OnMatchEndReasonChanged()
    {
        return MatchEndReasonChangedDelegate;
    }

private:
    /**
     * 현재 Phase와 해당 Phase 종료 서버 시각.
     *
     * 두 값을 하나의 RepNotify 단위로 복제한다.
     */
    UPROPERTY(
        VisibleInstanceOnly,
        ReplicatedUsing = OnRep_PhaseState,
        BlueprintReadOnly,
        Category = "DM|Match|State",
        meta = (AllowPrivateAccess = "true")
    )
    FDMMatchPhaseState PhaseState;

    /**
     * Match 시작 시점의 참가자 수.
     *
     * Match가 시작된 뒤 이탈자가 발생해도 변경하지 않는다.
     * 0은 아직 확정되지 않았다는 의미다.
     */
    UPROPERTY(
        VisibleInstanceOnly,
        ReplicatedUsing = OnRep_StartingPlayerCount,
        BlueprintReadOnly,
        Category = "DM|Match|Players",
        meta = (AllowPrivateAccess = "true")
    )
    int32 StartingPlayerCount = 0;

    /**
     * Match가 종료된 구체적인 원인.
     *
     * 종료 전에는 None이다.
     */
    UPROPERTY(
        VisibleInstanceOnly,
        ReplicatedUsing = OnRep_MatchEndReason,
        BlueprintReadOnly,
        Category = "DM|Match|Result",
        meta = (AllowPrivateAccess = "true")
    )
    EDMMatchEndReason MatchEndReason =
        EDMMatchEndReason::None;

    /**
     * 공개 Phase 상태를 변경한다.
     *
     * 서버에서만 호출해야 한다.
     * 합법적인 Phase 전환 여부는 MatchGameMode가 판정한다.
     */
    void SetPhaseState(
        const FDMMatchPhaseState& NewPhaseState
    );

    /**
     * Match 시작 인원을 최초 1회 확정한다.
     *
     * 서버에서만 호출하며 허용 범위는 2~4명이다.
     */
    void SetStartingPlayerCount(
        int32 NewStartingPlayerCount
    );

    /**
     * Match 종료 원인을 최초 1회 확정한다.
     *
     * 서버에서만 호출하며 None은 설정할 수 없다.
     */
    void SetMatchEndReason(
        EDMMatchEndReason NewMatchEndReason
    );

    UFUNCTION()
    void OnRep_PhaseState();

    UFUNCTION()
    void OnRep_StartingPlayerCount();

    UFUNCTION()
    void OnRep_MatchEndReason();

    void HandlePhaseStateChanged();
    void HandleStartingPlayerCountChanged();
    void HandleMatchEndReasonChanged();

    FDMOnMatchPhaseStateChanged
        PhaseStateChangedDelegate;

    FDMOnStartingPlayerCountChanged
        StartingPlayerCountChangedDelegate;

    FDMOnMatchEndReasonChanged
        MatchEndReasonChangedDelegate;
};
