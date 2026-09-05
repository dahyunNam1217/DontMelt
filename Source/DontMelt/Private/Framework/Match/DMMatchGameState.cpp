// Fill out your copyright notice in the Description page of Project Settings.


#include "Framework/Match/DMMatchGameState.h"

#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(
    LogDMMatchGameState,
    Log,
    All
);

void ADMMatchGameState::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(
        ADMMatchGameState,
        PhaseState
    );

    DOREPLIFETIME(
        ADMMatchGameState,
        StartingPlayerCount
    );

    DOREPLIFETIME(
        ADMMatchGameState,
        MatchEndReason
    );
}

int32 ADMMatchGameState::
GetCurrentPlayerStateCount() const
{
    return PlayerArray.Num();
}

double ADMMatchGameState::GetRemainingPhaseTime() const
{
    if (PhaseState.PhaseEndServerTime <= 0.0)
    {
        return 0.0;
    }

    const double RemainingTime =
        PhaseState.PhaseEndServerTime
        - GetServerWorldTimeSeconds();

    return FMath::Max(0.0, RemainingTime);
}

double ADMMatchGameState::
GetRemainingRegularTime() const
{
    if (
        PhaseState.CurrentPhase
        != EDMMatchPhase::Playing
        )
    {
        return 0.0;
    }

    return GetRemainingPhaseTime();
}

double ADMMatchGameState::
GetRemainingOvertimeTime() const
{
    if (
        PhaseState.CurrentPhase
        != EDMMatchPhase::Overtime
        )
    {
        return 0.0;
    }

    return GetRemainingPhaseTime();
}

void ADMMatchGameState::SetPhaseState(
    const FDMMatchPhaseState& NewPhaseState
)
{
    if (!HasAuthority())
    {
        return;
    }

    if (NewPhaseState.PhaseEndServerTime < 0.0)
    {
        UE_LOG(
            LogDMMatchGameState,
            Warning,
            TEXT(
                "SetPhaseState rejected: "
                "PhaseEndServerTime cannot be negative."
            )
        );

        return;
    }

    const bool bSamePhase =
        PhaseState.CurrentPhase
        == NewPhaseState.CurrentPhase;

    const bool bSameEndTime =
        FMath::IsNearlyEqual(
            PhaseState.PhaseEndServerTime,
            NewPhaseState.PhaseEndServerTime
        );

    if (bSamePhase && bSameEndTime)
    {
        return;
    }

    PhaseState = NewPhaseState;
    HandlePhaseStateChanged();
}

void ADMMatchGameState::SetStartingPlayerCount(
    const int32 NewStartingPlayerCount
)
{
    if (!HasAuthority())
    {
        return;
    }

    if (
        !FMath::IsWithinInclusive(
            NewStartingPlayerCount,
            2,
            4
        )
        )
    {
        UE_LOG(
            LogDMMatchGameState,
            Warning,
            TEXT(
                "SetStartingPlayerCount rejected: "
                "invalid count %d."
            ),
            NewStartingPlayerCount
        );

        return;
    }

    if (StartingPlayerCount == NewStartingPlayerCount)
    {
        return;
    }

    if (StartingPlayerCount != 0)
    {
        UE_LOG(
            LogDMMatchGameState,
            Warning,
            TEXT(
                "SetStartingPlayerCount rejected: "
                "count is already frozen. "
                "Current=%d, Requested=%d"
            ),
            StartingPlayerCount,
            NewStartingPlayerCount
        );

        return;
    }

    StartingPlayerCount = NewStartingPlayerCount;
    HandleStartingPlayerCountChanged();
}

void ADMMatchGameState::SetMatchEndReason(
    const EDMMatchEndReason NewMatchEndReason
)
{
    if (!HasAuthority())
    {
        return;
    }

    if (NewMatchEndReason == EDMMatchEndReason::None)
    {
        UE_LOG(
            LogDMMatchGameState,
            Warning,
            TEXT(
                "SetMatchEndReason rejected: "
                "None is not a final end reason."
            )
        );

        return;
    }

    if (MatchEndReason == NewMatchEndReason)
    {
        return;
    }

    if (MatchEndReason != EDMMatchEndReason::None)
    {
        UE_LOG(
            LogDMMatchGameState,
            Warning,
            TEXT(
                "SetMatchEndReason rejected: "
                "reason is already frozen."
            )
        );

        return;
    }

    MatchEndReason = NewMatchEndReason;
    HandleMatchEndReasonChanged();
}

void ADMMatchGameState::OnRep_PhaseState()
{
    HandlePhaseStateChanged();
}

void ADMMatchGameState::OnRep_StartingPlayerCount()
{
    HandleStartingPlayerCountChanged();
}

void ADMMatchGameState::OnRep_MatchEndReason()
{
    HandleMatchEndReasonChanged();
}

void ADMMatchGameState::HandlePhaseStateChanged()
{
    PhaseStateChangedDelegate.Broadcast(
        this,
        PhaseState
    );
}

void ADMMatchGameState::
HandleStartingPlayerCountChanged()
{
    StartingPlayerCountChangedDelegate.Broadcast(
        this,
        StartingPlayerCount
    );
}

void ADMMatchGameState::HandleMatchEndReasonChanged()
{
    MatchEndReasonChangedDelegate.Broadcast(
        this,
        MatchEndReason
    );
}