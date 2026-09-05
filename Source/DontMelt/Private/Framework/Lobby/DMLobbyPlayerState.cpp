// Fill out your copyright notice in the Description page of Project Settings.

#include "Framework/Lobby/DMLobbyPlayerState.h"

#include "Net/UnrealNetwork.h"

void ADMLobbyPlayerState::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ADMLobbyPlayerState, bIsReady);
    DOREPLIFETIME(ADMLobbyPlayerState, bIsHost);
}

void ADMLobbyPlayerState::SetReadyState(const bool bNewReady)
{
    if (!HasAuthority())
    {
        return;
    }

    if (bIsReady == bNewReady)
    {
        return;
    }

    bIsReady = bNewReady;
    HandleReadyStateChanged();
}

void ADMLobbyPlayerState::SetHostState(const bool bNewHost)
{
    if (!HasAuthority())
    {
        return;
    }

    if (bIsHost == bNewHost)
    {
        return;
    }

    bIsHost = bNewHost;
    HandleHostStateChanged();
}

void ADMLobbyPlayerState::OnRep_IsReady()
{
    HandleReadyStateChanged();
}

void ADMLobbyPlayerState::OnRep_IsHost()
{
    HandleHostStateChanged();
}

void ADMLobbyPlayerState::HandleReadyStateChanged()
{
    ReadyChangedDelegate.Broadcast(this, bIsReady);
}

void ADMLobbyPlayerState::HandleHostStateChanged()
{
    HostChangedDelegate.Broadcast(this, bIsHost);
}