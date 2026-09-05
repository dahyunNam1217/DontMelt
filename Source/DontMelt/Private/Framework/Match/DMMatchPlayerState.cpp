// Fill out your copyright notice in the Description page of Project Settings.


#include "Framework/Match/DMMatchPlayerState.h"

#include "Net/UnrealNetwork.h"

void ADMMatchPlayerState::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION(
        ADMMatchPlayerState,
        LifeState,
        COND_OwnerOnly
    );

    DOREPLIFETIME_CONDITION(
        ADMMatchPlayerState,
        BaseSlot,
        COND_OwnerOnly
    );
}

void ADMMatchPlayerState::SetLifeState(
    const EDMPlayerLifeState NewLifeState
)
{
    if (!HasAuthority())
    {
        return;
    }

    if (LifeState == NewLifeState)
    {
        return;
    }

    LifeState = NewLifeState;
    HandleLifeStateChanged();
}

void ADMMatchPlayerState::SetBaseSlot(
    const int32 NewBaseSlot
)
{
    if (!HasAuthority())
    {
        return;
    }

    const bool bIsValidBaseSlot =
        NewBaseSlot == INDEX_NONE
        || FMath::IsWithinInclusive(NewBaseSlot, 0, 3);

    if (!bIsValidBaseSlot)
    {
        return;
    }

    if (BaseSlot == NewBaseSlot)
    {
        return;
    }

    BaseSlot = NewBaseSlot;
    HandleBaseSlotChanged();
}

void ADMMatchPlayerState::OnRep_LifeState()
{
    HandleLifeStateChanged();
}

void ADMMatchPlayerState::OnRep_BaseSlot()
{
    HandleBaseSlotChanged();
}

void ADMMatchPlayerState::HandleLifeStateChanged()
{
    LifeStateChangedDelegate.Broadcast(
        this,
        LifeState
    );
}

void ADMMatchPlayerState::HandleBaseSlotChanged()
{
    BaseSlotChangedDelegate.Broadcast(
        this,
        BaseSlot
    );
}